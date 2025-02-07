/* plugin-flatpak-options.h
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

#include "plugin-flatpak-serializable.h"

G_BEGIN_DECLS

#define PLUGIN_TYPE_FLATPAK_OPTIONS (plugin_flatpak_options_get_type())

G_DECLARE_FINAL_TYPE (PluginFlatpakOptions, plugin_flatpak_options, PLUGIN, FLATPAK_OPTIONS, PluginFlatpakSerializable)

char **plugin_flatpak_options_dup_build_args   (PluginFlatpakOptions *self);
char  *plugin_flatpak_options_dup_append_path  (PluginFlatpakOptions *self);
char  *plugin_flatpak_options_dup_prepend_path (PluginFlatpakOptions *self);

G_END_DECLS
