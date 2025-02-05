/* plugin-flatpak-config.h
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <foundry.h>

G_BEGIN_DECLS

#define PLUGIN_TYPE_FLATPAK_CONFIG (plugin_flatpak_config_get_type())

FOUNDRY_DECLARE_INTERNAL_TYPE (PluginFlatpakConfig, plugin_flatpak_config, PLUGIN, FLATPAK_CONFIG, FoundryConfig)

GFile *plugin_flatpak_config_dup_file                (PluginFlatpakConfig *self);
char  *plugin_flatpak_config_dup_id                  (PluginFlatpakConfig *self);
char  *plugin_flatpak_config_dup_primary_module_name (PluginFlatpakConfig *self);
char  *plugin_flatpak_config_dup_runtime             (PluginFlatpakConfig *self);
char  *plugin_flatpak_config_dup_runtime_version     (PluginFlatpakConfig *self);
char  *plugin_flatpak_config_dup_sdk                 (PluginFlatpakConfig *self);

G_END_DECLS
