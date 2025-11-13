/* foundry-file-symbol.c
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

#include "foundry-file-symbol-private.h"
#include "foundry-context.h"
#include "foundry-symbol-locator.h"
#include "foundry-text-document.h"
#include "foundry-text-manager.h"
#include "foundry-util.h"

struct _FoundryFileSymbol
{
  FoundrySymbol parent_instance;

  FoundryContext *context;
  GFile *file;
};

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_FILE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryFileSymbol, foundry_file_symbol, FOUNDRY_TYPE_SYMBOL)

static GParamSpec *properties[N_PROPS];

static char *
foundry_file_symbol_dup_name (FoundrySymbol *symbol)
{
  FoundryFileSymbol *self = FOUNDRY_FILE_SYMBOL (symbol);

  g_return_val_if_fail (FOUNDRY_IS_FILE_SYMBOL (self), NULL);

  if (self->file == NULL)
    return NULL;

  return g_file_get_basename (self->file);
}

static DexFuture *
foundry_file_symbol_find_parent (FoundrySymbol *symbol)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SYMBOL (symbol), NULL);

  return dex_future_new_take_object (NULL);
}

static DexFuture *
foundry_file_symbol_list_children_fiber (gpointer data)
{
  FoundryFileSymbol *self = data;
  g_autoptr(FoundryTextManager) text_manager = NULL;
  g_autoptr(GListModel) documents = NULL;
  guint n_items;
  guint i;

  g_assert (FOUNDRY_IS_FILE_SYMBOL (self));

  if (self->file == NULL || self->context == NULL)
    return foundry_future_new_not_supported ();

  text_manager = foundry_context_dup_text_manager (self->context);
  if (text_manager == NULL)
    return foundry_future_new_not_supported ();

  documents = foundry_text_manager_list_documents (text_manager);
  if (documents == NULL)
    return foundry_future_new_not_supported ();

  n_items = g_list_model_get_n_items (documents);

  for (i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryTextDocument) document = NULL;
      g_autoptr(GFile) doc_file = NULL;

      document = g_list_model_get_item (documents, i);
      doc_file = foundry_text_document_dup_file (document);

      if (doc_file != NULL && g_file_equal (doc_file, self->file))
        return foundry_text_document_list_symbols (document);
    }

  return foundry_future_new_not_supported ();
}

static DexFuture *
foundry_file_symbol_list_children (FoundrySymbol *symbol)
{
  FoundryFileSymbol *self = FOUNDRY_FILE_SYMBOL (symbol);

  g_return_val_if_fail (FOUNDRY_IS_FILE_SYMBOL (self), NULL);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_file_symbol_list_children_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static FoundrySymbolLocator *
foundry_file_symbol_dup_locator (FoundrySymbol *symbol)
{
  FoundryFileSymbol *self = FOUNDRY_FILE_SYMBOL (symbol);

  g_return_val_if_fail (FOUNDRY_IS_FILE_SYMBOL (self), NULL);

  if (self->file == NULL)
    return NULL;

  return foundry_symbol_locator_new_for_file (self->file);
}

static GIcon *
foundry_file_symbol_dup_icon (FoundrySymbol *symbol)
{
  return g_themed_icon_new ("text-x-generic-symbolic");
}

static void
foundry_file_symbol_dispose (GObject *object)
{
  FoundryFileSymbol *self = FOUNDRY_FILE_SYMBOL (object);

  g_clear_object (&self->context);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (foundry_file_symbol_parent_class)->dispose (object);
}

static void
foundry_file_symbol_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundryFileSymbol *self = FOUNDRY_FILE_SYMBOL (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_take_object (value, self->context ? g_object_ref (self->context) : NULL);
      break;

    case PROP_FILE:
      g_value_take_object (value, self->file ? g_object_ref (self->file) : NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_file_symbol_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  FoundryFileSymbol *self = FOUNDRY_FILE_SYMBOL (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_set_object (&self->context, g_value_get_object (value));
      break;

    case PROP_FILE:
      g_set_object (&self->file, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_file_symbol_class_init (FoundryFileSymbolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundrySymbolClass *symbol_class = FOUNDRY_SYMBOL_CLASS (klass);

  object_class->dispose = foundry_file_symbol_dispose;
  object_class->get_property = foundry_file_symbol_get_property;
  object_class->set_property = foundry_file_symbol_set_property;

  symbol_class->dup_name = foundry_file_symbol_dup_name;
  symbol_class->find_parent = foundry_file_symbol_find_parent;
  symbol_class->list_children = foundry_file_symbol_list_children;
  symbol_class->dup_locator = foundry_file_symbol_dup_locator;
  symbol_class->dup_icon = foundry_file_symbol_dup_icon;

  properties[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         FOUNDRY_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_file_symbol_init (FoundryFileSymbol *self)
{
}

FoundryFileSymbol *
foundry_file_symbol_new (FoundryContext *context,
                         GFile          *file)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (FOUNDRY_TYPE_FILE_SYMBOL,
                       "context", context,
                       "file", file,
                       NULL);
}
