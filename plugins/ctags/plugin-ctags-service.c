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
  GListStore *files;
};

G_DEFINE_FINAL_TYPE (PluginCtagsService, plugin_ctags_service, FOUNDRY_TYPE_SERVICE)

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

  object_class->finalize = plugin_ctags_service_finalize;
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
