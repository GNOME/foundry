/* plugin-flatpak-util.h
 *
 * Copyright 2016-2024 Christian Hergert <chergert@redhat.com>
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

DexFuture  *plugin_flatpak_get_a11y_bus     (void);
gboolean    plugin_flatpak_parse_a11y_bus   (const char              *address,
                                             char                   **unix_path,
                                             char                   **address_suffix);
void        plugin_flatpak_set_config_dir   (FoundryProcessLauncher  *launcher);
char       *plugin_flatpak_get_repo_dir     (FoundryContext          *context);
char       *plugin_flatpak_get_staging_dir  (FoundryBuildPipeline    *pipeline);
gboolean    plugin_flatpak_split_id         (const char              *str,
                                             char                   **id,
                                             char                   **arch,
                                             char                   **branch);

G_END_DECLS
