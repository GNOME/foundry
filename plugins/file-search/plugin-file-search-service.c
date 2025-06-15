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

#include "plugin-file-search-service.h"

#include "foundry-fuzzy-index-private.h"

struct _PluginFileSearchService
{
  FoundryService     parent_instance;
  FoundryFuzzyIndex *index;
};

G_DEFINE_FINAL_TYPE (PluginFileSearchService, plugin_file_search_service, FOUNDRY_TYPE_SERVICE)

static DexFuture *
plugin_file_search_service_start_fiber (gpointer user_data)
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

  self->index = foundry_fuzzy_index_ref (fuzzy);

  return dex_future_new_true ();
}

static DexFuture *
plugin_file_search_service_start (FoundryService *service)
{
  g_assert (PLUGIN_IS_FILE_SEARCH_SERVICE (service));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_file_search_service_start_fiber,
                              g_object_ref (service),
                              g_object_unref);
}

static DexFuture *
plugin_file_search_service_stop (FoundryService *service)
{
  PluginFileSearchService *self = PLUGIN_FILE_SEARCH_SERVICE (service);

  g_clear_pointer (&self->index, foundry_fuzzy_index_unref);

  return dex_future_new_true ();
}

static void
plugin_file_search_service_finalize (GObject *object)
{
  G_OBJECT_CLASS (plugin_file_search_service_parent_class)->finalize (object);
}

static void
plugin_file_search_service_class_init (PluginFileSearchServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  object_class->finalize = plugin_file_search_service_finalize;

  service_class->start = plugin_file_search_service_start;
  service_class->stop= plugin_file_search_service_stop;
}

static void
plugin_file_search_service_init (PluginFileSearchService *self)
{
}
