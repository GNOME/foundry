/*
 * plugin-devhelp-importer.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include "plugin-devhelp-importer.h"

G_DEFINE_ABSTRACT_TYPE (PluginDevhelpImporter, plugin_devhelp_importer, G_TYPE_OBJECT)

static void
plugin_devhelp_importer_class_init (PluginDevhelpImporterClass *klass)
{
}

static void
plugin_devhelp_importer_init (PluginDevhelpImporter *self)
{
}

DexFuture *
plugin_devhelp_importer_import (PluginDevhelpImporter   *self,
                                PluginDevhelpRepository *repository,
                                PluginDevhelpProgress   *progress)
{
  g_return_val_if_fail (PLUGIN_IS_DEVHELP_IMPORTER (self), NULL);
  g_return_val_if_fail (PLUGIN_IS_DEVHELP_REPOSITORY (repository), NULL);
  g_return_val_if_fail (PLUGIN_IS_DEVHELP_PROGRESS (progress), NULL);

  return PLUGIN_DEVHELP_IMPORTER_GET_CLASS (self)->import (self, repository, progress);
}
