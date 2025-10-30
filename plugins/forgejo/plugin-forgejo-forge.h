/* plugin-forgejo-forge.h
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <libsoup/soup.h>
#include <foundry.h>

G_BEGIN_DECLS

#define PLUGIN_TYPE_FORGEJO_FORGE (plugin_forgejo_forge_get_type())

G_DECLARE_FINAL_TYPE (PluginForgejoForge, plugin_forgejo_forge, PLUGIN, FORGEJO_FORGE, FoundryForge)

DexFuture *plugin_forgejo_forge_create_message             (PluginForgejoForge *self,
                                                            const char          *method,
                                                            const char          *path,
                                                            const char * const  *params,
                                                            ...);
DexFuture *plugin_forgejo_forge_send_message               (PluginForgejoForge  *self,
                                                            SoupMessage         *message);
DexFuture *plugin_forgejo_forge_send_message_and_read_json (PluginForgejoForge  *self,
                                                            SoupMessage         *message);

G_END_DECLS
