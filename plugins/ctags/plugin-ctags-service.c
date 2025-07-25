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
  FoundryService  parent_instance;
  GListStore     *files;
};

G_DEFINE_FINAL_TYPE (PluginCtagsService, plugin_ctags_service, FOUNDRY_TYPE_SERVICE)

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

static void
plugin_ctags_service_finalize (GObject *object)
{
  PluginCtagsService *self = (PluginCtagsService *)object;

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
