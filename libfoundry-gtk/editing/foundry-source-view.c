/* foundry-source-view.c
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <libpeas.h>

#include "egg-joined-menu.h"

#include "foundry-pango.h"
#include "foundry-source-buffer-private.h"
#include "foundry-changes-gutter-renderer.h"
#include "foundry-source-completion-provider-private.h"
#include "foundry-source-hover-provider-private.h"
#include "foundry-source-indenter-private.h"
#include "foundry-source-view.h"
#include "foundry-source-view-addin-private.h"

struct _FoundrySourceView
{
  GtkSourceView                 parent_instance;

  FoundryTextDocument          *document;
  FoundryExtensionSet          *completion_addins;
  FoundryExtensionSet          *hover_addins;
  PeasExtensionSet             *view_addins;
  FoundryExtension             *indenter_addins;
  FoundryExtension             *rename_addins;

  GtkSourceGutterRenderer      *changes_gutter_renderer;

  GBindingGroup                *settings_bindings;
  FoundryTextSettings          *settings;

  EggJoinedMenu                *extra_menu;

  GtkEventController           *vim_key_controller;
  GtkIMContext                 *vim_im_context;

  GtkCssProvider               *css;
  PangoFontDescription         *font;

  double                        line_height;

  guint                         enable_vim : 1;
  guint                         show_line_changes : 1;
};

enum {
  PROP_0,
  PROP_DOCUMENT,
  PROP_ENABLE_VIM,
  PROP_FONT,
  PROP_LINE_HEIGHT,
  PROP_SHOW_LINE_CHANGES,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundrySourceView, foundry_source_view, GTK_SOURCE_TYPE_VIEW)

static GParamSpec *properties[N_PROPS];
static GSettings *editor_settings;

static void
foundry_source_view_update_font (FoundrySourceView *self)
{
  g_autoptr(PangoFontDescription) font = NULL;
  gboolean use_custom_font;

  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));

  if ((use_custom_font = g_settings_get_boolean (editor_settings, "use-custom-font")))
    {
      g_autofree char *custom_font = g_settings_get_string (editor_settings, "custom-font");

      font = pango_font_description_from_string (custom_font);
    }

  foundry_source_view_set_font (self, font);
}

static void
foundry_source_view_update_css (FoundrySourceView *self)
{
  g_autoptr(GString) gstr = NULL;
  g_autofree char *font = NULL;
  char line_height_str[32];

  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));

  gstr = g_string_new (NULL);

  if (self->font != NULL &&
      (font = foundry_font_description_to_css (self->font)))
    g_string_append_printf (gstr, "textview { %s }\n", font);

  g_ascii_dtostr (line_height_str, sizeof line_height_str, self->line_height);
  line_height_str[8] = 0;

  g_string_append_printf (gstr, "textview { line-height: %s; }\n", line_height_str);

  gtk_css_provider_load_from_string (self->css, gstr->str);
}

static FoundrySourceBuffer *
foundry_source_view_dup_buffer (FoundrySourceView *self)
{
  GtkTextBuffer *buffer;

  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  if (FOUNDRY_IS_SOURCE_BUFFER (buffer))
    return g_object_ref (FOUNDRY_SOURCE_BUFFER (buffer));

  return NULL;
}

static void
foundry_source_view_addin_added_cb (PeasExtensionSet *set,
                                    PeasPluginInfo   *plugin_info,
                                    GObject          *extension,
                                    gpointer          user_data)
{
  FoundrySourceViewAddin *addin = (FoundrySourceViewAddin *)extension;
  FoundrySourceView *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_SOURCE_VIEW_ADDIN (addin));
  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));

  g_debug ("Add view addin `%s`", G_OBJECT_TYPE_NAME (addin));

  foundry_source_view_addin_load (addin, self);
}

static void
foundry_source_view_addin_removed_cb (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      GObject          *extension,
                                      gpointer          user_data)
{
  FoundrySourceViewAddin *addin = (FoundrySourceViewAddin *)extension;
  FoundrySourceView *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_SOURCE_VIEW_ADDIN (addin));
  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));

  g_debug ("Remove view addin `%s`", G_OBJECT_TYPE_NAME (addin));

  foundry_source_view_addin_unload (addin);
}

static void
foundry_source_view_completion_provider_added_cb (FoundryExtensionSet *set,
                                                  PeasPluginInfo      *plugin_info,
                                                  GObject             *extension,
                                                  gpointer             user_data)
{
  FoundryCompletionProvider *provider = (FoundryCompletionProvider *)extension;
  g_autoptr(GtkSourceCompletionProvider) wrapper = NULL;
  GtkSourceCompletion *completion;
  FoundrySourceView *self = user_data;

  g_assert (FOUNDRY_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_COMPLETION_PROVIDER (provider));
  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));

  g_debug ("Add completion provider `%s`", G_OBJECT_TYPE_NAME (provider));

  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));
  wrapper = foundry_source_completion_provider_new (provider);

  g_object_set_data_full (G_OBJECT (provider),
                          "GTK_SOURCE_COMPLETION_PROVIDER",
                          g_object_ref (wrapper),
                          g_object_unref);

  gtk_source_completion_add_provider (completion, wrapper);
}

static void
foundry_source_view_completion_provider_removed_cb (FoundryExtensionSet *set,
                                                    PeasPluginInfo      *plugin_info,
                                                    GObject             *extension,
                                                    gpointer             user_data)
{
  FoundryCompletionProvider *provider = (FoundryCompletionProvider *)extension;
  GtkSourceCompletionProvider *wrapper;
  GtkSourceCompletion *completion;
  FoundrySourceView *self = user_data;

  g_assert (FOUNDRY_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_COMPLETION_PROVIDER (provider));
  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));

  g_debug ("Remove completion provider `%s`", G_OBJECT_TYPE_NAME (provider));

  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));

  if ((wrapper = g_object_get_data (G_OBJECT (provider), "GTK_SOURCE_COMPLETION_PROVIDER")))
    {
      gtk_source_completion_remove_provider (completion, wrapper);
      g_object_set_data (G_OBJECT (provider), "GTK_SOURCE_COMPLETION_PROVIDER", NULL);
    }
}

static void
foundry_source_view_hover_provider_added_cb (FoundryExtensionSet *set,
                                             PeasPluginInfo      *plugin_info,
                                             GObject             *extension,
                                             gpointer             user_data)
{
  FoundryHoverProvider *provider = (FoundryHoverProvider *)extension;
  g_autoptr(GtkSourceHoverProvider) wrapper = NULL;
  FoundrySourceView *self = user_data;
  GtkSourceHover *hover;

  g_assert (FOUNDRY_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_HOVER_PROVIDER (provider));
  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));

  g_debug ("Add hover provider `%s`", G_OBJECT_TYPE_NAME (provider));

  hover = gtk_source_view_get_hover (GTK_SOURCE_VIEW (self));
  wrapper = foundry_source_hover_provider_new (provider);

  g_object_set_data_full (G_OBJECT (provider),
                          "GTK_SOURCE_HOVER_PROVIDER",
                          g_object_ref (wrapper),
                          g_object_unref);

  gtk_source_hover_add_provider (hover, wrapper);
}

static void
foundry_source_view_hover_provider_removed_cb (FoundryExtensionSet *set,
                                               PeasPluginInfo      *plugin_info,
                                               GObject             *extension,
                                               gpointer             user_data)
{
  FoundryHoverProvider *provider = (FoundryHoverProvider *)extension;
  GtkSourceHoverProvider *wrapper;
  FoundrySourceView *self = user_data;
  GtkSourceHover *hover;

  g_assert (FOUNDRY_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_HOVER_PROVIDER (provider));
  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));

  g_debug ("Remove hover provider `%s`", G_OBJECT_TYPE_NAME (provider));

  hover = gtk_source_view_get_hover (GTK_SOURCE_VIEW (self));

  if ((wrapper = g_object_get_data (G_OBJECT (provider), "GTK_SOURCE_HOVER_PROVIDER")))
    {
      gtk_source_hover_remove_provider (hover, wrapper);
      g_object_set_data (G_OBJECT (provider), "GTK_SOURCE_HOVER_PROVIDER", NULL);
    }
}

static gboolean
_gtk_source_language_to_id_mapping (GBinding     *binding,
                                    const GValue *from,
                                    GValue       *to,
                                    gpointer      user_data)
{
  GtkSourceLanguage *language = g_value_get_object (from);
  if (language != NULL)
    g_value_set_string (to, gtk_source_language_get_id (language));
  return TRUE;
}

static gboolean
type_formatter_to_indenter (GBinding     *binding,
                            const GValue *from,
                            GValue       *to,
                            gpointer      user_data)
{
  FoundryOnTypeFormatter *formatter = g_value_get_object (from);
  if (formatter != NULL)
    g_value_take_object (to, foundry_source_indenter_new (formatter));
  return TRUE;
}

static void
foundry_source_view_set_settings (FoundrySourceView   *self,
                                  FoundryTextSettings *settings)
{
  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));
  g_assert (!settings || FOUNDRY_IS_TEXT_SETTINGS (settings));

  if (g_set_object (&self->settings, settings))
    g_binding_group_set_source (self->settings_bindings, settings);
}

static DexFuture *
foundry_source_view_apply_settings_cb (DexFuture *completed,
                                       gpointer   user_data)
{
  FoundrySourceView *self = user_data;
  g_autoptr(FoundryTextSettings) settings = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));

  if ((settings = dex_await_object (dex_ref (completed), &error)))
    foundry_source_view_set_settings (self, settings);

  return dex_ref (completed);
}

static void
foundry_source_view_constructed (GObject *object)
{
  FoundrySourceView *self = FOUNDRY_SOURCE_VIEW (object);
  g_autoptr(FoundryTextBuffer) buffer = NULL;

  G_OBJECT_CLASS (foundry_source_view_parent_class)->constructed (object);

  g_assert (FOUNDRY_IS_TEXT_DOCUMENT (self->document));
  buffer = foundry_text_document_dup_buffer (self->document);
  g_assert (FOUNDRY_IS_SOURCE_BUFFER (buffer));

  self->settings_bindings = g_binding_group_new ();
  g_binding_group_bind (self->settings_bindings, "auto-indent",
                        self, "auto-indent",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_bindings, "enable-snippets",
                        self, "enable-snippets",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_bindings, "highlight-current-line",
                        self, "highlight-current-line",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_bindings, "highlight-matching-brackets",
                        buffer, "highlight-matching-brackets",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_bindings, "implicit-trailing-newline",
                        buffer, "implicit-trailing-newline",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_bindings, "indent-on-tab",
                        self, "indent-on-tab",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_bindings, "indent-width",
                        self, "indent-width",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_bindings, "tab-width",
                        self, "tab-width",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_bindings, "insert-spaces-instead-of-tabs",
                        self, "insert-spaces-instead-of-tabs",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_bindings, "right-margin-position",
                        self, "right-margin-position",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_bindings, "show-line-changes",
                        self, "show-line-changes",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_bindings, "show-line-numbers",
                        self, "show-line-numbers",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_bindings, "show-right-margin",
                        self, "show-right-margin",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_bindings, "smart-backspace",
                        self, "smart-backspace",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_bindings, "smart-home-end",
                        self, "smart-home-end",
                        G_BINDING_SYNC_CREATE);
}

static void
foundry_source_view_dispose (GObject *object)
{
  FoundrySourceView *self = (FoundrySourceView *)object;

  g_clear_object (&self->settings_bindings);
  g_clear_object (&self->settings);
  g_clear_object (&self->vim_im_context);
  g_clear_object (&self->vim_key_controller);
  g_clear_object (&self->view_addins);
  g_clear_object (&self->completion_addins);
  g_clear_object (&self->hover_addins);
  g_clear_object (&self->indenter_addins);
  g_clear_object (&self->rename_addins);
  g_clear_object (&self->document);

  G_OBJECT_CLASS (foundry_source_view_parent_class)->dispose (object);
}

static void
foundry_source_view_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundrySourceView *self = FOUNDRY_SOURCE_VIEW (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      g_value_take_object (value, foundry_source_view_dup_document (self));
      break;

    case PROP_ENABLE_VIM:
      g_value_set_boolean (value, foundry_source_view_get_enable_vim (self));
      break;

    case PROP_FONT:
      g_value_take_boxed (value, foundry_source_view_dup_font (self));
      break;

    case PROP_LINE_HEIGHT:
      g_value_set_double (value, foundry_source_view_get_line_height (self));
      break;

    case PROP_SHOW_LINE_CHANGES:
      g_value_set_boolean (value, foundry_source_view_get_show_line_changes (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_source_view_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  FoundrySourceView *self = FOUNDRY_SOURCE_VIEW (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      self->document = g_value_dup_object (value);
      break;

    case PROP_ENABLE_VIM:
      foundry_source_view_set_enable_vim (self, g_value_get_boolean (value));
      break;

    case PROP_FONT:
      foundry_source_view_set_font (self, g_value_get_boxed (value));
      break;

    case PROP_LINE_HEIGHT:
      foundry_source_view_set_line_height (self, g_value_get_double (value));
      break;

    case PROP_SHOW_LINE_CHANGES:
      foundry_source_view_set_show_line_changes (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_source_view_class_init (FoundrySourceViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = foundry_source_view_constructed;
  object_class->dispose = foundry_source_view_dispose;
  object_class->get_property = foundry_source_view_get_property;
  object_class->set_property = foundry_source_view_set_property;

  properties[PROP_DOCUMENT] =
    g_param_spec_object ("document", NULL, NULL,
                         FOUNDRY_TYPE_TEXT_DOCUMENT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ENABLE_VIM] =
    g_param_spec_boolean ("enable-vim", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_FONT] =
    g_param_spec_boxed ("font", NULL, NULL,
                        PANGO_TYPE_FONT_DESCRIPTION,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_LINE_HEIGHT] =
    g_param_spec_double ("line-height", NULL, NULL,
                         .5, 5., 1.,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SHOW_LINE_CHANGES] =
    g_param_spec_boolean ("show-line-changes", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_source_view_init (FoundrySourceView *self)
{
  GtkSourceGutter *gutter;

  if (editor_settings == NULL)
    editor_settings = g_settings_new ("app.devsuite.foundry.editor");

  gtk_text_view_set_monospace (GTK_TEXT_VIEW (self), TRUE);

  self->css = gtk_css_provider_new ();
  self->line_height = 1.0;
  self->extra_menu = egg_joined_menu_new ();

  gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (self), GTK_TEXT_WINDOW_LEFT);

  self->changes_gutter_renderer = foundry_changes_gutter_renderer_new ();
  gtk_widget_set_visible (GTK_WIDGET (self->changes_gutter_renderer), self->show_line_changes);
  gtk_source_gutter_insert (gutter, self->changes_gutter_renderer, 100);

  gtk_text_view_set_extra_menu (GTK_TEXT_VIEW (self),
                                G_MENU_MODEL (self->extra_menu));

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS {
    GtkStyleContext *style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
    gtk_style_context_add_provider (style_context,
                                    GTK_STYLE_PROVIDER (self->css),
                                    G_MAXINT-1);
  } G_GNUC_END_IGNORE_DEPRECATIONS

  g_signal_connect_object (editor_settings,
                           "changed::custom-font",
                           G_CALLBACK (foundry_source_view_update_font),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (editor_settings,
                           "changed::use-custom-font",
                           G_CALLBACK (foundry_source_view_update_font),
                           self,
                           G_CONNECT_SWAPPED);

  foundry_source_view_update_font (self);
  foundry_source_view_update_css (self);
}

GtkWidget *
foundry_source_view_new (FoundryTextDocument *document)
{
  g_autoptr(FoundryTextBuffer) buffer = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(DexFuture) settings = NULL;
  GtkSourceLanguage *language;
  FoundrySourceView *self;
  const char *language_id;

  g_return_val_if_fail (FOUNDRY_IS_TEXT_DOCUMENT (document), NULL);

  buffer = foundry_text_document_dup_buffer (document);
  settings = foundry_text_document_load_settings (document);

  g_return_val_if_fail (FOUNDRY_IS_SOURCE_BUFFER (buffer), NULL);

  self = g_object_new (FOUNDRY_TYPE_SOURCE_VIEW,
                       "document", document,
                       "buffer", buffer,
                       NULL);

  context = foundry_source_buffer_dup_context (FOUNDRY_SOURCE_BUFFER (buffer));
  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
  language_id = language ? gtk_source_language_get_id (language) : NULL;

  self->view_addins = peas_extension_set_new (peas_engine_get_default (),
                                              FOUNDRY_TYPE_SOURCE_VIEW_ADDIN,
                                              NULL);
  g_signal_connect_object (self->view_addins,
                           "extension-added",
                           G_CALLBACK (foundry_source_view_addin_added_cb),
                           self,
                           0);
  g_signal_connect_object (self->view_addins,
                           "extension-removed",
                           G_CALLBACK (foundry_source_view_addin_removed_cb),
                           self,
                           0);
  peas_extension_set_foreach (self->view_addins,
                              foundry_source_view_addin_added_cb,
                              self);

  /* Setup completion providers */
  self->completion_addins = foundry_extension_set_new (context,
                                                       peas_engine_get_default (),
                                                       FOUNDRY_TYPE_COMPLETION_PROVIDER,
                                                       "Completion-Provider-Languages",
                                                       language_id,
                                                       "document", self->document,
                                                       NULL);
  g_object_bind_property_full (buffer, "language",
                               self->completion_addins, "value",
                               G_BINDING_SYNC_CREATE,
                               _gtk_source_language_to_id_mapping, NULL, NULL, NULL);
  g_signal_connect_object (self->completion_addins,
                           "extension-added",
                           G_CALLBACK (foundry_source_view_completion_provider_added_cb),
                           self,
                           0);
  g_signal_connect_object (self->completion_addins,
                           "extension-removed",
                           G_CALLBACK (foundry_source_view_completion_provider_removed_cb),
                           self,
                           0);
  foundry_extension_set_foreach (self->completion_addins,
                                 foundry_source_view_completion_provider_added_cb,
                                 self);

  /* Setup hover providers */
  self->hover_addins = foundry_extension_set_new (context,
                                                  peas_engine_get_default (),
                                                  FOUNDRY_TYPE_HOVER_PROVIDER,
                                                  "Hover-Provider-Languages",
                                                  language_id,
                                                  "document", self->document,
                                                  NULL);
  g_object_bind_property_full (buffer, "language",
                               self->hover_addins, "value",
                               G_BINDING_SYNC_CREATE,
                               _gtk_source_language_to_id_mapping, NULL, NULL, NULL);
  g_signal_connect_object (self->hover_addins,
                           "extension-added",
                           G_CALLBACK (foundry_source_view_hover_provider_added_cb),
                           self,
                           0);
  g_signal_connect_object (self->hover_addins,
                           "extension-removed",
                           G_CALLBACK (foundry_source_view_hover_provider_removed_cb),
                           self,
                           0);
  foundry_extension_set_foreach (self->hover_addins,
                                 foundry_source_view_hover_provider_added_cb,
                                 self);

  /* Setup indenters */
  self->indenter_addins = foundry_extension_new (context,
                                                 peas_engine_get_default (),
                                                 FOUNDRY_TYPE_ON_TYPE_FORMATTER,
                                                 "Indenter-Languages", language_id);
  g_object_bind_property_full (buffer, "language",
                               self->indenter_addins, "value",
                               G_BINDING_SYNC_CREATE,
                               _gtk_source_language_to_id_mapping, NULL, NULL, NULL);
  g_object_bind_property_full (self->indenter_addins, "extension",
                               self, "indenter",
                               G_BINDING_SYNC_CREATE,
                               type_formatter_to_indenter, NULL, NULL, NULL);

  /* Setup Rename Provider */
  self->rename_addins = foundry_extension_new (context,
                                               peas_engine_get_default (),
                                               FOUNDRY_TYPE_RENAME_PROVIDER,
                                               "Rename-Provider-Languages",
                                               NULL);
  g_object_bind_property_full (buffer, "language",
                               self->rename_addins, "value",
                               G_BINDING_SYNC_CREATE,
                               _gtk_source_language_to_id_mapping, NULL, NULL, NULL);

  /* When settings are loaded, apply them */
  dex_future_disown (dex_future_then (dex_ref (settings),
                                      foundry_source_view_apply_settings_cb,
                                      g_object_ref (self),
                                      g_object_unref));

  return GTK_WIDGET (self);
}

/**
 * foundry_source_view_dup_document:
 * @self: a [class@FoundryGtk.SourceView]
 *
 * Returns: (transfer full) (nullable):
 */
FoundryTextDocument *
foundry_source_view_dup_document (FoundrySourceView *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SOURCE_VIEW (self), NULL);

  return self->document ? g_object_ref (self->document) : NULL;
}

/**
 * foundry_source_view_rename:
 * @self: a [class@FoundryGtk.SourceView]
 * @iter: the location of the semantic word to rename
 * @new_name: the name for the replacement
 *
 * Uses the active [class@Foundry.RenameProvider] to semantically rename the
 * word found at @iter with @new_name.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.TextEdit].
 */
DexFuture *
foundry_source_view_rename (FoundrySourceView *self,
                            const GtkTextIter *iter,
                            const char        *new_name)
{
  g_autoptr(FoundrySourceBuffer) buffer = NULL;
  FoundryRenameProvider *provider;
  FoundryTextIter real;

  dex_return_error_if_fail (FOUNDRY_IS_SOURCE_VIEW (self));

  if (!self->rename_addins || !(provider = foundry_extension_get_extension (self->rename_addins)))
    return foundry_future_new_not_supported ();

  buffer = foundry_source_view_dup_buffer (self);

  _foundry_source_buffer_init_iter (buffer, &real, iter);

  return foundry_rename_provider_rename (provider, &real, new_name);
}

/**
 * foundry_source_view_dup_font:
 * @self: a [class@FoundryGtk.SourceView]
 *
 * Returns: (transfer full):
 */
PangoFontDescription *
foundry_source_view_dup_font (FoundrySourceView *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SOURCE_VIEW (self), NULL);

  return pango_font_description_copy (self->font);
}

void
foundry_source_view_set_font (FoundrySourceView          *self,
                              const PangoFontDescription *font)
{
  g_autoptr(PangoFontDescription) copy = NULL;

  g_return_if_fail (FOUNDRY_IS_SOURCE_VIEW (self));

  if (self->font == font)
    return;

  if (font != NULL &&
      self->font != NULL &&
      pango_font_description_equal (font, self->font))
    return;

  if (font != NULL)
    copy = pango_font_description_copy (font);

  g_clear_pointer (&self->font, pango_font_description_free);
  self->font = g_steal_pointer (&copy);

  foundry_source_view_update_css (self);
}

double
foundry_source_view_get_line_height (FoundrySourceView *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SOURCE_VIEW (self), 1.);

  return self->line_height;
}

void
foundry_source_view_set_line_height (FoundrySourceView *self,
                                     double             line_height)
{
  g_return_if_fail (FOUNDRY_IS_SOURCE_VIEW (self));

  line_height = CLAMP (line_height, .5, 5.);

  if (line_height != self->line_height)
    {
      self->line_height = line_height;
      foundry_source_view_update_css (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LINE_HEIGHT]);
    }
}

gboolean
foundry_source_view_get_enable_vim (FoundrySourceView *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SOURCE_VIEW (self), FALSE);

  return self->enable_vim;
}

void
foundry_source_view_set_enable_vim (FoundrySourceView *self,
                                    gboolean           enable_vim)
{
  g_return_if_fail (FOUNDRY_IS_SOURCE_VIEW (self));

  enable_vim = !!enable_vim;

  if (self->enable_vim == enable_vim)
    return;

  self->enable_vim = enable_vim;

  if (self->enable_vim)
    {
      self->vim_key_controller = gtk_event_controller_key_new ();
      self->vim_im_context = gtk_source_vim_im_context_new ();

      gtk_im_context_set_client_widget (self->vim_im_context, GTK_WIDGET (self));

      gtk_event_controller_set_propagation_phase (self->vim_key_controller, GTK_PHASE_CAPTURE);
      gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (self->vim_key_controller),
                                               self->vim_im_context);

      gtk_widget_add_controller (GTK_WIDGET (self),
                                 g_object_ref (self->vim_key_controller));
    }
  else
    {
      gtk_widget_remove_controller (GTK_WIDGET (self),
                                    self->vim_key_controller);

      g_clear_object (&self->vim_key_controller);
      g_clear_object (&self->vim_im_context);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLE_VIM]);
}

/**
 * foundry_source_view_dup_context:
 * @self: a [class@FoundryGtk.SourceView]
 *
 * Returns: (transfer full) (nullable): the [class@Foundry.Context] of the document
 */
FoundryContext *
foundry_source_view_dup_context (FoundrySourceView *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SOURCE_VIEW (self), NULL);

  if (self->document != NULL)
    return foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self->document));

  return NULL;
}

void
foundry_source_view_append_menu (FoundrySourceView *self,
                                 GMenuModel        *menu)
{
  g_return_if_fail (FOUNDRY_IS_SOURCE_VIEW (self));
  g_return_if_fail (G_IS_MENU_MODEL (menu));

  egg_joined_menu_append_menu (self->extra_menu, menu);
}

void
foundry_source_view_remove_menu (FoundrySourceView *self,
                                 GMenuModel        *menu)
{
  g_return_if_fail (FOUNDRY_IS_SOURCE_VIEW (self));
  g_return_if_fail (G_IS_MENU_MODEL (menu));

  egg_joined_menu_remove_menu (self->extra_menu, menu);
}

gboolean
foundry_source_view_get_show_line_changes (FoundrySourceView *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SOURCE_VIEW (self), FALSE);

  return self->show_line_changes;
}

void
foundry_source_view_set_show_line_changes (FoundrySourceView *self,
                                           gboolean           show_line_changes)
{
  g_return_if_fail (FOUNDRY_IS_SOURCE_VIEW (self));

  show_line_changes = !!show_line_changes;

  if (show_line_changes != self->show_line_changes)
    {
      self->show_line_changes = show_line_changes;
      gtk_widget_set_visible (GTK_WIDGET (self->changes_gutter_renderer), show_line_changes);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_LINE_CHANGES]);
    }
}
