/* test-debugger-gtk.c
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

#include <foundry.h>
#include <gtk/gtk.h>

static GMainLoop *main_loop;
static const char *project_dir;
static const char *command_name;
static char **command_argv;
static int command_argc;

static const char ui_data[] = {
#embed "test-debugger-gtk.ui"
};

static DexFuture *
main_fiber (gpointer data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GtkBuilder) builder = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryCommand) command = NULL;
  g_autoptr(FoundryDebuggerManager) debugger_manager = NULL;
  g_autoptr(FoundryDebuggerProvider) provider = NULL;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryBuildManager) build_manager = NULL;
  g_autoptr(FoundryDebugger) debugger = NULL;
  g_autoptr(FoundryDebuggerTarget) target = NULL;
  g_autoptr(FoundryDebuggerActions) actions = NULL;
  g_autofree char *path = NULL;
  GtkWindow *window;
  GtkStack *source_stack;

  dex_await (foundry_init (), NULL);

  if (!(path = dex_await_string (foundry_context_discover (project_dir, NULL), &error)))
    g_error ("%s", error->message);

  if (!(context = dex_await_object (foundry_context_new (path, project_dir, FOUNDRY_CONTEXT_FLAGS_NONE, NULL), &error)))
    g_error ("%s", error->message);

  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_string (builder, ui_data, -1, &error))
    g_error ("Failed to load UI: %s", error->message);

  window = GTK_WINDOW (gtk_builder_get_object (builder, "main_window"));
  source_stack = GTK_STACK (gtk_builder_get_object (builder, "source_stack"));

  g_signal_connect_swapped (window,
                            "close-request",
                            G_CALLBACK (g_main_loop_quit),
                            main_loop);

  g_print ("Project directory: %s\n", project_dir);
  g_print ("Command: %s\n", command_name);
  g_print ("Arguments: ");

  for (int i = 0; i < command_argc; i++)
    {
      g_print ("%s", command_argv[i]);
      if (i < command_argc - 1)
        g_print (" ");
    }
  g_print ("\n");

  gtk_window_present (window);

  command = foundry_command_new (context);
  foundry_command_set_argv (command, (const char * const *)command_argv);

  debugger_manager = foundry_context_dup_debugger_manager (context);
  build_manager = foundry_context_dup_build_manager (context);

  pipeline = dex_await_object (foundry_build_manager_load_pipeline (build_manager), &error);
  g_assert_no_error (error);
  g_assert_nonnull (pipeline);

  provider = dex_await_object (foundry_debugger_manager_discover (debugger_manager, pipeline, command), &error);
  g_assert_no_error (error);
  g_assert_nonnull (provider);

  debugger = dex_await_object (foundry_debugger_provider_load_debugger (provider, pipeline), &error);
  g_assert_no_error (error);
  g_assert_nonnull (debugger);

  g_print ("Using debugger `%s`\n", G_OBJECT_TYPE_NAME (debugger));

  g_assert_true (FOUNDRY_IS_DEBUGGER (debugger));
  g_assert_true (G_IS_ACTION_GROUP (debugger));

  target = foundry_debugger_target_command_new (command);
  dex_await (foundry_debugger_connect_to_target (debugger, target), &error);
  g_assert_no_error (error);

  dex_await (foundry_debugger_move (debugger, FOUNDRY_DEBUGGER_MOVEMENT_START), &error);
  g_assert_no_error (error);

  actions = foundry_debugger_actions_new (debugger, NULL);
  gtk_widget_insert_action_group (GTK_WIDGET (window), "debugger", G_ACTION_GROUP (actions));

  return NULL;
}

static void
print_usage (const char *program_name)
{
  g_printerr ("usage: %s PROJECT_DIR COMMAND [ARGS...]\n", program_name);
  g_printerr ("\n");
  g_printerr ("  PROJECT_DIR  Path to the project directory\n");
  g_printerr ("  COMMAND      Name of the command to debug\n");
  g_printerr ("  ARGS...      Additional arguments for the command\n");
  g_printerr ("\n");
  g_printerr ("Example: %s /path/to/project ./myprogram arg1 arg2\n", program_name);
}

int
main (int   argc,
      char *argv[])
{
  if (argc < 3)
    {
      print_usage (argv[0]);
      return 1;
    }

  project_dir = argv[1];
  command_name = argv[2];
  command_argv = &argv[2];
  command_argc = argc - 2;

  gtk_init ();

  main_loop = g_main_loop_new (NULL, FALSE);
  dex_future_disown (dex_scheduler_spawn (NULL, 8*1024*1024, main_fiber, NULL, NULL));
  g_main_loop_run (main_loop);

  return 0;
}
