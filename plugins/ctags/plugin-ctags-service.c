/* plugin-ctags-service.c
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

struct _PluginCtagsService
{
  FoundryService parent_instance;

  /* A GListModel of PluginCtagsFile which we use to perform queries
   * against the ctags indexes.
   */
  GListStore *files;

  /* A future that is a DexFiber doing the mining of new ctags data.
   * It looks through the project to find directories which have newer
   * data than their respective tags file.
   *
   * If found, it generates the tags for that directory.
   */
  DexFuture *miner;
};

G_DEFINE_FINAL_TYPE (PluginCtagsService, plugin_ctags_service, FOUNDRY_TYPE_SERVICE)

static gboolean
mine_directory_recursive (GFile      *sources_dir,
                          GFile      *tags_dir,
                          const char *ctags)
{
  g_assert (G_IS_FILE (sources_dir));
  g_assert (G_IS_FILE (tags_dir));
  g_assert (!foundry_str_empty0 (ctags));

  return FALSE;
}

static DexFuture *
plugin_ctags_service_miner_fiber (gpointer data)
{
  PluginCtagsService *self = data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GSettings) settings = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GFile) tagsdir = NULL;
  g_autofree char *ctags = NULL;

  g_assert (PLUGIN_IS_CTAGS_SERVICE (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  workdir = foundry_context_dup_project_directory (context);
  tagsdir = foundry_context_cache_file (context, "ctags", NULL);
  settings = g_settings_new ("app.devsuite.foundry.ctags");
  ctags = g_settings_get_string (settings, "path");

  if (foundry_str_empty0 (ctags))
    g_set_str (&ctags, "ctags");

  mine_directory_recursive (workdir, tagsdir, ctags);

  return dex_future_new_true ();
}

static DexFuture *
plugin_ctags_service_start_fiber (gpointer data)
{
  PluginCtagsService *self = data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GPtrArray) files = NULL;
  g_autoptr(GFile) ctags_file = NULL;
  g_autofree char *ctags_dir = NULL;

  g_assert (PLUGIN_IS_CTAGS_SERVICE (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  ctags_dir = foundry_context_cache_filename (context, "ctags", NULL);
  ctags_file = g_file_new_for_path (ctags_dir);

  if ((files = dex_await_boxed (foundry_file_find_with_depth (ctags_file, "tags", 10), NULL)))
    {
      g_autoptr(GPtrArray) futures = g_ptr_array_new_with_free_func (dex_unref);

      for (guint i = 0; i < files->len; i++)
        {
          GFile *file = g_ptr_array_index (files, i);

          g_ptr_array_add (futures, plugin_ctags_file_new (file));
        }

      if (futures->len > 0)
        dex_await (foundry_future_all (futures), NULL);

      for (guint i = 0; i < futures->len; i++)
        {
          DexFuture *future = g_ptr_array_index (futures, i);
          const GValue *value;

          if ((value = dex_future_get_value (future, NULL)))
            g_list_store_append (self->files, g_value_get_object (value));
        }
    }

  /* Now start the miner. We do not block startup on this because we
   * wouldn't want it to prevent shutdown of the service. So we create
   * a new fiber for the miner which may be discarded in stop(), and
   * thusly, potentially cancel the fiber.
   */
  self->miner = dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                                     plugin_ctags_service_miner_fiber,
                                     g_object_ref (self),
                                     g_object_unref);

  return dex_future_new_true ();
}

static DexFuture *
plugin_ctags_service_start (FoundryService *service)
{
  g_assert (PLUGIN_IS_CTAGS_SERVICE (service));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_ctags_service_start_fiber,
                              g_object_ref (service),
                              g_object_unref);
}

static DexFuture *
plugin_ctags_service_stop (FoundryService *service)
{
  PluginCtagsService *self = (PluginCtagsService *)service;

  g_assert (PLUGIN_IS_CTAGS_SERVICE (self));

  g_list_store_remove_all (self->files);
  dex_clear (&self->miner);

  return dex_future_new_true ();
}

static void
plugin_ctags_service_finalize (GObject *object)
{
  PluginCtagsService *self = (PluginCtagsService *)object;

  dex_clear (&self->miner);
  g_clear_object (&self->files);

  G_OBJECT_CLASS (plugin_ctags_service_parent_class)->finalize (object);
}

static void
plugin_ctags_service_class_init (PluginCtagsServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  object_class->finalize = plugin_ctags_service_finalize;

  service_class->start = plugin_ctags_service_start;
  service_class->stop = plugin_ctags_service_stop;
}

static void
plugin_ctags_service_init (PluginCtagsService *self)
{
  self->files = g_list_store_new (PLUGIN_TYPE_CTAGS_FILE);
}

/**
 * plugin_ctags_service_list_files:
 *
 * Returns: (transfer full):
 */
GListModel *
plugin_ctags_service_list_files (PluginCtagsService *self)
{
  g_return_val_if_fail (PLUGIN_IS_CTAGS_SERVICE (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->files));
}
