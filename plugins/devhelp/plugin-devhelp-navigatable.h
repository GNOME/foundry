/*
 * plugin-devhelp-navigatable.h
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

#include <gio/gio.h>
#include <libdex.h>

G_BEGIN_DECLS

#define PLUGIN_TYPE_DEVHELP_NAVIGATABLE (plugin_devhelp_navigatable_get_type())

G_DECLARE_FINAL_TYPE (PluginDevhelpNavigatable, plugin_devhelp_navigatable, PLUGIN, DEVHELP_NAVIGATABLE, GObject)

PluginDevhelpNavigatable *plugin_devhelp_navigatable_new              (void);
PluginDevhelpNavigatable *plugin_devhelp_navigatable_new_for_resource (GObject                  *resource);
GIcon                    *plugin_devhelp_navigatable_get_icon         (PluginDevhelpNavigatable *self);
void                      plugin_devhelp_navigatable_set_icon         (PluginDevhelpNavigatable *self,
                                                                       GIcon                    *icon);
const char               *plugin_devhelp_navigatable_get_title        (PluginDevhelpNavigatable *self);
void                      plugin_devhelp_navigatable_set_title        (PluginDevhelpNavigatable *self,
                                                                       const char               *title);
GIcon                    *plugin_devhelp_navigatable_get_menu_icon    (PluginDevhelpNavigatable *self);
void                      plugin_devhelp_navigatable_set_menu_icon    (PluginDevhelpNavigatable *self,
                                                                       GIcon                    *menu_icon);
const char               *plugin_devhelp_navigatable_get_menu_title   (PluginDevhelpNavigatable *self);
void                      plugin_devhelp_navigatable_set_menu_title   (PluginDevhelpNavigatable *self,
                                                                       const char               *menu_title);
const char               *plugin_devhelp_navigatable_get_uri          (PluginDevhelpNavigatable *self);
void                      plugin_devhelp_navigatable_set_uri          (PluginDevhelpNavigatable *self,
                                                                       const char               *uri);
gpointer                  plugin_devhelp_navigatable_get_item         (PluginDevhelpNavigatable *self);
void                      plugin_devhelp_navigatable_set_item         (PluginDevhelpNavigatable *self,
                                                                       gpointer                  item);
DexFuture                *plugin_devhelp_navigatable_find_parent      (PluginDevhelpNavigatable *self);
DexFuture                *plugin_devhelp_navigatable_find_children    (PluginDevhelpNavigatable *self);
DexFuture                *plugin_devhelp_navigatable_find_peers       (PluginDevhelpNavigatable *self);

G_END_DECLS
