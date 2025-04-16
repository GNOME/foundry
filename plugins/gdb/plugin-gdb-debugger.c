/* plugin-gdb-debugger.c
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

#include "config.h"

#include "plugin-gdb-debugger.h"

struct _PluginGdbDebugger
{
  FoundryDapDebugger parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginGdbDebugger, plugin_gdb_debugger, FOUNDRY_TYPE_DAP_DEBUGGER)

static DexFuture *
plugin_gdb_debugger_connect_to_target (FoundryDebugger       *debugger,
                                       FoundryDebuggerTarget *target)
{
  PluginGdbDebugger *self = (PluginGdbDebugger *)debugger;

  g_assert (PLUGIN_IS_GDB_DEBUGGER (self));
  g_assert (FOUNDRY_IS_DEBUGGER_TARGET (target));

  if (FOUNDRY_IS_DEBUGGER_TARGET_COMMAND (target))
    {
    }
  else if (FOUNDRY_IS_DEBUGGER_TARGET_PROCESS (target))
    {
    }
  else if (FOUNDRY_IS_DEBUGGER_TARGET_REMOTE (target))
    {
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not supported");
}

static void
plugin_gdb_debugger_class_init (PluginGdbDebuggerClass *klass)
{
  FoundryDebuggerClass *debugger_class = FOUNDRY_DEBUGGER_CLASS (klass);

  debugger_class->connect_to_target = plugin_gdb_debugger_connect_to_target;
}

static void
plugin_gdb_debugger_init (PluginGdbDebugger *self)
{
}

FoundryDebugger *
plugin_gdb_debugger_new (FoundryContext *context,
                         GSubprocess    *subprocess,
                         GIOStream      *stream)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (G_IS_SUBPROCESS (subprocess), NULL);
  g_return_val_if_fail (G_IS_IO_STREAM (stream), NULL);

  return g_object_new (PLUGIN_TYPE_GDB_DEBUGGER,
                       "context", context,
                       "subprocess", subprocess,
                       "stream", stream,
                       NULL);
}
