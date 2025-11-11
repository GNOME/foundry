/* plugin-tree-sitter-symbol.c
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

#include "plugin-tree-sitter-symbol.h"

struct _PluginTreeSitterSymbol
{
  FoundrySymbol parent_instance;
  ParsedTree   *parsed_tree;
  TSNode        node;
};

G_DEFINE_FINAL_TYPE (PluginTreeSitterSymbol, plugin_tree_sitter_symbol, FOUNDRY_TYPE_SYMBOL)

static char *
extract_symbol_name (ParsedTree *parsed_tree,
                     TSNode      node)
{
  guint child_count = ts_node_named_child_count (node);
  const char *type = ts_node_type (node);

  if (type == NULL || parsed_tree == NULL)
    return NULL;

  /* If the node itself is an identifier, extract its text */
  if (g_str_equal (type, "identifier") ||
      g_str_equal (type, "type_identifier") ||
      g_str_equal (type, "field_identifier"))
    {
      uint32_t start_byte = ts_node_start_byte (node);
      uint32_t end_byte = ts_node_end_byte (node);
      uint32_t length = end_byte - start_byte;

      if (length > 0 && start_byte < end_byte)
        return parsed_tree_get_text (parsed_tree, start_byte, length);
    }

  /* Look for identifier child */
  for (guint i = 0; i < child_count; i++)
    {
      TSNode child = ts_node_named_child (node, i);
      const char *child_type = ts_node_type (child);

      if (child_type != NULL && (g_str_equal (child_type, "identifier") ||
                                 g_str_equal (child_type, "type_identifier") ||
                                 g_str_equal (child_type, "field_identifier")))
        {
          uint32_t start_byte = ts_node_start_byte (child);
          uint32_t end_byte = ts_node_end_byte (child);
          uint32_t length = end_byte - start_byte;

          if (length > 0 && start_byte < end_byte)
            return parsed_tree_get_text (parsed_tree, start_byte, length);
        }
    }

  return NULL;
}

static gboolean
is_valid_symbol_node (TSNode node)
{
  guint child_count = ts_node_named_child_count (node);
  const char *type = ts_node_type (node);

  if (type == NULL || !ts_node_is_named (node))
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

static TSNode
find_parent_symbol_node (TSNode node)
{
  TSNode parent = ts_node_parent (node);

  /* Walk up to find a named node that contains an identifier */
  while (!ts_node_is_null (parent))
    {
      if (is_valid_symbol_node (parent))
        return parent;

      parent = ts_node_parent (parent);
    }

  return parent;
}

static void
plugin_tree_sitter_symbol_finalize (GObject *object)
{
  PluginTreeSitterSymbol *self = PLUGIN_TREE_SITTER_SYMBOL (object);

  g_clear_pointer (&self->parsed_tree, parsed_tree_unref);

  G_OBJECT_CLASS (plugin_tree_sitter_symbol_parent_class)->finalize (object);
}

static char *
plugin_tree_sitter_symbol_dup_name (FoundrySymbol *symbol)
{
  PluginTreeSitterSymbol *self = PLUGIN_TREE_SITTER_SYMBOL (symbol);

  return extract_symbol_name (self->parsed_tree, self->node);
}

static DexFuture *
plugin_tree_sitter_symbol_find_parent_fiber (gpointer data)
{
  PluginTreeSitterSymbol *self = data;
  TSNode parent_node;
  g_autoptr(PluginTreeSitterSymbol) parent = NULL;

  g_assert (PLUGIN_IS_TREE_SITTER_SYMBOL (self));

  parent_node = find_parent_symbol_node (self->node);

  if (ts_node_is_null (parent_node))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "No parent symbol found");

  parent = plugin_tree_sitter_symbol_new (self->parsed_tree, parent_node);

  return dex_future_new_take_object (g_steal_pointer (&parent));
}

static DexFuture *
plugin_tree_sitter_symbol_find_parent (FoundrySymbol *symbol)
{
  PluginTreeSitterSymbol *self = PLUGIN_TREE_SITTER_SYMBOL (symbol);

  g_return_val_if_fail (PLUGIN_IS_TREE_SITTER_SYMBOL (self), NULL);

  return dex_scheduler_spawn (NULL, 0,
                              plugin_tree_sitter_symbol_find_parent_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static DexFuture *
plugin_tree_sitter_symbol_list_children_fiber (gpointer data)
{
  PluginTreeSitterSymbol *self = data;
  g_autoptr(GListStore) store = NULL;
  guint child_count;

  g_assert (PLUGIN_IS_TREE_SITTER_SYMBOL (self));

  store = g_list_store_new (PLUGIN_TYPE_TREE_SITTER_SYMBOL);
  child_count = ts_node_named_child_count (self->node);

  for (guint i = 0; i < child_count; i++)
    {
      TSNode child = ts_node_named_child (self->node, i);

      if (is_valid_symbol_node (child))
        {
          g_autoptr(PluginTreeSitterSymbol) symbol = NULL;

          symbol = plugin_tree_sitter_symbol_new (self->parsed_tree, child);
          g_list_store_append (store, symbol);
        }
    }

  return dex_future_new_take_object (G_LIST_MODEL (g_steal_pointer (&store)));
}

static DexFuture *
plugin_tree_sitter_symbol_list_children (FoundrySymbol *symbol)
{
  PluginTreeSitterSymbol *self = PLUGIN_TREE_SITTER_SYMBOL (symbol);

  g_return_val_if_fail (PLUGIN_IS_TREE_SITTER_SYMBOL (self), NULL);

  return dex_scheduler_spawn (NULL, 0,
                              plugin_tree_sitter_symbol_list_children_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static void
plugin_tree_sitter_symbol_class_init (PluginTreeSitterSymbolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundrySymbolClass *symbol_class = FOUNDRY_SYMBOL_CLASS (klass);

  object_class->finalize = plugin_tree_sitter_symbol_finalize;

  symbol_class->dup_name = plugin_tree_sitter_symbol_dup_name;
  symbol_class->find_parent = plugin_tree_sitter_symbol_find_parent;
  symbol_class->list_children = plugin_tree_sitter_symbol_list_children;
}

static void
plugin_tree_sitter_symbol_init (PluginTreeSitterSymbol *self)
{
}

PluginTreeSitterSymbol *
plugin_tree_sitter_symbol_new (ParsedTree *parsed_tree,
                                TSNode     node)
{
  PluginTreeSitterSymbol *self;

  g_return_val_if_fail (parsed_tree != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_TREE_SITTER_SYMBOL, NULL);
  self->parsed_tree = parsed_tree_ref (parsed_tree);
  self->node = node;

  return self;
}
