/* plugin-gdb-debugger-provider.c
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

#include "plugin-gdb-debugger-provider.h"

struct _PluginGdbDebuggerProvider
{
  FoundryDebuggerProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginGdbDebuggerProvider, plugin_gdb_debugger_provider, FOUNDRY_TYPE_DEBUGGER_PROVIDER)

static DexFuture *
plugin_gdb_debugger_provider_supports_fiber (FoundryDebuggerProvider *provider,
                                             FoundryBuildPipeline    *pipeline,
                                             FoundryCommand          *command)
{
  g_assert (PLUGIN_IS_GDB_DEBUGGER_PROVIDER (provider));
  g_assert (!pipeline || FOUNDRY_IS_BUILD_PIPELINE (pipeline));
  g_assert (FOUNDRY_IS_COMMAND (command));

  if (pipeline != NULL)
    {
      g_autoptr(GError) error = NULL;

      if (!dex_await (foundry_build_pipeline_contains_program (pipeline, "gdb"), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_for_int (0);
}

static DexFuture *
plugin_gdb_debugger_provider_supports (FoundryDebuggerProvider *provider,
                                       FoundryBuildPipeline    *pipeline,
                                       FoundryCommand          *command)
{
  g_assert (PLUGIN_IS_GDB_DEBUGGER_PROVIDER (provider));
  g_assert (!pipeline || FOUNDRY_IS_BUILD_PIPELINE (pipeline));
  g_assert (FOUNDRY_IS_COMMAND (command));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_gdb_debugger_provider_supports_fiber),
                                  3,
                                  FOUNDRY_TYPE_DEBUGGER_PROVIDER, provider,
                                  FOUNDRY_TYPE_BUILD_PIPELINE, pipeline,
                                  FOUNDRY_TYPE_COMMAND, command);
}

static void
plugin_gdb_debugger_provider_class_init (PluginGdbDebuggerProviderClass *klass)
{
  FoundryDebuggerProviderClass *debugger_provider_class = FOUNDRY_DEBUGGER_PROVIDER_CLASS (klass);

  debugger_provider_class->supports = plugin_gdb_debugger_provider_supports;
}

static void
plugin_gdb_debugger_provider_init (PluginGdbDebuggerProvider *self)
{
}
