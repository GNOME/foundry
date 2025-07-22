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

#include "foundry-source-buffer-private.h"
#include "foundry-source-completion-provider-private.h"
#include "foundry-source-hover-provider-private.h"
#include "foundry-source-indenter-private.h"
#include "foundry-source-view.h"
#include "foundry-source-view-addin-private.h"

struct _FoundrySourceView
{
  GtkSourceView        parent_instance;

  FoundryTextDocument *document;
  FoundryExtensionSet *completion_addins;
  FoundryExtensionSet *hover_addins;
  PeasExtensionSet    *view_addins;
  FoundryExtension    *indenter_addins;
  FoundryExtension    *rename_addins;
};

enum {
  PROP_0,
  PROP_DOCUMENT,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundrySourceView, foundry_source_view, GTK_SOURCE_TYPE_VIEW)

static GParamSpec *properties[N_PROPS];

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
foundry_source_view_dispose (GObject *object)
{
  FoundrySourceView *self = (FoundrySourceView *)object;

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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_source_view_class_init (FoundrySourceViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_source_view_dispose;
  object_class->get_property = foundry_source_view_get_property;
  object_class->set_property = foundry_source_view_set_property;

  properties[PROP_DOCUMENT] =
    g_param_spec_object ("document", NULL, NULL,
                         FOUNDRY_TYPE_TEXT_DOCUMENT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_source_view_init (FoundrySourceView *self)
{
}

GtkWidget *
foundry_source_view_new (FoundryTextDocument *document)
{
  g_autoptr(FoundryTextBuffer) buffer = NULL;
  g_autoptr(FoundryContext) context = NULL;
  GtkSourceLanguage *language;
  FoundrySourceView *self;
  const char *language_id;

  g_return_val_if_fail (FOUNDRY_IS_TEXT_DOCUMENT (document), NULL);

  buffer = foundry_text_document_dup_buffer (document);

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

  /* Setup completion providers */
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
