/* plugin-gjs-dap-debugger-provider.c
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
#include "plugin-gjs-dap-debugger-provider.h"

struct _PluginGjsDapDebuggerProvider
{
  FoundryDebuggerProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginGjsDapDebuggerProvider, plugin_gjs_dap_debugger_provider, FOUNDRY_TYPE_DEBUGGER_PROVIDER)

static gboolean
is_module_command (const char * const *argv)
{
  g_autofree char *basename = NULL;

  if (argv == NULL || g_strv_length ((char **)argv) != 3)
    return FALSE;

  basename = g_path_get_basename (argv[0]);

  return g_str_equal (basename, "gjs") &&
         (g_str_equal (argv[1], "-m") || g_str_equal (argv[1], "--module")) &&
         argv[2][0] != '-' && argv[2][0] != 0;
}

static DexFuture *
prepare_launcher (FoundryCommand       *command,
                  FoundryBuildPipeline *pipeline)
{
  g_autoptr(FoundryProcessLauncher) launcher = foundry_process_launcher_new ();
  g_auto(GStrv) argv = foundry_command_dup_argv (command);
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_COMMAND (command));
  g_assert (!pipeline || FOUNDRY_IS_BUILD_PIPELINE (pipeline));

  if (!is_module_command ((const char * const *)argv))
    return foundry_future_new_not_supported ();

  if (!dex_await (foundry_command_prepare (command,
                                           pipeline,
                                           launcher,
                                           FOUNDRY_BUILD_PIPELINE_PHASE_NONE),
                  &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!is_module_command (foundry_process_launcher_get_argv (launcher)))
    return foundry_future_new_not_supported ();

  return dex_future_new_take_object (g_steal_pointer (&launcher));
}

static DexFuture *
probe (FoundryProcessLauncher *launcher)
{
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *output = NULL;
  g_autofree char *interpreter = NULL;
  const char *argv[3];

  g_assert (FOUNDRY_IS_PROCESS_LAUNCHER (launcher));

  interpreter = g_strdup (foundry_process_launcher_get_argv (launcher)[0]);
  argv[0] = interpreter;
  argv[1] = "--help";
  argv[2] = NULL;
  foundry_process_launcher_set_argv (launcher, argv);

  if (!(subprocess = foundry_process_launcher_spawn_with_flags (launcher,
                         (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(output = dex_await_string (dex_future_with_timeout_seconds (
          foundry_subprocess_communicate_utf8 (subprocess, NULL), 5), &error)))
    {
      g_subprocess_force_exit (subprocess);
      dex_await (dex_subprocess_wait (subprocess), NULL);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (!g_subprocess_get_successful (subprocess) || strstr (output, "--inspect") == NULL)
    return foundry_future_new_not_supported ();

  return dex_future_new_true ();
}

static DexFuture *
plugin_gjs_dap_debugger_provider_supports_fiber (FoundryDebuggerProvider *provider,
                                                 FoundryBuildPipeline    *pipeline,
                                                 FoundryCommand          *command)
{
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (PLUGIN_IS_GJS_DAP_DEBUGGER_PROVIDER (provider));

  if (!(launcher = dex_await_object (prepare_launcher (command, pipeline), &error)) ||
      !dex_await (probe (launcher), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_for_int (30);
}

static DexFuture *
plugin_gjs_dap_debugger_provider_load_fiber (FoundryDebuggerProvider *provider,
                                             FoundryBuildPipeline    *pipeline,
                                             FoundryCommand          *command)
{
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GIOStream) stream = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *interpreter = NULL;
  g_autofree char *program = NULL;
  g_autofree char *cwd = NULL;
  const char *argv[3];

  g_assert (PLUGIN_IS_GJS_DAP_DEBUGGER_PROVIDER (provider));

  if (!(launcher = dex_await_object (prepare_launcher (command, pipeline), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  interpreter = g_strdup (foundry_process_launcher_get_argv (launcher)[0]);
  program = g_strdup (foundry_process_launcher_get_argv (launcher)[2]);
  cwd = g_canonicalize_filename (foundry_process_launcher_get_cwd (launcher) ?: ".", NULL);

  if (!dex_await (probe (launcher), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  g_clear_object (&launcher);
  if (!(launcher = dex_await_object (prepare_launcher (command, pipeline), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  argv[0] = interpreter;
  argv[1] = "--inspect";
  argv[2] = NULL;
  foundry_process_launcher_set_argv (launcher, argv);
  foundry_process_launcher_set_cwd (launcher, cwd);

  if (!(stream = foundry_process_launcher_create_stdio_stream (launcher, &error)) ||
      !(subprocess = foundry_process_launcher_spawn (launcher, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (provider));

  return dex_future_new_take_object (plugin_gjs_dap_debugger_new (context, subprocess, stream,
                                                               command, cwd, program));
}

static DexFuture *
plugin_gjs_dap_debugger_provider_supports (FoundryDebuggerProvider *provider,
                                           FoundryBuildPipeline    *pipeline,
                                           FoundryCommand          *command)
{
  g_assert (PLUGIN_IS_GJS_DAP_DEBUGGER_PROVIDER (provider));

  return FOUNDRY_SCHEDULER_SPAWN (NULL, 0,
                                plugin_gjs_dap_debugger_provider_supports_fiber,
                                3,
                                FOUNDRY_TYPE_DEBUGGER_PROVIDER, provider,
                                FOUNDRY_TYPE_BUILD_PIPELINE, pipeline,
                                FOUNDRY_TYPE_COMMAND, command);
}

static DexFuture *
plugin_gjs_dap_debugger_provider_load (FoundryDebuggerProvider *provider,
                                       FoundryBuildPipeline    *pipeline,
                                       FoundryCommand          *command)
{
  g_assert (PLUGIN_IS_GJS_DAP_DEBUGGER_PROVIDER (provider));

  return FOUNDRY_SCHEDULER_SPAWN (NULL, 0,
                                plugin_gjs_dap_debugger_provider_load_fiber,
                                3,
                                FOUNDRY_TYPE_DEBUGGER_PROVIDER, provider,
                                FOUNDRY_TYPE_BUILD_PIPELINE, pipeline,
                                FOUNDRY_TYPE_COMMAND, command);
}

static void
plugin_gjs_dap_debugger_provider_class_init (PluginGjsDapDebuggerProviderClass *klass)
{
  FoundryDebuggerProviderClass *provider_class = FOUNDRY_DEBUGGER_PROVIDER_CLASS (klass);

  provider_class->supports = plugin_gjs_dap_debugger_provider_supports;
  provider_class->load_debugger_for_command = plugin_gjs_dap_debugger_provider_load;
}

static void
plugin_gjs_dap_debugger_provider_init (PluginGjsDapDebuggerProvider *self)
{
}
