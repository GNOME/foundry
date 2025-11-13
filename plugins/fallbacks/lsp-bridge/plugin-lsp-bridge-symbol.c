/* plugin-lsp-bridge-symbol.c
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

#include "plugin-lsp-bridge-symbol.h"

struct _PluginLspBridgeSymbol
{
  FoundrySymbol  parent_instance;
  GFile         *file;
  JsonNode      *node;
};

G_DEFINE_FINAL_TYPE (PluginLspBridgeSymbol, plugin_lsp_bridge_symbol, FOUNDRY_TYPE_SYMBOL)

static GIcon *text_x_generic_icon;
static GIcon *lang_class_icon;
static GIcon *lang_method_icon;
static GIcon *lang_property_icon;
static GIcon *lang_enum_icon;
static GIcon *lang_function_icon;
static GIcon *lang_constant_icon;
static GIcon *lang_struct_icon;

static void
plugin_lsp_bridge_symbol_dispose (GObject *object)
{
  PluginLspBridgeSymbol *self = PLUGIN_LSP_BRIDGE_SYMBOL (object);

  g_clear_object (&self->file);
  g_clear_pointer (&self->node, json_node_unref);

  G_OBJECT_CLASS (plugin_lsp_bridge_symbol_parent_class)->dispose (object);
}

static GIcon *
get_icon_for_kind (gint64 kind)
{
  switch (kind)
    {
    case 1:  /* File */
      return g_object_ref (text_x_generic_icon);

    case 2:  /* Module */
      return NULL;

    case 3:  /* Namespace */
      return NULL;

    case 4:  /* Package */
      return NULL;

    case 5:  /* Class */
      return g_object_ref (lang_class_icon);

    case 6:  /* Method */
      return g_object_ref (lang_method_icon);

    case 7:  /* Property */
      return g_object_ref (lang_property_icon);

    case 8:  /* Field */
      return NULL;

    case 9:  /* Constructor */
      return NULL;

    case 10: /* Enum */
      return g_object_ref (lang_enum_icon);

    case 11: /* Interface */
      return NULL;

    case 12: /* Function */
      return g_object_ref (lang_function_icon);

    case 13: /* Variable */
      return NULL;

    case 14: /* Constant */
      return g_object_ref (lang_constant_icon);

    case 15: /* String */
    case 16: /* Number */
    case 17: /* Boolean */
    case 18: /* Array */
    case 19: /* Object */
    case 20: /* Key */
    case 21: /* Null */
      return NULL;

    case 22: /* EnumMember */
      return NULL;

    case 23: /* Struct */
      return g_object_ref (lang_struct_icon);

    case 24: /* Event */
      return NULL;

    case 25: /* Operator */
      return NULL;

    case 26: /* TypeParameter */
      return NULL;

    default:
      return NULL;
    }
}

static char *
plugin_lsp_bridge_symbol_dup_name (FoundrySymbol *symbol)
{
  PluginLspBridgeSymbol *self = PLUGIN_LSP_BRIDGE_SYMBOL (symbol);
  const char *name = NULL;

  g_assert (PLUGIN_IS_LSP_BRIDGE_SYMBOL (self));

  if (!FOUNDRY_JSON_OBJECT_PARSE (self->node, "name", FOUNDRY_JSON_NODE_GET_STRING (&name)))
    return NULL;

  return g_strdup (name);
}

static FoundrySymbolLocator *
plugin_lsp_bridge_symbol_dup_locator (FoundrySymbol *symbol)
{
  PluginLspBridgeSymbol *self = PLUGIN_LSP_BRIDGE_SYMBOL (symbol);
  JsonNode *selection_range_node = NULL;
  JsonNode *range_node = NULL;
  guint selection_start_line = 0;
  guint selection_start_line_offset = 0;

  g_assert (PLUGIN_IS_LSP_BRIDGE_SYMBOL (self));

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "selectionRange", FOUNDRY_JSON_NODE_GET_NODE (&selection_range_node)))
    {
      JsonNode *start_node = NULL;
      gint64 start_line_int = 0;
      gint64 start_line_offset_int = 0;

      if (FOUNDRY_JSON_OBJECT_PARSE (selection_range_node,
                                     "start", FOUNDRY_JSON_NODE_GET_NODE (&start_node)) &&
          FOUNDRY_JSON_OBJECT_PARSE (start_node,
                                     "line", FOUNDRY_JSON_NODE_GET_INT (&start_line_int),
                                     "character", FOUNDRY_JSON_NODE_GET_INT (&start_line_offset_int)))
        {
          selection_start_line = CLAMP (start_line_int, 0, G_MAXUINT);
          selection_start_line_offset = CLAMP (start_line_offset_int, 0, G_MAXUINT);
        }
    }
  else if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "range", FOUNDRY_JSON_NODE_GET_NODE (&range_node)))
    {
      JsonNode *start_node = NULL;
      gint64 start_line_int = 0;
      gint64 start_line_offset_int = 0;

      if (FOUNDRY_JSON_OBJECT_PARSE (range_node,
                                     "start", FOUNDRY_JSON_NODE_GET_NODE (&start_node)) &&
          FOUNDRY_JSON_OBJECT_PARSE (start_node,
                                     "line", FOUNDRY_JSON_NODE_GET_INT (&start_line_int),
                                     "character", FOUNDRY_JSON_NODE_GET_INT (&start_line_offset_int)))
        {
          selection_start_line = CLAMP (start_line_int, 0, G_MAXUINT);
          selection_start_line_offset = CLAMP (start_line_offset_int, 0, G_MAXUINT);
        }
    }

  return foundry_symbol_locator_new_for_file_and_line_offset (self->file,
                                                              selection_start_line,
                                                              selection_start_line_offset);
}

static GIcon *
plugin_lsp_bridge_symbol_dup_icon (FoundrySymbol *symbol)
{
  PluginLspBridgeSymbol *self = PLUGIN_LSP_BRIDGE_SYMBOL (symbol);
  gint64 kind = 0;
  GIcon *icon = NULL;

  g_return_val_if_fail (PLUGIN_IS_LSP_BRIDGE_SYMBOL (self), NULL);

  if (!FOUNDRY_JSON_OBJECT_PARSE (self->node, "kind", FOUNDRY_JSON_NODE_GET_INT (&kind)))
    return NULL;

  icon = get_icon_for_kind (kind);

  if (icon == NULL)
    return NULL;

  return icon;
}

static DexFuture *
plugin_lsp_bridge_symbol_find_parent (FoundrySymbol *symbol)
{
  g_return_val_if_fail (PLUGIN_IS_LSP_BRIDGE_SYMBOL (symbol), NULL);

  return foundry_future_new_not_supported ();
}

static DexFuture *
plugin_lsp_bridge_symbol_list_children (FoundrySymbol *symbol)
{
  PluginLspBridgeSymbol *self = PLUGIN_LSP_BRIDGE_SYMBOL (symbol);
  JsonNode *children_node = NULL;
  JsonArray *children_array;
  GListStore *store;
  guint n_children;

  g_return_val_if_fail (PLUGIN_IS_LSP_BRIDGE_SYMBOL (self), NULL);

  if (!FOUNDRY_JSON_OBJECT_PARSE (self->node, "children", FOUNDRY_JSON_NODE_GET_NODE (&children_node)) ||
      !JSON_NODE_HOLDS_ARRAY (children_node) ||
      (children_array = json_node_get_array (children_node)) == NULL)
    return foundry_future_new_not_supported ();

  store = g_list_store_new (FOUNDRY_TYPE_SYMBOL);

  n_children = json_array_get_length (children_array);

  for (guint i = 0; i < n_children; i++)
    {
      JsonNode *child_node = json_array_get_element (children_array, i);
      g_autoptr(PluginLspBridgeSymbol) child = NULL;

      child = plugin_lsp_bridge_symbol_new (self->file, child_node);

      if (child != NULL)
        g_list_store_append (store, FOUNDRY_SYMBOL (child));
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static gboolean
plugin_lsp_bridge_symbol_has_children (FoundrySymbol *symbol)
{
  PluginLspBridgeSymbol *self = PLUGIN_LSP_BRIDGE_SYMBOL (symbol);
  JsonNode *children_node = NULL;
  JsonArray *children_array = NULL;

  g_return_val_if_fail (PLUGIN_IS_LSP_BRIDGE_SYMBOL (self), FALSE);

  if (!FOUNDRY_JSON_OBJECT_PARSE (self->node, "children", FOUNDRY_JSON_NODE_GET_NODE (&children_node)) ||
      !JSON_NODE_HOLDS_ARRAY (children_node) ||
      (children_array = json_node_get_array (children_node)) == NULL)
    return FALSE;

  return json_array_get_length (children_array) > 0;
}

static void
plugin_lsp_bridge_symbol_class_init (PluginLspBridgeSymbolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundrySymbolClass *symbol_class = FOUNDRY_SYMBOL_CLASS (klass);

  object_class->dispose = plugin_lsp_bridge_symbol_dispose;

  symbol_class->dup_name = plugin_lsp_bridge_symbol_dup_name;
  symbol_class->dup_locator = plugin_lsp_bridge_symbol_dup_locator;
  symbol_class->dup_icon = plugin_lsp_bridge_symbol_dup_icon;
  symbol_class->find_parent = plugin_lsp_bridge_symbol_find_parent;
  symbol_class->list_children = plugin_lsp_bridge_symbol_list_children;
  symbol_class->has_children = plugin_lsp_bridge_symbol_has_children;

  text_x_generic_icon = g_themed_icon_new ("text-x-generic-symbolic");
  lang_class_icon = g_themed_icon_new ("lang-class-symbolic");
  lang_method_icon = g_themed_icon_new ("lang-method-symbolic");
  lang_property_icon = g_themed_icon_new ("lang-property-symbolic");
  lang_enum_icon = g_themed_icon_new ("lang-enum-symbolic");
  lang_function_icon = g_themed_icon_new ("lang-function-symbolic");
  lang_constant_icon = g_themed_icon_new ("lang-constant-symbolic");
  lang_struct_icon = g_themed_icon_new ("lang-struct-symbolic");
}

static void
plugin_lsp_bridge_symbol_init (PluginLspBridgeSymbol *self)
{
}

static gboolean
parse_range (JsonNode *node,
             guint    *start_line,
             guint    *start_line_offset,
             guint    *end_line,
             guint    *end_line_offset)
{
  JsonNode *start_node = NULL;
  JsonNode *end_node = NULL;
  gint64 start_line_int = 0;
  gint64 start_line_offset_int = 0;
  gint64 end_line_int = 0;
  gint64 end_line_offset_int = 0;

  if (!FOUNDRY_JSON_OBJECT_PARSE (node,
                                  "start", FOUNDRY_JSON_NODE_GET_NODE (&start_node),
                                  "end", FOUNDRY_JSON_NODE_GET_NODE (&end_node)))
    return FALSE;

  if (!FOUNDRY_JSON_OBJECT_PARSE (start_node,
                                  "line", FOUNDRY_JSON_NODE_GET_INT (&start_line_int),
                                  "character", FOUNDRY_JSON_NODE_GET_INT (&start_line_offset_int)))
    return FALSE;

  if (!FOUNDRY_JSON_OBJECT_PARSE (end_node,
                                  "line", FOUNDRY_JSON_NODE_GET_INT (&end_line_int),
                                  "character", FOUNDRY_JSON_NODE_GET_INT (&end_line_offset_int)))
    return FALSE;

  *start_line = (guint)CLAMP (start_line_int, 0, G_MAXUINT);
  *start_line_offset = (guint)CLAMP (start_line_offset_int, 0, G_MAXUINT);
  *end_line = (guint)CLAMP (end_line_int, 0, G_MAXUINT);
  *end_line_offset = (guint)CLAMP (end_line_offset_int, 0, G_MAXUINT);

  return TRUE;
}

static PluginLspBridgeSymbol *
parse_document_symbol (GFile    *file,
                       JsonNode *node)
{
  PluginLspBridgeSymbol *self;
  const char *name = NULL;
  gint64 kind = 0;

  g_assert (G_IS_FILE (file));
  g_assert (node != NULL);

  if (!FOUNDRY_JSON_OBJECT_PARSE (node,
                                  "name", FOUNDRY_JSON_NODE_GET_STRING (&name),
                                  "kind", FOUNDRY_JSON_NODE_GET_INT (&kind)))
    return NULL;

  self = g_object_new (PLUGIN_TYPE_LSP_BRIDGE_SYMBOL, NULL);
  self->file = g_object_ref (file);
  self->node = json_node_ref (node);

  return self;
}

PluginLspBridgeSymbol *
plugin_lsp_bridge_symbol_new (GFile    *file,
                               JsonNode *node)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (node != NULL, NULL);

  return parse_document_symbol (file, node);
}

gboolean
plugin_lsp_bridge_symbol_contains_position (PluginLspBridgeSymbol *self,
                                            guint                  line,
                                            guint                  line_offset)
{
  JsonNode *range_node = NULL;
  guint range_start_line = 0;
  guint range_start_line_offset = 0;
  guint range_end_line = 0;
  guint range_end_line_offset = 0;

  g_return_val_if_fail (PLUGIN_IS_LSP_BRIDGE_SYMBOL (self), FALSE);

  if (!FOUNDRY_JSON_OBJECT_PARSE (self->node, "range", FOUNDRY_JSON_NODE_GET_NODE (&range_node)))
    return FALSE;

  if (!parse_range (range_node,
                    &range_start_line,
                    &range_start_line_offset,
                    &range_end_line,
                    &range_end_line_offset))
    return FALSE;

  if (line < range_start_line || line > range_end_line)
    return FALSE;

  if (line == range_start_line && line_offset < range_start_line_offset)
    return FALSE;

  if (line == range_end_line && line_offset > range_end_line_offset)
    return FALSE;

  return TRUE;
}

PluginLspBridgeSymbol *
plugin_lsp_bridge_symbol_find_at_position (PluginLspBridgeSymbol *self,
                                           guint                  line,
                                           guint                  line_offset)
{
  JsonNode *children_node = NULL;
  JsonArray *children_array = NULL;

  g_return_val_if_fail (PLUGIN_IS_LSP_BRIDGE_SYMBOL (self), NULL);

  if (!plugin_lsp_bridge_symbol_contains_position (self, line, line_offset))
    return NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "children", FOUNDRY_JSON_NODE_GET_NODE (&children_node)) &&
      JSON_NODE_HOLDS_ARRAY (children_node) &&
      (children_array = json_node_get_array (children_node)) != NULL)
    {
      guint n_items = json_array_get_length (children_array);

      for (guint i = 0; i < n_items; i++)
        {
          JsonNode *child_node = json_array_get_element (children_array, i);
          g_autoptr(PluginLspBridgeSymbol) child = NULL;
          g_autoptr(PluginLspBridgeSymbol) found = NULL;

          child = plugin_lsp_bridge_symbol_new (self->file, child_node);

          if (child != NULL &&
              (found = plugin_lsp_bridge_symbol_find_at_position (child, line, line_offset)))
            return g_steal_pointer (&found);
        }
    }

  return g_object_ref (self);
}
