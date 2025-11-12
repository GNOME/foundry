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

struct _PluginLspBridgeSymbolProvider
{
  FoundrySymbolProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginLspBridgeSymbolProvider, plugin_lsp_bridge_symbol_provider, FOUNDRY_TYPE_SYMBOL_PROVIDER)

static DexFuture *
plugin_lsp_bridge_symbol_provider_list_symbols_fiber (FoundrySymbolProvider *self,
                                                      GFile                 *file,
                                                      GBytes                *contents)
{
  g_assert (PLUGIN_IS_LSP_BRIDGE_SYMBOL_PROVIDER (self));
  g_assert (G_IS_FILE (file));

  return foundry_future_new_not_supported ();
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
plugin_lsp_bridge_symbol_provider_find_symbol_at_fiber (FoundrySymbolProvider *self,
                                                        GFile                 *file,
                                                        GBytes                *contents,
                                                        guint                  line,
                                                        guint                  line_offset)
{
  g_assert (PLUGIN_IS_LSP_BRIDGE_SYMBOL_PROVIDER (self));
  g_assert (G_IS_FILE (file));

  return foundry_future_new_not_supported ();
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
