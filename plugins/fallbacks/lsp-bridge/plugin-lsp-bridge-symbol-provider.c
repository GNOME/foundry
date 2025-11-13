/* plugin-lsp-bridge-symbol-provider.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "plugin-lsp-bridge-symbol-provider.h"
#include "plugin-lsp-bridge-symbol.h"

struct _PluginLspBridgeSymbolProvider
{
  FoundrySymbolProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginLspBridgeSymbolProvider, plugin_lsp_bridge_symbol_provider, FOUNDRY_TYPE_SYMBOL_PROVIDER)

static char *
find_language_for_file (PluginLspBridgeSymbolProvider *self,
                        GFile                         *file,
                        GBytes                        *contents)
{
  g_autoptr(FoundryFileManager) file_manager = NULL;
  g_autoptr(FoundryTextManager) text_manager = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GListModel) documents = NULL;
  g_autofree char *language_id = NULL;
  guint n_items;

  g_assert (PLUGIN_IS_LSP_BRIDGE_SYMBOL_PROVIDER (self));

  if (!(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))))
    return NULL;

  if (!(text_manager = foundry_context_dup_text_manager (context)))
    return NULL;

  if (!(documents = foundry_text_manager_list_documents (text_manager)))
    return NULL;

  n_items = g_list_model_get_n_items (documents);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryTextDocument) document = g_list_model_get_item (documents, i);
      g_autoptr(GFile) this_file = foundry_text_document_dup_file (document);

      if (g_file_equal (this_file, file))
        {
          g_autoptr(FoundryTextBuffer) buffer = foundry_text_document_dup_buffer (document);

          return foundry_text_buffer_dup_language_id (buffer);
        }
    }

  if (!(file_manager = foundry_context_dup_file_manager (context)))
    return NULL;

  if ((language_id = dex_await_string (foundry_file_manager_guess_language (file_manager, file, NULL, contents), NULL)))
    return g_steal_pointer (&language_id);

  return NULL;
}

static FoundryLspClient *
find_client (PluginLspBridgeSymbolProvider  *self,
             GFile                          *file,
             GBytes                         *contents,
             GError                        **error)
{
  g_autoptr(FoundryLspManager) lsp_manager = NULL;
  g_autoptr(FoundryLspClient) client = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autofree char *language_id = NULL;

  g_assert (PLUGIN_IS_LSP_BRIDGE_SYMBOL_PROVIDER (self));
  g_assert (G_IS_FILE (file));

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (self), error)))
    return NULL;

  if (!(language_id = find_language_for_file (self, file, contents)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "No language specified");
      return NULL;
    }

  lsp_manager = foundry_context_dup_lsp_manager (context);

  if (!(client = dex_await_object (foundry_lsp_manager_load_client (lsp_manager, language_id), error)))
    return NULL;

  if (!foundry_lsp_client_supports_language (client, language_id))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Language `%s` is not supported by language server",
                   language_id);
      return NULL;
    }

  return g_steal_pointer (&client);
}


static DexFuture *
plugin_lsp_bridge_symbol_provider_list_symbols_fiber (PluginLspBridgeSymbolProvider *self,
                                                      GFile                         *file,
                                                      GBytes                        *contents)
{
  g_autoptr(FoundryLspClient) client = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(JsonNode) params = NULL;
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *uri = NULL;
  JsonArray *array = NULL;

  g_assert (PLUGIN_IS_LSP_BRIDGE_SYMBOL_PROVIDER (self));
  g_assert (G_IS_FILE (file));

  if (!(client = find_client (self, file, contents, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  uri = g_file_get_uri (file);

  params = FOUNDRY_JSON_OBJECT_NEW (
    "textDocument", "{",
      "uri", FOUNDRY_JSON_NODE_PUT_STRING (uri),
    "}"
  );

  if (!(reply = dex_await_boxed (foundry_lsp_client_call (client, "textDocument/documentSymbol", params), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  store = g_list_store_new (FOUNDRY_TYPE_SYMBOL);

  if (reply == NULL || !JSON_NODE_HOLDS_ARRAY (reply))
    return dex_future_new_take_object (g_steal_pointer (&store));

  array = json_node_get_array (reply);

  if (array != NULL)
    {
      guint n_items = json_array_get_length (array);

      for (guint i = 0; i < n_items; i++)
        {
          JsonNode *node = json_array_get_element (array, i);
          g_autoptr(PluginLspBridgeSymbol) symbol = NULL;

          if ((symbol = plugin_lsp_bridge_symbol_new (file, node)))
            g_list_store_append (store, FOUNDRY_SYMBOL (symbol));
        }
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static DexFuture *
plugin_lsp_bridge_symbol_provider_list_symbols (FoundrySymbolProvider *self,
                                                GFile                 *file,
                                                GBytes                *contents)
{
  g_assert (PLUGIN_IS_LSP_BRIDGE_SYMBOL_PROVIDER (self));
  g_assert (!file || G_IS_FILE (file));
  g_assert (file || contents);

  if (file == NULL)
    return foundry_future_new_not_supported ();

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_lsp_bridge_symbol_provider_list_symbols_fiber),
                                  3,
                                  FOUNDRY_TYPE_SYMBOL_PROVIDER, self,
                                  G_TYPE_FILE, file,
                                  G_TYPE_BYTES, contents);
}

static DexFuture *
plugin_lsp_bridge_symbol_provider_find_symbol_at_fiber (PluginLspBridgeSymbolProvider *self,
                                                        GFile                         *file,
                                                        GBytes                        *contents,
                                                        guint                          line,
                                                        guint                          line_offset)
{
  g_autoptr(FoundryLspClient) client = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(JsonNode) params = NULL;
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *uri = NULL;
  JsonArray *array = NULL;
  PluginLspBridgeSymbol *symbol = NULL;

  g_assert (PLUGIN_IS_LSP_BRIDGE_SYMBOL_PROVIDER (self));
  g_assert (G_IS_FILE (file));

  if (!(client = find_client (self, file, contents, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  uri = g_file_get_uri (file);

  params = FOUNDRY_JSON_OBJECT_NEW (
    "textDocument", "{",
      "uri", FOUNDRY_JSON_NODE_PUT_STRING (uri),
    "}"
  );

  if (!(reply = dex_await_boxed (foundry_lsp_client_call (client, "textDocument/documentSymbol", params), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (reply == NULL || !JSON_NODE_HOLDS_ARRAY (reply))
    return foundry_future_new_not_supported ();

  array = json_node_get_array (reply);

  if (array == NULL)
    return foundry_future_new_not_supported ();

  store = g_list_store_new (FOUNDRY_TYPE_SYMBOL);

  {
    guint n_items = json_array_get_length (array);

    for (guint i = 0; i < n_items; i++)
      {
        JsonNode *node = json_array_get_element (array, i);
        g_autoptr(PluginLspBridgeSymbol) sym = NULL;

        if ((sym = plugin_lsp_bridge_symbol_new (file, node)))
          g_list_store_append (store, FOUNDRY_SYMBOL (sym));
      }
  }

  {
    guint n_items = g_list_model_get_n_items (G_LIST_MODEL (store));

    for (guint i = 0; i < n_items; i++)
      {
        g_autoptr(PluginLspBridgeSymbol) sym = g_list_model_get_item (G_LIST_MODEL (store), i);
        g_autoptr(PluginLspBridgeSymbol) found = NULL;

        if ((found = plugin_lsp_bridge_symbol_find_at_position (sym, line, line_offset)))
          {
            symbol = g_steal_pointer (&found);
            break;
          }
      }
  }

  if (symbol == NULL)
    return foundry_future_new_not_supported ();

  return dex_future_new_take_object (g_object_ref (FOUNDRY_SYMBOL (symbol)));
}

static DexFuture *
plugin_lsp_bridge_symbol_provider_find_symbol_at (FoundrySymbolProvider *self,
                                                  GFile                 *file,
                                                  GBytes                *contents,
                                                  guint                  line,
                                                  guint                  line_offset)
{
  g_assert (PLUGIN_IS_LSP_BRIDGE_SYMBOL_PROVIDER (self));
  g_assert (!file || G_IS_FILE (file));
  g_assert (file || contents);

  if (file == NULL)
    return foundry_future_new_not_supported ();

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_lsp_bridge_symbol_provider_find_symbol_at_fiber),
                                  5,
                                  FOUNDRY_TYPE_SYMBOL_PROVIDER, self,
                                  G_TYPE_FILE, file,
                                  G_TYPE_BYTES, contents,
                                  G_TYPE_UINT, line,
                                  G_TYPE_UINT, line_offset);
}

static void
plugin_lsp_bridge_symbol_provider_dispose (GObject *object)
{
  G_OBJECT_CLASS (plugin_lsp_bridge_symbol_provider_parent_class)->dispose (object);
}

static void
plugin_lsp_bridge_symbol_provider_class_init (PluginLspBridgeSymbolProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundrySymbolProviderClass *symbol_provider_class = FOUNDRY_SYMBOL_PROVIDER_CLASS (klass);

  object_class->dispose = plugin_lsp_bridge_symbol_provider_dispose;

  symbol_provider_class->find_symbol_at = plugin_lsp_bridge_symbol_provider_find_symbol_at;
  symbol_provider_class->list_symbols = plugin_lsp_bridge_symbol_provider_list_symbols;
}

static void
plugin_lsp_bridge_symbol_provider_init (PluginLspBridgeSymbolProvider *self)
{
}
