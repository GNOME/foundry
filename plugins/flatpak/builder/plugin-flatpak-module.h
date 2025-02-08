/* plugin-flatpak-module.h
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

#pragma once

#include "plugin-flatpak-options.h"
#include "plugin-flatpak-serializable.h"
#include "plugin-flatpak-sources.h"

G_BEGIN_DECLS

typedef struct _PluginFlatpakModules PluginFlatpakModules;

#define PLUGIN_TYPE_FLATPAK_MODULE (plugin_flatpak_module_get_type())

G_DECLARE_FINAL_TYPE (PluginFlatpakModule, plugin_flatpak_module, PLUGIN, FLATPAK_MODULE, PluginFlatpakSerializable)

PluginFlatpakSources  *plugin_flatpak_module_dup_sources       (PluginFlatpakModule *self);
PluginFlatpakModules  *plugin_flatpak_module_dup_modules       (PluginFlatpakModule *self);
char                  *plugin_flatpak_module_dup_name          (PluginFlatpakModule *self);
char                  *plugin_flatpak_module_dup_buildsystem   (PluginFlatpakModule *self);
PluginFlatpakOptions  *plugin_flatpak_module_dup_build_options (PluginFlatpakModule *self);
char                 **plugin_flatpak_module_dup_config_opts   (PluginFlatpakModule *self);

G_END_DECLS
