/* plugin-lsp-bridge-symbol.h
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

#pragma once

#include <foundry.h>

G_BEGIN_DECLS

#define PLUGIN_TYPE_LSP_BRIDGE_SYMBOL (plugin_lsp_bridge_symbol_get_type())

G_DECLARE_FINAL_TYPE (PluginLspBridgeSymbol, plugin_lsp_bridge_symbol, PLUGIN, LSP_BRIDGE_SYMBOL, FoundrySymbol)

PluginLspBridgeSymbol *plugin_lsp_bridge_symbol_new               (GFile                 *file,
                                                                   JsonNode              *node,
                                                                   JsonArray             *root_array);
gboolean               plugin_lsp_bridge_symbol_contains_position (PluginLspBridgeSymbol *self,
                                                                   guint                  line,
                                                                   guint                  line_offset);
PluginLspBridgeSymbol *plugin_lsp_bridge_symbol_find_at_position  (PluginLspBridgeSymbol *self,
                                                                   guint                  line,
                                                                   guint                  line_offset);

G_END_DECLS
