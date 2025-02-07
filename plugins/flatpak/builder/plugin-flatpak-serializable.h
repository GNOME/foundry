/* plugin-flatpak-serializable.h
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
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define PLUGIN_TYPE_FLATPAK_SERIALIZABLE (plugin_flatpak_serializable_get_type())

G_DECLARE_DERIVABLE_TYPE (PluginFlatpakSerializable, plugin_flatpak_serializable, PLUGIN, FLATPAK_SERIALIZABLE, GObject)

struct _PluginFlatpakSerializableClass
{
  GObjectClass parent_class;

  DexFuture *(*deserialize)          (PluginFlatpakSerializable *self,
                                      JsonNode                  *node);
  DexFuture *(*deserialize_property) (PluginFlatpakSerializable *self,
                                      const char                *property_name,
                                      JsonNode                  *property_node);
};

GFile *plugin_flatpak_serializable_resolve_file (PluginFlatpakSerializable  *self,
                                                 const char                 *path,
                                                 GError                    **error);

G_END_DECLS
