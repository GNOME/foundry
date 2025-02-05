/* plugin-flatpak-builder-utils.h
 *
 * Copyright 2015 Red Hat, Inc
 * Copyright 2023 GNOME Foundation Inc.
 * Copyright 2025 Christian Hergert
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
 * Authors:
 *       Alexander Larsson <alexl@redhat.com>
 *       Hubert Figuière <hub@figuiere.net>
 *       Christian Hergert <chergert@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <foundry.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

GParamSpec  *plugin_flatpak_builder_serializable_find_property        (JsonSerializable *serializable,
                                                                       const char       *name);
GParamSpec **plugin_flatpak_builder_serializable_list_properties      (JsonSerializable *serializable,
                                                                       guint            *n_pspecs);
gboolean     plugin_flatpak_builder_serializable_deserialize_property (JsonSerializable *serializable,
                                                                       const char       *property_name,
                                                                       GValue           *value,
                                                                       GParamSpec       *pspec,
                                                                       JsonNode         *property_node);
JsonNode    *plugin_flatpak_builder_serializable_serialize_property   (JsonSerializable *serializable,
                                                                       const char       *property_name,
                                                                       const GValue     *value,
                                                                       GParamSpec       *pspec);
void         plugin_flatpak_builder_serializable_get_property         (JsonSerializable *serializable,
                                                                       GParamSpec       *pspec,
                                                                       GValue           *value);
void         plugin_flatpak_builder_serializable_set_property         (JsonSerializable *serializable,
                                                                       GParamSpec       *pspec,
                                                                       const GValue     *value);

G_END_DECLS
