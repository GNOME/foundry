/* plugin-file-search-service.c
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

#include "plugin-file-search-results.h"
#include "plugin-file-search-service.h"

#include "foundry-fuzzy-index-private.h"

struct _PluginFileSearchService
{
  FoundryService  parent_instance;
  DexFuture      *index;
};

G_DEFINE_FINAL_TYPE (PluginFileSearchService, plugin_file_search_service, FOUNDRY_TYPE_SERVICE)

static DexFuture *
plugin_file_search_service_load_index_fiber (gpointer user_data)
{
  PluginFileSearchService *self = user_data;
  g_autoptr(FoundryFuzzyIndex) fuzzy = NULL;
  g_autoptr(FoundryVcsManager) vcs_manager = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryVcs) vcs = NULL;
  g_autoptr(GListModel) files = NULL;
  g_autoptr(GError) error = NULL;
  guint n_items;

  g_assert (PLUGIN_IS_FILE_SEARCH_SERVICE (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  vcs_manager = foundry_context_dup_vcs_manager (context);

  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (vcs_manager)), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  vcs = foundry_vcs_manager_dup_vcs (vcs_manager);

  if (!(files = dex_await_object (foundry_vcs_list_files (vcs), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  n_items = g_list_model_get_n_items (files);
  fuzzy = foundry_fuzzy_index_new (FALSE);

  foundry_fuzzy_index_begin_bulk_insert (fuzzy);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryVcsFile) file = g_list_model_get_item (files, i);
      g_autofree char *relative_path = foundry_vcs_file_dup_relative_path (file);

      foundry_fuzzy_index_insert (fuzzy, relative_path, NULL);
    }

  foundry_fuzzy_index_end_bulk_insert (fuzzy);

  return dex_future_new_take_boxed (FOUNDRY_TYPE_FUZZY_INDEX, g_steal_pointer (&fuzzy));
}

static DexFuture *
plugin_file_search_service_load_index (PluginFileSearchService *self)
{
  g_assert (PLUGIN_IS_FILE_SEARCH_SERVICE (self));

  if (self->index == NULL)
    self->index = dex_scheduler_spawn (NULL, 0,
                                       plugin_file_search_service_load_index_fiber,
                                       g_object_ref (self),
                                       g_object_unref);

  return dex_ref (self->index);
}

static DexFuture *
plugin_file_search_service_stop (FoundryService *service)
{
  PluginFileSearchService *self = PLUGIN_FILE_SEARCH_SERVICE (service);

  dex_clear (&self->index);

  return dex_future_new_true ();
}

static void
plugin_file_search_service_finalize (GObject *object)
{
  PluginFileSearchService *self = (PluginFileSearchService *)object;

  dex_clear (&self->index);

  G_OBJECT_CLASS (plugin_file_search_service_parent_class)->finalize (object);
}

static void
plugin_file_search_service_class_init (PluginFileSearchServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  object_class->finalize = plugin_file_search_service_finalize;

  service_class->stop = plugin_file_search_service_stop;
}

static void
plugin_file_search_service_init (PluginFileSearchService *self)
{
}

static DexFuture *
plugin_file_search_service_query_cb (DexFuture *future,
                                     gpointer   user_data)
{
  g_autoptr(FoundryFuzzyIndex) fuzzy = NULL;
  const char *search_text = user_data;
  g_autoptr(GString) delimited = NULL;
  g_autoptr(GArray) ar = NULL;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (dex_future_is_resolved (future));
  g_assert (search_text != NULL);

  delimited = g_string_new (NULL);

  for (const char *iter = search_text;
       *iter;
       iter = g_utf8_next_char (iter))
    {
      gunichar ch = g_utf8_get_char (iter);

      if (!g_unichar_isspace (ch))
        g_string_append_unichar (delimited, ch);
    }

  fuzzy = dex_await_boxed (dex_ref (future), NULL);
  ar = foundry_fuzzy_index_match (fuzzy, delimited->str, 0);

  return dex_future_new_take_object (plugin_file_search_results_new (g_steal_pointer (&fuzzy),
                                                                     g_steal_pointer (&ar)));
}

/**
 * plugin_file_search_service_query:
 * @self: a [class@Plugin.FileSearchService]
 * @search_text: the text to query
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [iface@Gio.ListModel] of [class@Foundry.SearchResult] or rejects with error.
 */
DexFuture *
plugin_file_search_service_query (PluginFileSearchService *self,
                                  const char              *search_text)
{
  DexFuture *future;

  dex_return_error_if_fail (PLUGIN_IS_FILE_SEARCH_SERVICE (self));
  dex_return_error_if_fail (search_text != NULL);

  future = plugin_file_search_service_load_index (self);
  future = dex_future_then (future,
                            plugin_file_search_service_query_cb,
                            g_strdup (search_text),
                            g_free);

  return future;
}
