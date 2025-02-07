/* plugin-flatpak-source.h
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

#include <foundry.h>

#include "plugin-flatpak-serializable.h"

G_BEGIN_DECLS

#define PLUGIN_TYPE_FLATPAK_SOURCE (plugin_flatpak_source_get_type())

FOUNDRY_DECLARE_INTERNAL_TYPE (PluginFlatpakSource, plugin_flatpak_source, PLUGIN, FLATPAK_SOURCE, PluginFlatpakSerializable)

struct _PluginFlatpakSourceClass
{
  PluginFlatpakSerializableClass parent_class;
  const char *type;
};

GFile                *plugin_flatpak_source_dup_base_dir    (PluginFlatpakSource  *self) G_GNUC_WARN_UNUSED_RESULT;
void                  plugin_flatpak_source_set_base_dir    (PluginFlatpakSource  *self,
                                                             GFile                *base_dir);
char                 *plugin_flatpak_source_dup_dest        (PluginFlatpakSource  *self) G_GNUC_WARN_UNUSED_RESULT;
void                  plugin_flatpak_source_set_dest        (PluginFlatpakSource  *self,
                                                             const char           *dest);
char                **plugin_flatpak_source_dup_only_arches (PluginFlatpakSource  *self) G_GNUC_WARN_UNUSED_RESULT;
void                  plugin_flatpak_source_set_only_arches (PluginFlatpakSource  *self,
                                                             const char * const   *only_arches);
char                **plugin_flatpak_source_dup_skip_arches (PluginFlatpakSource  *self) G_GNUC_WARN_UNUSED_RESULT;
void                  plugin_flatpak_source_set_skip_arches (PluginFlatpakSource  *self,
                                                             const char * const   *skip_arches);
PluginFlatpakSource  *plugin_flatpak_source_new_from_json   (JsonNode             *node,
                                                             GError              **error) G_GNUC_WARN_UNUSED_RESULT;
JsonNode             *plugin_flatpak_source_to_json         (PluginFlatpakSource  *self) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
