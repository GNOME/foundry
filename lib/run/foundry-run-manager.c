/* foundry-run-manager.c
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

#include <glib/gstdio.h>

#include <libpeas.h>

#include "foundry-build-progress.h"
#include "foundry-build-pipeline.h"
#include "foundry-command.h"
#include "foundry-debug.h"
#include "foundry-deploy-strategy.h"
#include "foundry-process-launcher.h"
#include "foundry-service-private.h"
#include "foundry-run-manager.h"
#include "foundry-run-tool-private.h"
#include "foundry-sdk.h"

struct _FoundryRunManager
{
  FoundryService parent_instance;
};

struct _FoundryRunManagerClass
{
  FoundryServiceClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryRunManager, foundry_run_manager, FOUNDRY_TYPE_SERVICE)

static void
foundry_run_manager_class_init (FoundryRunManagerClass *klass)
{
}

static void
foundry_run_manager_init (FoundryRunManager *self)
{
}

/**
 * foundry_run_manager_list_tools:
 * @self: a #FoundryRunManager
 *
 * Gets the available tools that can be used to run the program.
 *
 * Returns: (transfer full): a list of tools supported by the run manager
 *   such as "gdb" or "valgrind" or "sysprof".
 */
char **
foundry_run_manager_list_tools (FoundryRunManager *self)
{
  g_autoptr(PeasExtensionSet) addins = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GStrvBuilder) builder = NULL;
  guint n_items;

  g_return_val_if_fail (FOUNDRY_IS_RUN_MANAGER (self), NULL);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  addins = peas_extension_set_new (peas_engine_get_default (),
                                   FOUNDRY_TYPE_RUN_TOOL,
                                   "context", context,
                                   NULL);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (addins));
  builder = g_strv_builder_new ();

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryRunTool) run_tool = g_list_model_get_item (G_LIST_MODEL (addins), i);
      g_autoptr(PeasPluginInfo) plugin_info = NULL;

      if ((plugin_info = foundry_run_tool_dup_plugin_info (run_tool)))
        g_strv_builder_add (builder, peas_plugin_info_get_module_name (plugin_info));
    }

  return g_strv_builder_end (builder);
}

typedef struct _Run
{
  FoundryRunTool         *run_tool;
  FoundryBuildPipeline   *pipeline;
  FoundryCommand         *command;
  FoundryProcessLauncher *launcher;
  DexCancellable         *cancellable;
  int                     build_pty_fd;
  int                     run_pty_fd;
} Run;

static void
run_free (Run *state)
{
  g_clear_object (&state->run_tool);
  g_clear_object (&state->pipeline);
  g_clear_object (&state->command);
  g_clear_object (&state->launcher);
  g_clear_fd (&state->build_pty_fd, NULL);
  g_clear_fd (&state->run_pty_fd, NULL);
  dex_clear (&state->cancellable);
  g_free (state);
}

static DexFuture *
foundry_run_manager_run_fiber (gpointer data)
{
  Run *state = data;
  g_autoptr(FoundryDeployStrategy) deploy_strategy = NULL;
  g_autoptr(FoundryBuildProgress) progress = NULL;
  g_autoptr(FoundrySdk) sdk = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_RUN_TOOL (state->run_tool));
  g_assert (FOUNDRY_IS_BUILD_PIPELINE (state->pipeline));
  g_assert (FOUNDRY_IS_COMMAND (state->command));
  g_assert (FOUNDRY_IS_PROCESS_LAUNCHER (state->launcher));
  g_assert (!state->cancellable || DEX_IS_CANCELLABLE (state->cancellable));
  g_assert (state->build_pty_fd >= -1);
  g_assert (state->run_pty_fd >= -1);

  sdk = foundry_build_pipeline_dup_sdk (state->pipeline);

  if (!(deploy_strategy = dex_await_object (foundry_deploy_strategy_new (state->pipeline), &error)) ||
      !dex_await (foundry_deploy_strategy_deploy (deploy_strategy, state->build_pty_fd, state->cancellable), &error) ||
      !dex_await (foundry_deploy_strategy_prepare (deploy_strategy, state->launcher, state->pipeline, state->build_pty_fd, state->cancellable), &error) ||
      !dex_await (foundry_run_tool_prepare (state->run_tool, state->pipeline, state->command, state->launcher), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_object_ref (state->run_tool));
}

/**
 * foundry_run_manager_run:
 * @self: a [class@Foundry.RunManager]
 * @pipeline: a [class@Foundry.BuildPipeline]
 * @command: a [class@Foundry.Command]
 *
 * Starts running a program.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.RunTool].
 */
DexFuture *
foundry_run_manager_run (FoundryRunManager    *self,
                         FoundryBuildPipeline *pipeline,
                         FoundryCommand       *command,
                         const char           *tool,
                         int                   build_pty_fd,
                         int                   run_pty_fd,
                         DexCancellable       *cancellable)
{
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GObject) object = NULL;
  PeasPluginInfo *plugin_info;
  PeasEngine *engine;
  Run *state;

  dex_return_error_if_fail (FOUNDRY_IS_RUN_MANAGER (self));
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_PIPELINE (pipeline));
  dex_return_error_if_fail (FOUNDRY_IS_COMMAND (command));
  dex_return_error_if_fail (tool != NULL);
  dex_return_error_if_fail (build_pty_fd >= -1);
  dex_return_error_if_fail (run_pty_fd >= -1);
  dex_return_error_if_fail (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  engine = peas_engine_get_default ();

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  if (context == NULL)
    goto reject;

  plugin_info = peas_engine_get_plugin_info (engine, tool);
  if (plugin_info == NULL)
    goto reject;

  object = peas_engine_create_extension (engine,
                                         plugin_info,
                                         FOUNDRY_TYPE_RUN_TOOL,
                                         "context", context,
                                         NULL);
  if (object == NULL)
    goto reject;

  state = g_new0 (Run, 1);
  state->command = g_object_ref (command);
  state->pipeline = g_object_ref (pipeline);
  state->run_tool = g_object_ref (FOUNDRY_RUN_TOOL (object));
  state->launcher = foundry_process_launcher_new ();
  state->build_pty_fd = build_pty_fd >= 0 ? dup (build_pty_fd) : -1;
  state->run_pty_fd = run_pty_fd >= 0 ? dup (run_pty_fd) : -1;
  state->cancellable = cancellable ? dex_ref (cancellable) : NULL;

  return dex_scheduler_spawn (NULL, 0,
                              foundry_run_manager_run_fiber,
                              state,
                              (GDestroyNotify) run_free);

reject:
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_FOUND,
                                "Cannot find tool \"%s\"",
                                tool);
}
