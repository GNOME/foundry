/* plugin-gjs-dap-debugger.c
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

#include "config.h"

#include "plugin-gjs-dap-debugger.h"
#include "foundry-dap-debugger-private.h"

struct _PluginGjsDapDebugger
{
  FoundryDapDebugger  parent_instance;
  FoundryCommand     *command;
  char               *cwd;
  char               *program;
};

G_DEFINE_FINAL_TYPE (PluginGjsDapDebugger, plugin_gjs_dap_debugger, FOUNDRY_TYPE_DAP_DEBUGGER)

static DexFuture *
call_checked (PluginGjsDapDebugger *self,
              JsonNode             *node)
{
  g_assert (PLUGIN_IS_GJS_DAP_DEBUGGER (self));

  return dex_future_then (foundry_dap_debugger_call (FOUNDRY_DAP_DEBUGGER (self), node),
                          foundry_dap_protocol_unwrap_error, NULL, NULL);
}

static gboolean
plugin_gjs_dap_debugger_supports_request (FoundryDapDebugger *debugger,
                                          const char         *request)
{
  static const char * const supported[] = {
    "initialize", "launch", "configurationDone", "disconnect", "threads",
    "continue", "stackTrace", "setBreakpoints", "scopes", "variables",
    "next", "stepIn", "stepOut", "setExceptionBreakpoints", NULL
  };

  g_assert (PLUGIN_IS_GJS_DAP_DEBUGGER (debugger));

  return g_strv_contains (supported, request);
}

static DexFuture *
plugin_gjs_dap_debugger_initialize_fiber (gpointer data)
{
  PluginGjsDapDebugger *self = data;
  g_autoptr(GError) error = NULL;

  g_assert (PLUGIN_IS_GJS_DAP_DEBUGGER (self));

  if (!dex_await (call_checked (self,
                                FOUNDRY_JSON_OBJECT_NEW ("type", "request", "command", "initialize", "arguments", "{", "clientID", "foundry", "adapterID", "gjs", "pathFormat", "path", "linesStartAt1", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE), "columnsStartAt1", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE), "}")),
                  &error) ||
      !dex_await (foundry_dap_debugger_when_initialized (FOUNDRY_DAP_DEBUGGER (self)), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
plugin_gjs_dap_debugger_initialize (FoundryDebugger *debugger)
{
  g_assert (PLUGIN_IS_GJS_DAP_DEBUGGER (debugger));

  return dex_future_with_timeout_seconds (
    dex_scheduler_spawn (NULL, 0, plugin_gjs_dap_debugger_initialize_fiber,
                         g_object_ref (debugger), g_object_unref), 5);
}

static DexFuture *
plugin_gjs_dap_debugger_connect_fiber (PluginGjsDapDebugger  *self,
                                       FoundryDebuggerTarget *target)
{
  g_autoptr(FoundryCommand) command = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *program = NULL;
  g_autofree char *absolute = NULL;
  g_autoptr(GFile) base = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GString) prefix = g_string_new (NULL);

  g_assert (PLUGIN_IS_GJS_DAP_DEBUGGER (self));
  g_assert (FOUNDRY_IS_DEBUGGER_TARGET (target));

  if (!FOUNDRY_IS_DEBUGGER_TARGET_COMMAND (target))
    return foundry_future_new_not_supported ();

  command = foundry_debugger_target_command_dup_command (FOUNDRY_DEBUGGER_TARGET_COMMAND (target));
  if (command != self->command)
    return foundry_future_new_not_supported ();

  if (!dex_await (foundry_dap_debugger_when_initialized (FOUNDRY_DAP_DEBUGGER (self)), &error) ||
      !dex_await (_foundry_dap_debugger_flush_breakpoints (FOUNDRY_DAP_DEBUGGER (self)), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  /* The draft concatenates cwd and program even for absolute filenames. */
  absolute = g_canonicalize_filename (self->program, self->cwd);
  file = g_file_new_for_path (absolute);
  base = g_file_new_for_path (self->cwd);
  while (!(program = g_file_get_relative_path (base, file)))
    {
      GFile *parent = g_file_get_parent (base);

      if (parent == NULL)
        return foundry_future_new_not_supported ();
      g_set_object (&base, parent);
      g_object_unref (parent);
      g_string_append (prefix, "../");
    }
  g_string_append (prefix, program);

  if (!dex_await (call_checked (self,
                                FOUNDRY_JSON_OBJECT_NEW ("type", "request", "command", "launch", "arguments", "{", "cwd", FOUNDRY_JSON_NODE_PUT_STRING (self->cwd), "program", FOUNDRY_JSON_NODE_PUT_STRING (prefix->str), "stopOnEntry", FOUNDRY_JSON_NODE_PUT_BOOLEAN (FALSE), "}")),
                  &error) ||
      !dex_await (call_checked (self,
        FOUNDRY_JSON_OBJECT_NEW ("type", "request", "command", "configurationDone")), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
plugin_gjs_dap_debugger_connect (FoundryDebugger       *debugger,
                                 FoundryDebuggerTarget *target)
{
  g_assert (PLUGIN_IS_GJS_DAP_DEBUGGER (debugger));

  return FOUNDRY_SCHEDULER_SPAWN (NULL, 0, plugin_gjs_dap_debugger_connect_fiber,
                                2,
                                PLUGIN_TYPE_GJS_DAP_DEBUGGER, debugger,
                                FOUNDRY_TYPE_DEBUGGER_TARGET, target);
}

static DexFuture *
plugin_gjs_dap_debugger_stop_fiber (gpointer data)
{
  PluginGjsDapDebugger *self = data;
  g_autoptr(GSubprocess) subprocess = NULL;
  gint64 deadline = g_get_monotonic_time () + G_USEC_PER_SEC;

  g_assert (PLUGIN_IS_GJS_DAP_DEBUGGER (self));

  subprocess = foundry_dap_debugger_dup_subprocess (FOUNDRY_DAP_DEBUGGER (self));
  dex_await (dex_future_with_deadline (call_checked (self, FOUNDRY_JSON_OBJECT_NEW ("type", "request", "command", "disconnect")),
                                       deadline),
             NULL);

  if (!dex_await (dex_future_with_deadline (dex_subprocess_wait (subprocess), deadline), NULL))
    {
      g_subprocess_force_exit (subprocess);
      dex_await (dex_subprocess_wait (subprocess), NULL);
    }

  return dex_future_new_true ();
}

static DexFuture *
plugin_gjs_dap_debugger_stop (FoundryDebugger *debugger)
{
  g_assert (PLUGIN_IS_GJS_DAP_DEBUGGER (debugger));

  return dex_scheduler_spawn (NULL,
                              0,
                              plugin_gjs_dap_debugger_stop_fiber,
                              g_object_ref (debugger),
                              g_object_unref);
}

static char *
plugin_gjs_dap_debugger_dup_name (FoundryDebugger *debugger)
{
  return g_strdup ("GJS");
}

static void
plugin_gjs_dap_debugger_finalize (GObject *object)
{
  PluginGjsDapDebugger *self = PLUGIN_GJS_DAP_DEBUGGER (object);

  g_clear_object (&self->command);
  g_clear_pointer (&self->cwd, g_free);
  g_clear_pointer (&self->program, g_free);

  G_OBJECT_CLASS (plugin_gjs_dap_debugger_parent_class)->finalize (object);
}

static void
plugin_gjs_dap_debugger_class_init (PluginGjsDapDebuggerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryDebuggerClass *debugger_class = FOUNDRY_DEBUGGER_CLASS (klass);
  FoundryDapDebuggerClass *dap_class = FOUNDRY_DAP_DEBUGGER_CLASS (klass);

  object_class->finalize = plugin_gjs_dap_debugger_finalize;
  debugger_class->initialize = plugin_gjs_dap_debugger_initialize;
  debugger_class->connect_to_target = plugin_gjs_dap_debugger_connect;
  debugger_class->stop = plugin_gjs_dap_debugger_stop;
  debugger_class->dup_name = plugin_gjs_dap_debugger_dup_name;
  dap_class->supports_request = plugin_gjs_dap_debugger_supports_request;
}

static void
plugin_gjs_dap_debugger_init (PluginGjsDapDebugger *self)
{
}

FoundryDebugger *
plugin_gjs_dap_debugger_new (FoundryContext *context,
                             GSubprocess    *subprocess,
                             GIOStream      *stream,
                             FoundryCommand *command,
                             const char     *cwd,
                             const char     *program)
{
  PluginGjsDapDebugger *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (G_IS_SUBPROCESS (subprocess), NULL);
  g_return_val_if_fail (G_IS_IO_STREAM (stream), NULL);
  g_return_val_if_fail (FOUNDRY_IS_COMMAND (command), NULL);
  g_return_val_if_fail (cwd != NULL, NULL);
  g_return_val_if_fail (program != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_GJS_DAP_DEBUGGER,
                       "context", context,
                       "subprocess", subprocess,
                       "stream", stream,
                       "quirks", (FOUNDRY_DAP_DEBUGGER_QUIRK_QUERY_THREADS | FOUNDRY_DAP_DEBUGGER_QUIRK_ONE_BASED_COORDINATES | FOUNDRY_DAP_DEBUGGER_QUIRK_UNCLASSIFIED_LOCALS | FOUNDRY_DAP_DEBUGGER_QUIRK_NEWEST_FRAME_ONLY | FOUNDRY_DAP_DEBUGGER_QUIRK_ENCODED_SOURCE_PATHS),
                       NULL);
  self->command = g_object_ref (command);
  self->cwd = g_strdup (cwd);
  self->program = g_strdup (program);

  return FOUNDRY_DEBUGGER (self);
}
