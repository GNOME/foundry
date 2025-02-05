/* plugin-flatpak-builder-context.h
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

#define PLUGIN_TYPE_FLATPAK_BUILDER_CONTEXT (plugin_flatpak_builder_context_get_type())

G_DECLARE_FINAL_TYPE (PluginFlatpakBuilderContext, plugin_flatpak_builder_context, PLUGIN, FLATPAK_BUILDER_CONTEXT, GObject)

GFile *plugin_flatpak_builder_context_dup_app_dir (PluginFlatpakBuilderContext *self);
void plugin_flatpak_builder_context_set_app_dir (PluginFlatpakBuilderContext *self,
                                                 GFile *app_dir);
GFile *plugin_flatpak_builder_context_dup_run_dir (PluginFlatpakBuilderContext *self);
void plugin_flatpak_builder_context_set_run_dir (PluginFlatpakBuilderContext *self,
                                                 GFile *run_dir);
char *plugin_flatpak_builder_context_dup_state_subdir (PluginFlatpakBuilderContext *self);
void plugin_flatpak_builder_context_set_state_subdir (PluginFlatpakBuilderContext *self,
                                                      const char *state_subdir);
char *plugin_flatpak_builder_context_dup_arch (PluginFlatpakBuilderContext *self);

G_END_DECLS
