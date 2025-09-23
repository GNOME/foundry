/* fdb.c
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

#include <gio/gunixinputstream.h>

#include <foundry.h>

static const char *dirpath;
static const char * const *command_argv;
static GMainLoop *main_loop;

static void
handle_log (GListModel *model,
            guint       position,
            guint       removed,
            guint       added,
            gpointer    user_data)
{
  if (added == 0)
    return;

  for (guint i = 0; i < added; i++)
    {
      g_autoptr(FoundryDebuggerLogMessage) item = g_list_model_get_item (model, position + i);
      g_autofree char *message = foundry_debugger_log_message_dup_message (item);

      g_print ("%s", message);
    }
}

static DexFuture *
main_fiber (gpointer data)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryCommand) command = NULL;
  g_autoptr(FoundryBuildManager) build_manager = NULL;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryDebuggerManager) debugger_manager = NULL;
  g_autoptr(FoundryDebuggerProvider) provider = NULL;
  g_autoptr(FoundryDebugger) debugger = NULL;
  g_autoptr(FoundryDebuggerTarget) target = NULL;
  g_autoptr(GInputStream) stdin_stream = NULL;
  g_autoptr(GListModel) logs = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  g_autofree char *title = NULL;

  dex_await (foundry_init (), NULL);

  if (!(path = dex_await_string (foundry_context_discover (dirpath, NULL), &error)))
    g_error ("Failed to discover project: %s", error->message);

  if (!(context = dex_await_object (foundry_context_new (path, NULL, FOUNDRY_CONTEXT_FLAGS_NONE, NULL), &error)))
    g_error ("Failed to load context: %s", error->message);

  title = foundry_context_dup_title (context);
  g_print ("Project `%s` loaded\n", title);

  build_manager = foundry_context_dup_build_manager (context);
  debugger_manager = foundry_context_dup_debugger_manager (context);

  if (!(pipeline = dex_await_object (foundry_build_manager_load_pipeline (build_manager), &error)))
    g_error ("Failed to load build pipeline: %s", error->message);

  command = foundry_command_new (context);
  foundry_command_set_argv (command, command_argv);
  foundry_command_set_cwd (command, g_get_current_dir ());

  if (!(provider = dex_await_object (foundry_debugger_manager_discover (debugger_manager, pipeline, command), &error)))
    g_error ("Failed to discover debugger: %s", error->message);
  g_print ("Using debugger provider of type `%s`\n", G_OBJECT_TYPE_NAME (provider));

  if (!(debugger = dex_await_object (foundry_debugger_provider_load_debugger (provider, pipeline), &error)))
    g_error ("Failed to load debugger: %s", error->message);
  g_print ("Using debugger of type `%s`\n", G_OBJECT_TYPE_NAME (debugger));

  logs = foundry_debugger_list_log_messages (debugger);
  g_signal_connect (logs, "items-changed", G_CALLBACK (handle_log), NULL);
  handle_log (logs, 0, 0, g_list_model_get_n_items (logs), NULL);

  if (!dex_await (foundry_debugger_initialize (debugger), &error))
    g_error ("Failed to initialize debugger: %s", error->message);

  g_print ("\n");
  g_print ("(^C) stop, (s) step, (si) step-in, (so) step-out, (b symbol) add breakpoint\n");

  stdin_stream = g_unix_input_stream_new (STDIN_FILENO, FALSE);
  target = foundry_debugger_target_command_new (command);

  if (!dex_await (foundry_debugger_connect_to_target (debugger, target), &error))
    g_error ("Failed to connect to target: %s", error->message);

  g_main_loop_quit (main_loop);

  return dex_future_new_true ();
}

int
main (int argc,
      char *argv[])
{
  int i = 1;

  if (argc < 3)
    {
    print_usage:
      g_printerr ("usage: %s [PROJECT_DIR] -- COMMAND...\n", argv[0]);
      return 1;
    }

  if (g_strcmp0 (argv[i], "--") == 0)
    dirpath = g_get_current_dir ();
  else
    dirpath = argv[i++];

  if (g_strcmp0 (argv[i], "--") != 0)
    goto print_usage;

  command_argv = (const char * const *)&argv[++i];

  main_loop = g_main_loop_new (NULL, FALSE);
  dex_future_disown (dex_scheduler_spawn (NULL, 8*1024*1024, main_fiber, NULL, NULL));
  g_main_loop_run (main_loop);

  return 0;
}
