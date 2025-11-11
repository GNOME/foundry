/* plugin-tree-sitter-symbol-provider.c
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

#include <tree_sitter/api.h>
#include <tree_sitter/tree-sitter-bash.h>
#include <tree_sitter/tree-sitter-c.h>
#include <tree_sitter/tree-sitter-c-sharp.h>
#include <tree_sitter/tree-sitter-cmake.h>
#include <tree_sitter/tree-sitter-cpp.h>
#include <tree_sitter/tree-sitter-css.h>
#include <tree_sitter/tree-sitter-go.h>
#include <tree_sitter/tree-sitter-heex.h>
#include <tree_sitter/tree-sitter-html.h>
#include <tree_sitter/tree-sitter-java.h>
#include <tree_sitter/tree-sitter-javascript.h>
#include <tree_sitter/tree-sitter-jsdoc.h>
#include <tree_sitter/tree-sitter-json.h>
#include <tree_sitter/tree-sitter-lua.h>
#include <tree_sitter/tree-sitter-php.h>
#include <tree_sitter/tree-sitter-php_only.h>
#include <tree_sitter/tree-sitter-python.h>
#include <tree_sitter/tree-sitter-ruby.h>
#include <tree_sitter/tree-sitter-rust.h>
#include <tree_sitter/tree-sitter-toml.h>
#include <tree_sitter/tree-sitter-tsx.h>
#include <tree_sitter/tree-sitter-typescript.h>
#include <tree_sitter/tree-sitter-yaml.h>

#include "parsed-tree.h"
#include "plugin-tree-sitter-symbol-provider.h"
#include "plugin-tree-sitter-symbol.h"

struct _PluginTreeSitterSymbolProvider
{
  FoundrySymbolProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginTreeSitterSymbolProvider, plugin_tree_sitter_symbol_provider, FOUNDRY_TYPE_SYMBOL_PROVIDER)

static const TSLanguage *
get_tree_sitter_language (const char *language_id)
{
  if (language_id == NULL)
    return NULL;

  /* Map GtkSourceView language IDs to tree-sitter languages */
  if (g_str_equal (language_id, "bash") || g_str_equal (language_id, "sh"))
    return tree_sitter_bash ();
  if (g_str_equal (language_id, "c") || g_str_equal (language_id, "chdr"))
    return tree_sitter_c ();
  if (g_str_equal (language_id, "c-sharp"))
    return tree_sitter_c_sharp ();
  if (g_str_equal (language_id, "cmake"))
    return tree_sitter_cmake ();
  if (g_str_equal (language_id, "cpp") || g_str_equal (language_id, "cpphdr"))
    return tree_sitter_cpp ();
  if (g_str_equal (language_id, "css"))
    return tree_sitter_css ();
  if (g_str_equal (language_id, "go"))
    return tree_sitter_go ();
  if (g_str_equal (language_id, "heex"))
    return tree_sitter_heex ();
  if (g_str_equal (language_id, "html"))
    return tree_sitter_html ();
  if (g_str_equal (language_id, "java"))
    return tree_sitter_java ();
  if (g_str_equal (language_id, "javascript") || g_str_equal (language_id, "js"))
    return tree_sitter_javascript ();
  if (g_str_equal (language_id, "jsdoc"))
    return tree_sitter_jsdoc ();
  if (g_str_equal (language_id, "json"))
    return tree_sitter_json ();
  if (g_str_equal (language_id, "lua"))
    return tree_sitter_lua ();
  if (g_str_equal (language_id, "php"))
    return tree_sitter_php ();
  if (g_str_equal (language_id, "python") || g_str_equal (language_id, "python3"))
    return tree_sitter_python ();
  if (g_str_equal (language_id, "ruby"))
    return tree_sitter_ruby ();
  if (g_str_equal (language_id, "rust"))
    return tree_sitter_rust ();
  if (g_str_equal (language_id, "toml"))
    return tree_sitter_toml ();
  if (g_str_equal (language_id, "tsx"))
    return tree_sitter_tsx ();
  if (g_str_equal (language_id, "typescript"))
    return tree_sitter_typescript ();
  if (g_str_equal (language_id, "yaml"))
    return tree_sitter_yaml ();

  return NULL;
}

static gboolean
is_container_node (TSNode node)
{
  const char *type = ts_node_type (node);

  if (type == NULL)
    return FALSE;

  /* Filter out common container node types that wrap actual symbols */
  if (g_str_equal (type, "translation_unit") ||
      g_str_equal (type, "program") ||
      g_str_equal (type, "source_file") ||
      g_str_equal (type, "module") ||
      g_str_equal (type, "compilation_unit"))
    return TRUE;

  return FALSE;
}

static gboolean
is_valid_symbol_node (TSNode node)
{
  guint child_count = ts_node_named_child_count (node);
  const char *type = ts_node_type (node);

  if (type == NULL || !ts_node_is_named (node))
    return FALSE;

  /* Skip container nodes */
  if (is_container_node (node))
    return FALSE;

  /* Check if this node has an identifier child */
  for (guint i = 0; i < child_count; i++)
    {
      TSNode child = ts_node_named_child (node, i);
      const char *child_type = ts_node_type (child);

      if (child_type != NULL && g_str_equal (child_type, "identifier"))
        return TRUE;
    }

  /* For some node types, the identifier might be the node itself */
  if (g_str_equal (type, "identifier") ||
      g_str_equal (type, "type_identifier") ||
      g_str_equal (type, "field_identifier"))
    return TRUE;

  return FALSE;
}

static void
collect_toplevel_symbols_recursive (ParsedTree *parsed_tree,
                                    TSNode      node,
                                    GListStore *store)
{
  guint child_count;

  if (ts_node_is_null (node))
    return;

  child_count = ts_node_named_child_count (node);

  for (guint i = 0; i < child_count; i++)
    {
      TSNode child = ts_node_named_child (node, i);

      if (is_container_node (child))
        {
          /* Recursively search inside container nodes */
          collect_toplevel_symbols_recursive (parsed_tree, child, store);
        }
      else if (is_valid_symbol_node (child))
        {
          /* Found a toplevel symbol */
          g_autoptr(PluginTreeSitterSymbol) symbol = NULL;

          symbol = plugin_tree_sitter_symbol_new (parsed_tree, child);
          g_list_store_append (store, symbol);
        }
    }
}

static TSNode
find_symbol_node_at (TSNode node,
                     guint  line,
                     guint  line_offset)
{
  TSPoint point = { .row = line, .column = line_offset };
  TSNode descendant;
  TSNode best = node;

  if (ts_node_is_null (node))
    return node;

  descendant = ts_node_descendant_for_point_range (node, point, point);

  if (!ts_node_is_null (descendant))
    {
      const char *type = ts_node_type (descendant);

      if (type != NULL && ts_node_is_named (descendant))
        {
          /* Look for identifier-like nodes */
          if (g_str_equal (type, "identifier") ||
              g_str_equal (type, "function_declarator") ||
              g_str_equal (type, "declarator") ||
              g_str_equal (type, "type_identifier") ||
              g_str_equal (type, "field_identifier") ||
              g_str_equal (type, "field_declaration") ||
              g_str_equal (type, "declaration"))
            {
              best = descendant;
            }
        }
    }

  /* Walk up to find a named node that contains an identifier */
  while (!ts_node_is_null (best))
    {
      guint child_count = ts_node_named_child_count (best);
      const char *type = ts_node_type (best);

      if (type != NULL && ts_node_is_named (best))
        {
          /* Check if this node has an identifier child */
          for (guint i = 0; i < child_count; i++)
            {
              TSNode child = ts_node_named_child (best, i);
              const char *child_type = ts_node_type (child);

              if (child_type != NULL && g_str_equal (child_type, "identifier"))
                return best;
            }

          /* For some node types, the identifier might be the node itself */
          if (g_str_equal (type, "identifier") ||
              g_str_equal (type, "type_identifier") ||
              g_str_equal (type, "field_identifier"))
            return best;
        }

      best = ts_node_parent (best);
    }

  return best;
}


static DexFuture *
plugin_tree_sitter_symbol_provider_find_symbol_at_fiber (PluginTreeSitterSymbolProvider *self,
                                                         GFile                          *file,
                                                         GBytes                         *contents,
                                                         guint                           line,
                                                         guint                           line_offset)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryFileManager) file_manager = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *language_id = NULL;
  const TSLanguage *ts_language = NULL;
  TSParser *parser = NULL;
  TSTree *tree = NULL;
  TSNode root;
  TSNode symbol_node;
  g_autoptr(ParsedTree) parsed_tree = NULL;
  g_autoptr(PluginTreeSitterSymbol) symbol = NULL;
  g_autoptr(GBytes) source_bytes = NULL;
  const char *source_text = NULL;
  gsize source_length = 0;

  g_assert (PLUGIN_IS_TREE_SITTER_SYMBOL_PROVIDER (self));
  g_assert (G_IS_FILE (file));

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  file_manager = foundry_context_dup_file_manager (context);

  if (!(language_id = dex_await_string (foundry_file_manager_guess_language (file_manager, file, NULL, contents), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(ts_language = get_tree_sitter_language (language_id)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Language '%s' not supported by tree-sitter",
                                  language_id);

  parser = ts_parser_new ();
  if (!ts_parser_set_language (parser, ts_language))
    {
      ts_parser_delete (parser);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "Failed to set tree-sitter language");
    }

  if (contents != NULL)
    {
      source_bytes = g_bytes_ref (contents);
      source_text = (const char *)g_bytes_get_data (contents, &source_length);
    }
  else
    {
      g_autofree char *file_path = NULL;

      file_path = g_file_get_path (file);
      if (file_path == NULL)
        {
          ts_parser_delete (parser);
          return dex_future_new_reject (G_IO_ERROR,
                                        G_IO_ERROR_NOT_SUPPORTED,
                                        "Cannot read file without path");
        }

      source_bytes = dex_await_object (dex_file_load_contents_bytes (file), &error);
      if (source_bytes == NULL)
        {
          ts_parser_delete (parser);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }

      source_text = (const char *)g_bytes_get_data (source_bytes, &source_length);
    }

  if (source_text == NULL || source_length == 0)
    {
      ts_parser_delete (parser);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_INVALID_DATA,
                                    "File is empty");
    }

  tree = ts_parser_parse_string (parser, NULL, source_text, (uint32_t)source_length);

  if (tree == NULL)
    {
      ts_parser_delete (parser);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "Failed to parse file with tree-sitter");
    }

  parsed_tree = parsed_tree_new (parser, tree, source_bytes);
  parser = NULL;
  tree = NULL;

  root = ts_tree_root_node (parsed_tree_get_tree (parsed_tree));
  symbol_node = find_symbol_node_at (root, line, line_offset);

  if (ts_node_is_null (symbol_node))
    {
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_NOT_FOUND,
                                    "No symbol found at line %u, offset %u",
                                    line, line_offset);
    }

  symbol = plugin_tree_sitter_symbol_new (parsed_tree, symbol_node);

  return dex_future_new_take_object (g_steal_pointer (&symbol));
}

static DexFuture *
plugin_tree_sitter_symbol_provider_list_symbols_fiber (PluginTreeSitterSymbolProvider *self,
                                                       GFile                          *file,
                                                       GBytes                         *contents)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryFileManager) file_manager = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *language_id = NULL;
  const TSLanguage *ts_language = NULL;
  TSParser *parser = NULL;
  TSTree *tree = NULL;
  TSNode root;
  g_autoptr(ParsedTree) parsed_tree = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GBytes) source_bytes = NULL;
  const char *source_text = NULL;
  gsize source_length = 0;

  g_assert (PLUGIN_IS_TREE_SITTER_SYMBOL_PROVIDER (self));
  g_assert (G_IS_FILE (file));

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  file_manager = foundry_context_dup_file_manager (context);

  if (!(language_id = dex_await_string (foundry_file_manager_guess_language (file_manager, file, NULL, contents), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(ts_language = get_tree_sitter_language (language_id)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Language '%s' not supported by tree-sitter",
                                  language_id);

  parser = ts_parser_new ();
  if (!ts_parser_set_language (parser, ts_language))
    {
      ts_parser_delete (parser);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "Failed to set tree-sitter language");
    }

  if (contents != NULL)
    {
      source_bytes = g_bytes_ref (contents);
      source_text = (const char *)g_bytes_get_data (contents, &source_length);
    }
  else
    {
      g_autofree char *file_path = NULL;

      file_path = g_file_get_path (file);
      if (file_path == NULL)
        {
          ts_parser_delete (parser);
          return dex_future_new_reject (G_IO_ERROR,
                                        G_IO_ERROR_NOT_SUPPORTED,
                                        "Cannot read file without path");
        }

      source_bytes = dex_await_object (dex_file_load_contents_bytes (file), &error);
      if (source_bytes == NULL)
        {
          ts_parser_delete (parser);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }

      source_text = (const char *)g_bytes_get_data (source_bytes, &source_length);
    }

  if (source_text == NULL || source_length == 0)
    {
      ts_parser_delete (parser);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_INVALID_DATA,
                                    "File is empty");
    }

  tree = ts_parser_parse_string (parser, NULL, source_text, (uint32_t)source_length);

  if (tree == NULL)
    {
      ts_parser_delete (parser);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "Failed to parse file with tree-sitter");
    }

  parsed_tree = parsed_tree_new (parser, tree, source_bytes);
  parser = NULL;
  tree = NULL;

  root = ts_tree_root_node (parsed_tree_get_tree (parsed_tree));
  store = g_list_store_new (PLUGIN_TYPE_TREE_SITTER_SYMBOL);

  /* Recursively collect toplevel symbols, skipping container nodes */
  collect_toplevel_symbols_recursive (parsed_tree, root, store);

  return dex_future_new_take_object (G_LIST_MODEL (g_steal_pointer (&store)));
}

static DexFuture *
plugin_tree_sitter_symbol_provider_list_symbols (FoundrySymbolProvider *symbol_provider,
                                                 GFile                 *file,
                                                 GBytes                *contents)
{
  PluginTreeSitterSymbolProvider *self = PLUGIN_TREE_SITTER_SYMBOL_PROVIDER (symbol_provider);

  g_return_val_if_fail (PLUGIN_IS_TREE_SITTER_SYMBOL_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_tree_sitter_symbol_provider_list_symbols_fiber),
                                  3,
                                  PLUGIN_TYPE_TREE_SITTER_SYMBOL_PROVIDER, self,
                                  G_TYPE_FILE, file,
                                  G_TYPE_BYTES, contents);
}

static DexFuture *
plugin_tree_sitter_symbol_provider_find_symbol_at (FoundrySymbolProvider *symbol_provider,
                                                   GFile                 *file,
                                                   GBytes                *contents,
                                                   guint                  line,
                                                   guint                  line_offset)
{
  PluginTreeSitterSymbolProvider *self = PLUGIN_TREE_SITTER_SYMBOL_PROVIDER (symbol_provider);

  g_return_val_if_fail (PLUGIN_IS_TREE_SITTER_SYMBOL_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_tree_sitter_symbol_provider_find_symbol_at_fiber),
                                  5,
                                  PLUGIN_TYPE_TREE_SITTER_SYMBOL_PROVIDER, self,
                                  G_TYPE_FILE, file,
                                  G_TYPE_BYTES, contents,
                                  G_TYPE_UINT, line,
                                  G_TYPE_UINT, line_offset);
}

static void
plugin_tree_sitter_symbol_provider_finalize (GObject *object)
{
  G_OBJECT_CLASS (plugin_tree_sitter_symbol_provider_parent_class)->finalize (object);
}

static void
plugin_tree_sitter_symbol_provider_class_init (PluginTreeSitterSymbolProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundrySymbolProviderClass *provider_class = FOUNDRY_SYMBOL_PROVIDER_CLASS (klass);

  object_class->finalize = plugin_tree_sitter_symbol_provider_finalize;

  provider_class->list_symbols = plugin_tree_sitter_symbol_provider_list_symbols;
  provider_class->find_symbol_at = plugin_tree_sitter_symbol_provider_find_symbol_at;
}

static void
plugin_tree_sitter_symbol_provider_init (PluginTreeSitterSymbolProvider *self)
{
}
