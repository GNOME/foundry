/* plugin-flatpak-manifest-loader.h
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

#include <libdex.h>

G_BEGIN_DECLS

#define PLUGIN_TYPE_FLATPAK_MANIFEST_LOADER (plugin_flatpak_manifest_loader_get_type())

G_DECLARE_FINAL_TYPE (PluginFlatpakManifestLoader, plugin_flatpak_manifest_loader, PLUGIN, FLATPAK_MANIFEST_LOADER, GObject)

PluginFlatpakManifestLoader *plugin_flatpak_manifest_loader_new              (GFile *file);
GFile                       *plugin_flatpak_manifest_loader_dup_file         (PluginFlatpakManifestLoader *self);
GFile                       *plugin_flatpak_manifest_loader_dup_base_dir     (PluginFlatpakManifestLoader *self);
DexFuture                   *plugin_flatpak_manifest_loader_load             (PluginFlatpakManifestLoader *self);
GListModel                  *plugin_flatpak_manifest_loader_list_diagnostics (PluginFlatpakManifestLoader *self);

G_END_DECLS
