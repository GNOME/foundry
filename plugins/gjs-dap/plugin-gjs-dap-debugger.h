/* plugin-gjs-dap-debugger.h
 *
 * Copyright 2026 Christian Hergert
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

#define PLUGIN_TYPE_GJS_DAP_DEBUGGER (plugin_gjs_dap_debugger_get_type())

G_DECLARE_FINAL_TYPE (PluginGjsDapDebugger, plugin_gjs_dap_debugger, PLUGIN, GJS_DAP_DEBUGGER, FoundryDapDebugger)

FoundryDebugger *plugin_gjs_dap_debugger_new (FoundryContext *context,
                                              GSubprocess    *subprocess,
                                              GIOStream      *stream,
                                              FoundryCommand *command,
                                              const char     *cwd,
                                              const char     *program);

G_END_DECLS
