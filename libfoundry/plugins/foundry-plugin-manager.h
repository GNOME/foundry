/* foundry-plugin-manager.h
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

#include <libpeas.h>

#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_PLUGIN_MANAGER (foundry_plugin_manager_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (FoundryPluginManager, foundry_plugin_manager, FOUNDRY, PLUGIN_MANAGER, GObject)

FOUNDRY_AVAILABLE_IN_ALL
FoundryPluginManager *foundry_plugin_manager_get_default  (void);
FOUNDRY_AVAILABLE_IN_ALL
gboolean              foundry_plugin_manager_get_disabled (FoundryPluginManager *self,
                                                           PeasPluginInfo       *plugin_info);
FOUNDRY_AVAILABLE_IN_ALL
void                  foundry_plugin_manager_set_disabled (FoundryPluginManager *self,
                                                           PeasPluginInfo       *plugin_info,
                                                           gboolean              disabled);

G_END_DECLS
