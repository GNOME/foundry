/* plugin-lsp-bridge-code-fold-provider.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "plugin-lsp-bridge-code-fold-provider.h"
#include "plugin-lsp-bridge-code-fold-provider-private.h"

struct _PluginLspBridgeCodeFoldProvider
{
  FoundryCodeFoldProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginLspBridgeCodeFoldProvider, plugin_lsp_bridge_code_fold_provider, FOUNDRY_TYPE_CODE_FOLD_PROVIDER)

static gboolean
read_line (JsonObject *object,
           const char *name,
           guint      *line)
{
  JsonNode *node;
  gint64 value;

  g_assert (object != NULL);
  g_assert (name != NULL);
  g_assert (line != NULL);

  if (!(node = json_object_get_member (object, name)) ||
      !JSON_NODE_HOLDS_VALUE (node) ||
      json_node_get_value_type (node) != G_TYPE_INT64)
    return FALSE;

  value = json_node_get_int (node);
  if (value < 0 || value > G_MAXUINT)
    return FALSE;

  *line = value;
  return TRUE;
}

static int
compare_folds (gconstpointer a,
               gconstpointer b)
{
  FoundryCodeFold *fa = *(FoundryCodeFold * const *)a;
  FoundryCodeFold *fb = *(FoundryCodeFold * const *)b;
  guint sa = foundry_code_fold_get_start (fa);
  guint sb = foundry_code_fold_get_start (fb);
  guint ea = foundry_code_fold_get_end (fa);
  guint eb = foundry_code_fold_get_end (fb);

  if (sa != sb)
    return (sa > sb) - (sa < sb);

  return (eb > ea) - (eb < ea);
}

GListModel *
_plugin_lsp_bridge_convert_folding_ranges (JsonNode *reply,
                                           GBytes   *contents)
{
  g_autoptr(GArray) lines = NULL;
  g_autoptr(GPtrArray) folds = NULL;
  g_autoptr(GPtrArray) stack = NULL;
  GListStore *store;
  const char *text;
  gsize size;
  guint offset = 0;
  JsonArray *ranges;

  g_assert (contents != NULL);

  store = g_list_store_new (FOUNDRY_TYPE_CODE_FOLD);
  if (reply == NULL || !JSON_NODE_HOLDS_ARRAY (reply))
    return G_LIST_MODEL (store);

  text = g_bytes_get_data (contents, &size);
  if (size == 0 || !g_utf8_validate (text, size, NULL))
    return G_LIST_MODEL (store);

  lines = g_array_new (FALSE, FALSE, sizeof (guint));
  g_array_append_val (lines, offset);
  for (const char *p = text; p < text + size; p = g_utf8_next_char (p))
    {
      offset++;
      if (*p == '\n')
        g_array_append_val (lines, offset);
    }

  folds = g_ptr_array_new_with_free_func (g_object_unref);
  ranges = json_node_get_array (reply);
  for (guint i = 0; i < json_array_get_length (ranges); i++)
    {
      JsonNode *node = json_array_get_element (ranges, i);
      JsonObject *object;
      guint first, last, anchor, start, end;

      if (!JSON_NODE_HOLDS_OBJECT (node))
        continue;

      object = json_node_get_object (node);
      if (!read_line (object, "startLine", &first) ||
          !read_line (object, "endLine", &last) ||
          first >= last || last >= lines->len)
        continue;

      anchor = g_array_index (lines, guint, first);
      start = g_array_index (lines, guint, first + 1);
      end = last + 1 < lines->len ? g_array_index (lines, guint, last + 1) : offset;
      if (start >= end)
        continue;

      g_ptr_array_add (folds, foundry_code_fold_new (anchor, start, end));
    }

  g_ptr_array_sort (folds, compare_folds);
  stack = g_ptr_array_new ();
  for (guint i = 0; i < folds->len; i++)
    {
      FoundryCodeFold *fold = g_ptr_array_index (folds, i);
      guint start = foundry_code_fold_get_start (fold);
      guint end = foundry_code_fold_get_end (fold);
      FoundryCodeFold *parent;

      while (stack->len > 0 &&
             foundry_code_fold_get_end (g_ptr_array_index (stack, stack->len - 1)) <= start)
        g_ptr_array_set_size (stack, stack->len - 1);

      parent = stack->len ? g_ptr_array_index (stack, stack->len - 1) : NULL;
      if (parent != NULL &&
          (end > foundry_code_fold_get_end (parent) ||
           (start == foundry_code_fold_get_start (parent) &&
            end == foundry_code_fold_get_end (parent)) ||
           foundry_code_fold_get_anchor (fold) < foundry_code_fold_get_anchor (parent)))
        continue;

      g_list_store_append (store, fold);
      g_ptr_array_add (stack, fold);
    }

  return G_LIST_MODEL (store);
}

static DexFuture *
list_folds_fiber (PluginLspBridgeCodeFoldProvider *self,
                  FoundryTextDocument             *document,
                  GBytes                          *contents)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryLspManager) manager = NULL;
  g_autoptr(FoundryLspClient) client = NULL;
  g_autoptr(FoundryTextBuffer) buffer = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(JsonNode) capabilities = NULL;
  g_autoptr(JsonNode) params = NULL;
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *language = NULL;
  g_autofree char *uri = NULL;
  JsonNode *capability;
  JsonObject *object;

  g_assert (PLUGIN_IS_LSP_BRIDGE_CODE_FOLD_PROVIDER (self));
  g_assert (FOUNDRY_IS_TEXT_DOCUMENT (document));
  g_assert (contents != NULL);

  file = foundry_text_document_dup_file (document);
  buffer = foundry_text_document_dup_buffer (document);
  language = foundry_text_buffer_dup_language_id (buffer);
  if (file == NULL || language == NULL)
    return foundry_future_new_not_supported ();

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  manager = foundry_context_dup_lsp_manager (context);
  if (!(client = dex_await_object (foundry_lsp_manager_load_client (manager, language), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!foundry_lsp_client_supports_language (client, language))
    return foundry_future_new_not_supported ();

  if (!(capabilities = dex_await_boxed (foundry_lsp_client_query_capabilities (client), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!JSON_NODE_HOLDS_OBJECT (capabilities))
    return foundry_future_new_not_supported ();

  object = json_node_get_object (capabilities);
  capability = json_object_get_member (object, "foldingRangeProvider");
  if (capability == NULL ||
      !(JSON_NODE_HOLDS_OBJECT (capability) ||
        (JSON_NODE_HOLDS_VALUE (capability) &&
         json_node_get_value_type (capability) == G_TYPE_BOOLEAN &&
         json_node_get_boolean (capability))))
    return foundry_future_new_not_supported ();

  uri = g_file_get_uri (file);
  params = FOUNDRY_JSON_OBJECT_NEW (
    "textDocument", "{",
      "uri", FOUNDRY_JSON_NODE_PUT_STRING (uri),
    "}"
  );

  reply = dex_await_boxed (foundry_lsp_client_call (client, "textDocument/foldingRange", params), &error);
  if (error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (reply != NULL && !JSON_NODE_HOLDS_NULL (reply) && !JSON_NODE_HOLDS_ARRAY (reply))
    return dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                                  "Invalid folding range response");

  return dex_future_new_take_object (_plugin_lsp_bridge_convert_folding_ranges (reply, contents));
}

static DexFuture *
plugin_lsp_bridge_code_fold_provider_list_folds (FoundryCodeFoldProvider *provider,
                                                 FoundryTextDocument     *document,
                                                 GBytes                  *contents)
{
  return dex_scheduler_spawnv (NULL, 0,
                               G_CALLBACK (list_folds_fiber),
                               3,
                               PLUGIN_TYPE_LSP_BRIDGE_CODE_FOLD_PROVIDER, provider,
                               FOUNDRY_TYPE_TEXT_DOCUMENT, document,
                               G_TYPE_BYTES, contents);
}

static void
plugin_lsp_bridge_code_fold_provider_class_init (PluginLspBridgeCodeFoldProviderClass *klass)
{
  FOUNDRY_CODE_FOLD_PROVIDER_CLASS (klass)->list_folds = plugin_lsp_bridge_code_fold_provider_list_folds;
}

static void
plugin_lsp_bridge_code_fold_provider_init (PluginLspBridgeCodeFoldProvider *self)
{
}
