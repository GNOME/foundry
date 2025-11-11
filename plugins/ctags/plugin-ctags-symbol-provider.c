/* plugin-ctags-symbol-provider.c
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

#include "plugin-ctags-file.h"
#include "plugin-ctags-service.h"
#include "plugin-ctags-symbol-provider.h"
#include "plugin-ctags-symbol.h"

struct _PluginCtagsSymbolProvider
{
  FoundrySymbolProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginCtagsSymbolProvider, plugin_ctags_symbol_provider, FOUNDRY_TYPE_SYMBOL_PROVIDER)

static void
plugin_ctags_symbol_provider_finalize (GObject *object)
{
  G_OBJECT_CLASS (plugin_ctags_symbol_provider_parent_class)->finalize (object);
}

static DexFuture *
plugin_ctags_symbol_provider_find_symbol_at_fiber (PluginCtagsSymbolProvider *self,
                                                   GFile                     *file,
                                                   GBytes                    *contents,
                                                   guint                      line,
                                                   guint                      line_offset)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(PluginCtagsService) service = NULL;
  g_autoptr(PluginCtagsFile) index_file = NULL;
  g_autoptr(GError) error = NULL;
  PluginCtagsMatch match;
  gsize match_count;
  g_autoptr(PluginCtagsSymbol) symbol = NULL;

  g_assert (PLUGIN_IS_CTAGS_SYMBOL_PROVIDER (self));
  g_assert (G_IS_FILE (file));

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(service = foundry_context_dup_service_typed (context, PLUGIN_TYPE_CTAGS_SERVICE)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "PluginCtagsService not available");

  if (!(index_file = dex_await_object (plugin_ctags_service_index (service, file, contents), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  match_count = plugin_ctags_file_find_matches_at (index_file, NULL, line, line_offset, &match, 1);

  if (match_count == 0)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "No symbol found at line %u, offset %u",
                                  line, line_offset);

  symbol = plugin_ctags_symbol_new (index_file, &match);

  return dex_future_new_take_object (g_steal_pointer (&symbol));
}

static DexFuture *
plugin_ctags_symbol_provider_list_symbols_fiber (PluginCtagsSymbolProvider *self,
                                                 GFile                     *file,
                                                 GBytes                    *contents)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(PluginCtagsService) service = NULL;
  g_autoptr(PluginCtagsFile) index_file = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autofree char *file_path = NULL;
  gsize size;

  g_assert (PLUGIN_IS_CTAGS_SYMBOL_PROVIDER (self));
  g_assert (G_IS_FILE (file));

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(service = foundry_context_dup_service_typed (context, PLUGIN_TYPE_CTAGS_SERVICE)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "PluginCtagsService not available");

  if (!(index_file = dex_await_object (plugin_ctags_service_index (service, file, contents), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  store = g_list_store_new (PLUGIN_TYPE_CTAGS_SYMBOL);
  file_path = g_file_get_path (file);
  size = plugin_ctags_file_get_size (index_file);

  for (gsize i = 0; i < size; i++)
    {
      PluginCtagsKind kind;
      gboolean is_toplevel_type;

      kind = plugin_ctags_file_get_kind (index_file, i);

      /* Filter for toplevel types */
      is_toplevel_type = (kind == PLUGIN_CTAGS_KIND_CLASS_NAME ||
                          kind == PLUGIN_CTAGS_KIND_UNION ||
                          kind == PLUGIN_CTAGS_KIND_STRUCTURE ||
                          kind == PLUGIN_CTAGS_KIND_TYPEDEF ||
                          kind == PLUGIN_CTAGS_KIND_ENUMERATION_NAME ||
                          kind == PLUGIN_CTAGS_KIND_FUNCTION);

      if (!is_toplevel_type)
        continue;

      /* All items will match our file because we indexed just this file */

      /* Create symbol and add to store */
      {
        PluginCtagsMatch match;
        gsize name_len;
        gsize path_len;
        gsize pattern_len;
        gsize kv_len;
        g_autoptr(PluginCtagsSymbol) symbol = NULL;

        plugin_ctags_file_peek_name (index_file, i, &match.name, &name_len);
        plugin_ctags_file_peek_path (index_file, i, &match.path, &path_len);
        plugin_ctags_file_peek_pattern (index_file, i, &match.pattern, &pattern_len);
        plugin_ctags_file_peek_keyval (index_file, i, &match.kv, &kv_len);
        match.name_len = (guint16)name_len;
        match.path_len = (guint16)path_len;
        match.pattern_len = (guint16)pattern_len;
        match.kv_len = (guint16)kv_len;
        match.kind = plugin_ctags_file_get_kind (index_file, i);

        symbol = plugin_ctags_symbol_new (index_file, &match);

        g_list_store_append (store, symbol);
      }
    }

  return dex_future_new_take_object (G_LIST_MODEL (g_steal_pointer (&store)));
}

static DexFuture *
plugin_ctags_symbol_provider_list_symbols (FoundrySymbolProvider *symbol_provider,
                                           GFile                 *file,
                                           GBytes                *contents)
{
  PluginCtagsSymbolProvider *self = PLUGIN_CTAGS_SYMBOL_PROVIDER (symbol_provider);

  g_return_val_if_fail (PLUGIN_IS_CTAGS_SYMBOL_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_ctags_symbol_provider_list_symbols_fiber),
                                  3,
                                  PLUGIN_TYPE_CTAGS_SYMBOL_PROVIDER, self,
                                  G_TYPE_FILE, file,
                                  G_TYPE_BYTES, contents);
}

static DexFuture *
plugin_ctags_symbol_provider_find_symbol_at (FoundrySymbolProvider *symbol_provider,
                                             GFile                 *file,
                                             GBytes                *contents,
                                             guint                  line,
                                             guint                  line_offset)
{
  PluginCtagsSymbolProvider *self = PLUGIN_CTAGS_SYMBOL_PROVIDER (symbol_provider);

  g_return_val_if_fail (PLUGIN_IS_CTAGS_SYMBOL_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_ctags_symbol_provider_find_symbol_at_fiber),
                                  5,
                                  PLUGIN_TYPE_CTAGS_SYMBOL_PROVIDER, self,
                                  G_TYPE_FILE, file,
                                  G_TYPE_BYTES, contents,
                                  G_TYPE_UINT, line,
                                  G_TYPE_UINT, line_offset);
}

static void
plugin_ctags_symbol_provider_class_init (PluginCtagsSymbolProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundrySymbolProviderClass *provider_class = FOUNDRY_SYMBOL_PROVIDER_CLASS (klass);

  object_class->finalize = plugin_ctags_symbol_provider_finalize;

  provider_class->list_symbols = plugin_ctags_symbol_provider_list_symbols;
  provider_class->find_symbol_at = plugin_ctags_symbol_provider_find_symbol_at;
}

static void
plugin_ctags_symbol_provider_init (PluginCtagsSymbolProvider *self)
{
}
