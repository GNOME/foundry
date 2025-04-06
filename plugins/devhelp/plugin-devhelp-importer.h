/*
 * plugin-devhelp-importer.h
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

#pragma once

#include <libdex.h>

#include "plugin-devhelp-repository.h"
#include "plugin-devhelp-progress.h"

G_BEGIN_DECLS

#define PLUGIN_TYPE_DEVHELP_IMPORTER (plugin_devhelp_importer_get_type())

G_DECLARE_DERIVABLE_TYPE (PluginDevhelpImporter, plugin_devhelp_importer, PLUGIN, DEVHELP_IMPORTER, GObject)

struct _PluginDevhelpImporterClass
{
  GObjectClass parent_class;

  DexFuture *(*import) (PluginDevhelpImporter   *self,
                        PluginDevhelpRepository *repository,
                        PluginDevhelpProgress   *progress);
};

DexFuture *plugin_devhelp_importer_import (PluginDevhelpImporter   *self,
                                           PluginDevhelpRepository *repository,
                                           PluginDevhelpProgress   *progress);

G_END_DECLS
