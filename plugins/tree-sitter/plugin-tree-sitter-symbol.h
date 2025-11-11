/* plugin-tree-sitter-symbol.h
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

#pragma once

#include <foundry.h>
#include <tree_sitter/api.h>

#include "parsed-tree.h"

G_BEGIN_DECLS

#define PLUGIN_TYPE_TREE_SITTER_SYMBOL (plugin_tree_sitter_symbol_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (PluginTreeSitterSymbol, plugin_tree_sitter_symbol, PLUGIN, TREE_SITTER_SYMBOL, FoundrySymbol)

FOUNDRY_AVAILABLE_IN_ALL
PluginTreeSitterSymbol *plugin_tree_sitter_symbol_new (ParsedTree *parsed_tree,
                                                        TSNode     node);

G_END_DECLS
