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

#include <locale.h>

#include "plugin-gdb-debugger.h"

struct _PluginGdbDebugger
{
  FoundryDapDebugger parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginGdbDebugger, plugin_gdb_debugger, FOUNDRY_TYPE_DAP_DEBUGGER)

static DexFuture *
plugin_gdb_debugger_connect_to_target_fiber (PluginGdbDebugger     *self,
                                             FoundryDebuggerTarget *target)
{
  g_assert (PLUGIN_IS_GDB_DEBUGGER (self));
  g_assert (FOUNDRY_IS_DEBUGGER_TARGET (target));

  if (FOUNDRY_IS_DEBUGGER_TARGET_COMMAND (target))
    {
      g_autoptr(FoundryCommand) command = NULL;

      if ((command = foundry_debugger_target_command_dup_command (FOUNDRY_DEBUGGER_TARGET_COMMAND (target))))
        {
          g_autofree char *cwd = foundry_command_dup_cwd (command);
          g_auto(GStrv) argv = foundry_command_dup_argv (command);
          g_auto(GStrv) env = foundry_command_dup_environ (command);
          g_autoptr(JsonNode) message = NULL;
          g_autoptr(JsonNode) reply = NULL;
          g_autoptr(GError) error = NULL;

          message = FOUNDRY_JSON_OBJECT_NEW (
            "type", "request",
            "command", "launch",
            "arguments", "{",
              "args", FOUNDRY_JSON_NODE_PUT_STRV ((const char * const *)argv),
              "env", FOUNDRY_JSON_NODE_PUT_STRV ((const char * const *)env),
              "cwd", FOUNDRY_JSON_NODE_PUT_STRING (cwd),
              "stopAtBeginningOfMainSubprogram", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
              "stopOnEntry", FOUNDRY_JSON_NODE_PUT_BOOLEAN (FALSE),
            "}"
          );

          if (!(reply = dex_await_boxed (foundry_dap_debugger_call (FOUNDRY_DAP_DEBUGGER (self),
                                                                    g_steal_pointer (&message)),
                                         &error)))
            return dex_future_new_for_error (g_steal_pointer (&error));

          if (foundry_dap_protocol_has_error (reply))
            return dex_future_new_for_error (foundry_dap_protocol_extract_error (reply));

          return dex_future_new_true ();
        }
    }
  else if (FOUNDRY_IS_DEBUGGER_TARGET_PROCESS (target))
    {
      GPid pid = foundry_debugger_target_process_get_pid (FOUNDRY_DEBUGGER_TARGET_PROCESS (target));
      g_autoptr(JsonNode) message = NULL;
      g_autoptr(JsonNode) reply = NULL;
      g_autoptr(GError) error = NULL;

      message = FOUNDRY_JSON_OBJECT_NEW (
        "type", "request",
        "command", "attach",
        "arguments", "{",
          "pid", FOUNDRY_JSON_NODE_PUT_INT (pid),
          "program", FOUNDRY_JSON_NODE_PUT_STRING (NULL),
        "}"
      );

      if (!(reply = dex_await_boxed (foundry_dap_debugger_call (FOUNDRY_DAP_DEBUGGER (self),
                                                                g_steal_pointer (&message)),
                                     &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (foundry_dap_protocol_has_error (reply))
        return dex_future_new_for_error (foundry_dap_protocol_extract_error (reply));

      return dex_future_new_true ();
    }
  else if (FOUNDRY_IS_DEBUGGER_TARGET_REMOTE (target))
    {
      g_autofree char *address = foundry_debugger_target_remote_dup_address (FOUNDRY_DEBUGGER_TARGET_REMOTE (target));
      g_autoptr(JsonNode) message = NULL;
      g_autoptr(JsonNode) reply = NULL;
      g_autoptr(GError) error = NULL;

      message = FOUNDRY_JSON_OBJECT_NEW (
        "type", "request",
        "command", "attach",
        "arguments", "{",
          "target", FOUNDRY_JSON_NODE_PUT_STRING (address),
          "program", FOUNDRY_JSON_NODE_PUT_STRING (NULL),
        "}"
      );

      if (!(reply = dex_await_boxed (foundry_dap_debugger_call (FOUNDRY_DAP_DEBUGGER (self),
                                                                g_steal_pointer (&message)),
                                     &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (foundry_dap_protocol_has_error (reply))
        return dex_future_new_for_error (foundry_dap_protocol_extract_error (reply));

      return dex_future_new_true ();
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not supported");
}

static DexFuture *
plugin_gdb_debugger_connect_to_target (FoundryDebugger       *debugger,
                                       FoundryDebuggerTarget *target)
{
  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_gdb_debugger_connect_to_target_fiber),
                                  2,
                                  PLUGIN_TYPE_GDB_DEBUGGER, debugger,
                                  FOUNDRY_TYPE_DEBUGGER_TARGET, target);
}

static DexFuture *
plugin_gdb_debugger_initialize_fiber (gpointer data)
{
  PluginGdbDebugger *self = data;
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(JsonNode) message = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (PLUGIN_IS_GDB_DEBUGGER (self));

  message = FOUNDRY_JSON_OBJECT_NEW (
    "type", "request",
    "command", "initialize",
    "arguments", "{",
      "clientID", "libfoundry-" PACKAGE_VERSION,
      "clientName", FOUNDRY_JSON_NODE_PUT_STRING (g_get_application_name ()),
      "adapterID", "libfoundry-" PACKAGE_VERSION,
      "locale", FOUNDRY_JSON_NODE_PUT_STRING (setlocale (LC_ALL, NULL)),
      "pathFormat", "uri",
      "columnsStartAt1", FOUNDRY_JSON_NODE_PUT_BOOLEAN (FALSE),
      "linesStartAt1", FOUNDRY_JSON_NODE_PUT_BOOLEAN (FALSE),
      "supportsANSIStyling", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
      "supportsArgsCanBeInterpretedByShell", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
      "supportsMemoryEvent", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
      "supportsMemoryReferences", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
      "supportsProgressReporting", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
      "supportsRunInTerminalRequest", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
      "supportsStartDebuggingRequest", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
      "supportsVariablePaging", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
      "supportsVariableType", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
    "}"
  );

  if (!(reply = dex_await_boxed (foundry_dap_debugger_call (FOUNDRY_DAP_DEBUGGER (self),
                                                            g_steal_pointer (&message)),
                                 &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (foundry_dap_protocol_has_error (reply))
    return dex_future_new_for_error (foundry_dap_protocol_extract_error (reply));

  return dex_future_new_true ();
}

static DexFuture *
plugin_gdb_debugger_initialize (FoundryDebugger *debugger)
{
  PluginGdbDebugger *self = (PluginGdbDebugger *)debugger;

  g_assert (PLUGIN_IS_GDB_DEBUGGER (self));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_gdb_debugger_initialize_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static void
plugin_gdb_debugger_class_init (PluginGdbDebuggerClass *klass)
{
  FoundryDebuggerClass *debugger_class = FOUNDRY_DEBUGGER_CLASS (klass);

  debugger_class->connect_to_target = plugin_gdb_debugger_connect_to_target;
  debugger_class->initialize = plugin_gdb_debugger_initialize;
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
