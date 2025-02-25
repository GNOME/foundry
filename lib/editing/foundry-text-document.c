/* foundry-text-document.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "foundry-debug.h"
#include "foundry-text-document-private.h"
#include "foundry-text-document-addin.h"
#include "foundry-util.h"

struct _FoundryTextDocument
{
  FoundryContextual  parent_instance;
  FoundryTextBuffer *buffer;
  GFile             *file;
  char              *draft_id;
  PeasExtensionSet  *addins;
};

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_DRAFT_ID,
  PROP_FILE,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryTextDocument, foundry_text_document, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static void
foundry_text_document_addin_added (PeasExtensionSet *set,
                                   PeasPluginInfo   *plugin_info,
                                   GObject          *addin,
                                   gpointer          user_data)
{
  FoundryTextDocument *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_TEXT_DOCUMENT_ADDIN (addin));
  g_assert (FOUNDRY_IS_TEXT_DOCUMENT (self));

  g_debug ("Adding FoundryTextDocumentAddin of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_text_document_addin_load (FOUNDRY_TEXT_DOCUMENT_ADDIN (addin)));
}

static void
foundry_text_document_addin_removed (PeasExtensionSet *set,
                                     PeasPluginInfo   *plugin_info,
                                     GObject          *addin,
                                     gpointer          user_data)
{
  FoundryTextDocument *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_TEXT_DOCUMENT_ADDIN (addin));
  g_assert (FOUNDRY_IS_TEXT_DOCUMENT (self));

  g_debug ("Removing FoundryTextDocumentAddin of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_text_document_addin_unload (FOUNDRY_TEXT_DOCUMENT_ADDIN (addin)));
}

static DexFuture *
foundry_text_document_init_plugins (FoundryTextDocument *self)
{
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_TEXT_DOCUMENT (self));
  g_assert (PEAS_IS_EXTENSION_SET (self->addins));

  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (foundry_text_document_addin_added),
                           self,
                           0);

  g_signal_connect_object (self->addins,
                           "extension-removed",
                           G_CALLBACK (foundry_text_document_addin_removed),
                           self,
                           0);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryTextDocumentAddin) addin = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures, foundry_text_document_addin_load (addin));
    }

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

DexFuture *
foundry_text_document_new (FoundryContext    *context,
                           GFile             *file,
                           const char        *draft_id,
                           FoundryTextBuffer *buffer)
{
  g_autoptr(FoundryTextDocument) self = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_CONTEXT (context));
  dex_return_error_if_fail (!file || G_IS_FILE (file));
  dex_return_error_if_fail (file != NULL || draft_id != NULL);
  dex_return_error_if_fail (FOUNDRY_IS_TEXT_BUFFER (buffer));

  self = g_object_new (FOUNDRY_TYPE_TEXT_DOCUMENT,
                       "context", context,
                       "file", file,
                       "draft-id", draft_id,
                       "buffer", buffer,
                       NULL);

  return dex_future_finally (foundry_text_document_init_plugins (self),
                             foundry_future_return_object,
                             g_object_ref (self),
                             g_object_unref);
}

static void
foundry_text_document_constructed (GObject *object)
{
  FoundryTextDocument *self = (FoundryTextDocument *)object;
  g_autoptr(FoundryContext) context = NULL;

  G_OBJECT_CLASS (foundry_text_document_parent_class)->constructed (object);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         FOUNDRY_TYPE_TEXT_DOCUMENT_ADDIN,
                                         "context", context,
                                         "document", self,
                                         NULL);
}

static void
foundry_text_document_dispose (GObject *object)
{
  FoundryTextDocument *self = (FoundryTextDocument *)object;

  g_clear_object (&self->addins);

  G_OBJECT_CLASS (foundry_text_document_parent_class)->dispose (object);
}

static void
foundry_text_document_finalize (GObject *object)
{
  FoundryTextDocument *self = (FoundryTextDocument *)object;

  g_clear_object (&self->buffer);
  g_clear_object (&self->file);
  g_clear_pointer (&self->draft_id, g_free);

  G_OBJECT_CLASS (foundry_text_document_parent_class)->finalize (object);
}

static void
foundry_text_document_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundryTextDocument *self = FOUNDRY_TEXT_DOCUMENT (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_take_object (value, foundry_text_document_dup_buffer (self));
      break;

    case PROP_DRAFT_ID:
      g_value_set_string (value, self->draft_id);
      break;

    case PROP_FILE:
      g_value_take_object (value, foundry_text_document_dup_file (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, foundry_text_document_dup_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_text_document_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  FoundryTextDocument *self = FOUNDRY_TEXT_DOCUMENT (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      self->buffer = g_value_dup_object (value);
      break;

    case PROP_FILE:
      self->file = g_value_dup_object (value);
      break;

    case PROP_DRAFT_ID:
      self->draft_id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_text_document_class_init (FoundryTextDocumentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = foundry_text_document_constructed;
  object_class->dispose = foundry_text_document_dispose;
  object_class->finalize = foundry_text_document_finalize;
  object_class->get_property = foundry_text_document_get_property;
  object_class->set_property = foundry_text_document_set_property;

  properties[PROP_BUFFER] =
    g_param_spec_object ("buffer", NULL, NULL,
                         FOUNDRY_TYPE_TEXT_BUFFER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_DRAFT_ID] =
    g_param_spec_string ("draft-id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_text_document_init (FoundryTextDocument *self)
{
}

/**
 * foundry_text_document_dup_file:
 * @self: a #FoundryTextDocument
 *
 * Gets the underlying [iface@Gio.File].
 *
 * Returns: (transfer full) (nullable): a [iface@Gio.File] or %NULL
 */
GFile *
foundry_text_document_dup_file (FoundryTextDocument *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEXT_DOCUMENT (self), NULL);

  return self->file ? g_object_ref (self->file) : NULL;
}

/**
 * foundry_text_document_dup_buffer:
 * @self: a #FoundryTextDocument
 *
 * Gets the underlying [iface@Foundry.TextBuffer].
 *
 * Returns: (transfer full): a [iface@Foundry.TextBuffer].
 */
FoundryTextBuffer *
foundry_text_document_dup_buffer (FoundryTextDocument *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEXT_DOCUMENT (self), NULL);

  return self->buffer ? g_object_ref (self->buffer) : NULL;
}

/**
 * foundry_text_document_dup_title:
 * @self: a #FoundryTextDocument
 *
 * Gets the title for the document.
 *
 * Returns: (transfer full) (nullable): a string or %NULL
 */
char *
foundry_text_document_dup_title (FoundryTextDocument *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEXT_DOCUMENT (self), NULL);

  if (self->file)
    return g_file_get_basename (self->file);

  return NULL;
}

/**
 * foundry_text_document_when_changed:
 * @self: a #FoundryTextDocument
 *
 * This function will resolve a new [class@Dex.Future] when the underlying
 * [iface@Foundry.TextBuffer] has changed.
 *
 * Use this to invalidate external resources that rely on the contents of
 * the document buffer.
 *
 * This notification is done after a short delay to avoid spurious events
 * during rapid modification to the buffer.
 *
 * If the document is dispoed, this future will reject with error.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 */
DexFuture *
foundry_text_document_when_changed (FoundryTextDocument *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_TEXT_DOCUMENT (self));

  /* TODO: */

  return NULL;
}

/**
 * foundry_text_document_list_code_actions:
 * @self: a #FoundryTextDocument
 *
 * Queries [class@Foundry.CodeActionProvider] for actions that are
 * relevant to the document based on the current diagnostics.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.FutureListModel] of [class@Foundry.CodeAction].
 */
DexFuture *
foundry_text_document_list_code_actions (FoundryTextDocument *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_TEXT_DOCUMENT (self));

  /* TODO: */

  return NULL;
}

/**
 * foundry_text_document_list_diagnostics:
 * @self: a #FoundryTextDocument
 *
 * Queries [class@Foundry.DiagnosticProvider] for diagnostics that are
 * relevant to the document.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.FutureListModel] of [class@Foundry.Diagnostic].
 */
DexFuture *
foundry_text_document_list_diagnostics (FoundryTextDocument *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_TEXT_DOCUMENT (self));

  /* TODO: */

  return NULL;
}

/**
 * foundry_text_document_list_symbols:
 * @self: a #FoundryTextDocument
 *
 * Queries [class@Foundry.SymbolProvider] for symbols that are
 * relevant to the document.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.FutureListModel] of [class@Foundry.Symbol].
 */
DexFuture *
foundry_text_document_list_symbols (FoundryTextDocument *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_TEXT_DOCUMENT (self));

  /* TODO: */

  return NULL;
}
