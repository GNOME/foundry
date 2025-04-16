/* plugin-gdb-launch-request.h
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

#include "foundry-dap-request-private.h"

G_BEGIN_DECLS

#define PLUGIN_TYPE_GDB_LAUNCH_REQUEST (plugin_gdb_launch_request_get_type())

G_DECLARE_FINAL_TYPE (PluginGdbLaunchRequest, plugin_gdb_launch_request, PLUGIN, GDB_LAUNCH_REQUEST, FoundryDapRequest)

FoundryDapRequest *plugin_gdb_launch_request_new (const char * const *args,
                                                  const char         *cwd,
                                                  const char * const *env,
                                                  const char         *program,
                                                  gboolean            stop_at_main,
                                                  gboolean            stop_on_entry);

G_END_DECLS
