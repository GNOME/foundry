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

#include "foundry-source-buffer.h"
#include "foundry-source-completion-provider-private.h"
#include "foundry-source-hover-provider-private.h"
#include "foundry-source-view.h"

typedef struct
{
  FoundrySourceBuffer *buffer;
  FoundryExtensionSet *completion_addins;
  FoundryExtensionSet *hover_addins;
} FoundrySourceViewPrivate;

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (FoundrySourceView, foundry_source_view, GTK_SOURCE_TYPE_VIEW)

static GParamSpec *properties[N_PROPS];

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

  g_debug ("Add completion provider \"%s\"", G_OBJECT_TYPE_NAME (provider));

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

  g_debug ("Remove completion provider \"%s\"", G_OBJECT_TYPE_NAME (provider));

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
  FoundrySourceHoverProvider *provider = (FoundrySourceHoverProvider *)extension;
  FoundrySourceView *self = user_data;
  GtkSourceHover *hover;

  g_assert (FOUNDRY_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_SOURCE_HOVER_PROVIDER (provider));
  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));

  g_debug ("Add hover provider `%s`", G_OBJECT_TYPE_NAME (provider));

  hover = gtk_source_view_get_hover (GTK_SOURCE_VIEW (self));
  gtk_source_hover_add_provider (hover, GTK_SOURCE_HOVER_PROVIDER (provider));
}

static void
foundry_source_view_hover_provider_removed_cb (FoundryExtensionSet *set,
                                               PeasPluginInfo      *plugin_info,
                                               GObject             *extension,
                                               gpointer             user_data)
{
  FoundrySourceHoverProvider *provider = (FoundrySourceHoverProvider *)extension;
  FoundrySourceView *self = user_data;
  GtkSourceHover *hover;

  g_assert (FOUNDRY_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_SOURCE_HOVER_PROVIDER (provider));
  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));

  g_debug ("Remove hover provider `%s`", G_OBJECT_TYPE_NAME (provider));

  hover = gtk_source_view_get_hover (GTK_SOURCE_VIEW (self));
  gtk_source_hover_remove_provider (hover, GTK_SOURCE_HOVER_PROVIDER (provider));
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

static void
foundry_source_view_connect_buffer (FoundrySourceView *self)
{
  FoundrySourceViewPrivate *priv = foundry_source_view_get_instance_private (self);
  g_autoptr(FoundryContext) context = NULL;
  GtkSourceLanguage *language;
  const char *language_id;

  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));
  g_assert (priv->buffer == NULL || FOUNDRY_IS_SOURCE_BUFFER (priv->buffer));

  if (priv->buffer == NULL)
    return;

  context = foundry_source_buffer_dup_context (FOUNDRY_SOURCE_BUFFER (priv->buffer));
  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (priv->buffer));
  language_id = language ? gtk_source_language_get_id (language) : NULL;

  /* Setup completion providers */
  priv->completion_addins = foundry_extension_set_new (context,
                                                       peas_engine_get_default (),
                                                       FOUNDRY_TYPE_COMPLETION_PROVIDER,
                                                       "Completion-Provider-Languages",
                                                       language_id);
  g_object_bind_property_full (priv->buffer, "language",
                               priv->completion_addins, "value",
                               G_BINDING_SYNC_CREATE,
                               _gtk_source_language_to_id_mapping, NULL, NULL, NULL);
  g_signal_connect_object (priv->completion_addins,
                           "extension-added",
                           G_CALLBACK (foundry_source_view_completion_provider_added_cb),
                           self,
                           0);
  g_signal_connect_object (priv->completion_addins,
                           "extension-removed",
                           G_CALLBACK (foundry_source_view_completion_provider_removed_cb),
                           self,
                           0);
  foundry_extension_set_foreach (priv->completion_addins,
                                 foundry_source_view_completion_provider_added_cb,
                                 self);

  /* Setup completion providers */
  priv->hover_addins = foundry_extension_set_new (context,
                                                  peas_engine_get_default (),
                                                  FOUNDRY_TYPE_HOVER_PROVIDER,
                                                  "Hover-Provider-Languages",
                                                  language_id);
  g_object_bind_property_full (priv->buffer, "language",
                               priv->hover_addins, "value",
                               G_BINDING_SYNC_CREATE,
                               _gtk_source_language_to_id_mapping, NULL, NULL, NULL);
  g_signal_connect_object (priv->hover_addins,
                           "extension-added",
                           G_CALLBACK (foundry_source_view_hover_provider_added_cb),
                           self,
                           0);
  g_signal_connect_object (priv->hover_addins,
                           "extension-removed",
                           G_CALLBACK (foundry_source_view_hover_provider_removed_cb),
                           self,
                           0);
  foundry_extension_set_foreach (priv->hover_addins,
                                 foundry_source_view_hover_provider_added_cb,
                                 self);
}

static void
foundry_source_view_disconnect_buffer (FoundrySourceView *self)
{
  FoundrySourceViewPrivate *priv = foundry_source_view_get_instance_private (self);

  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));

  if (priv->buffer == NULL)
    return;

  g_clear_object (&priv->completion_addins);
  g_clear_object (&priv->hover_addins);
  g_clear_object (&priv->buffer);
}

static void
foundry_source_view_notify_buffer (FoundrySourceView *self)
{
  FoundrySourceViewPrivate *priv = foundry_source_view_get_instance_private (self);
  GtkTextBuffer *text_buffer;

  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));

  text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  if (!FOUNDRY_IS_SOURCE_BUFFER (text_buffer))
    text_buffer = NULL;

  if (text_buffer == GTK_TEXT_BUFFER (priv->buffer))
    return;

  if (priv->buffer != NULL)
    foundry_source_view_disconnect_buffer (self);

  g_set_object (&priv->buffer, FOUNDRY_SOURCE_BUFFER (text_buffer));

  if (priv->buffer)
    foundry_source_view_connect_buffer (self);
}

static void
foundry_source_view_constructed (GObject *object)
{
  FoundrySourceView *self = (FoundrySourceView *)object;
  FoundrySourceViewPrivate *priv = foundry_source_view_get_instance_private (self);

  G_OBJECT_CLASS (foundry_source_view_parent_class)->constructed (object);
}

static void
foundry_source_view_dispose (GObject *object)
{
  FoundrySourceView *self = (FoundrySourceView *)object;
  FoundrySourceViewPrivate *priv = foundry_source_view_get_instance_private (self);

  foundry_source_view_disconnect_buffer (self);

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
}

static void
foundry_source_view_init (FoundrySourceView *self)
{
  g_signal_connect (self,
                    "notify::buffer",
                    G_CALLBACK (foundry_source_view_notify_buffer),
                    NULL);
}

GtkWidget *
foundry_source_view_new (FoundrySourceBuffer *buffer)
{
  g_return_val_if_fail (FOUNDRY_IS_SOURCE_BUFFER (buffer), NULL);

  return g_object_new (FOUNDRY_TYPE_SOURCE_VIEW,
                       "buffer", buffer,
                       NULL);
}
