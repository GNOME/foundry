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
#include <foundry-gtk.h>

static GMainLoop *main_loop;
static const char *project_dir;
static const char *command_name;
static char **command_argv;
static int command_argc;

static GtkDropDown *threads_dropdown;
static GtkListView *stack_trace_listview;
static GtkStringList *threads_model;
static GtkSingleSelection *trace_selection;
static GtkScrolledWindow *scroller;
static FoundryTextManager *text_manager;
static FoundryDebugger *debugger_instance;

static DexFuture *
refresh_stack_trace_cb (DexFuture *completed,
                        gpointer   user_data)
{
  g_autoptr(GListModel) frames = dex_await_object (dex_ref (completed), NULL);
  gtk_single_selection_set_model (trace_selection, frames);
  return dex_future_new_true ();
}

static void
refresh_stack_trace (FoundryDebuggerThread *thread)
{
  if (!thread || !trace_selection)
    return;

  gtk_single_selection_set_model (trace_selection, NULL);
  dex_future_disown (dex_future_finally (foundry_debugger_thread_list_frames (thread),
                                         refresh_stack_trace_cb,
                                         NULL, NULL));
}

static FoundryDebuggerThread *
get_current_thread (void)
{
  FoundryDebuggerThread *ret;
  GListModel *threads;
  guint selected;

  selected = gtk_drop_down_get_selected (threads_dropdown);
  if (selected == GTK_INVALID_LIST_POSITION)
    return NULL;

  if (!(threads = gtk_drop_down_get_model (threads_dropdown)))
    return NULL;

  if ((ret = g_list_model_get_item (threads, selected)))
    g_object_unref (ret);

  return ret;
}

static void
on_thread_stopped_changed (FoundryDebuggerThread *thread,
                           GParamSpec            *pspec,
                           gpointer               user_data)
{
  if (thread == get_current_thread ())
    refresh_stack_trace (thread);
}

static void
on_thread_selection_changed (GtkDropDown *dropdown,
                             gpointer     user_data)
{
  FoundryDebuggerThread *thread;

  g_assert (GTK_IS_DROP_DOWN (dropdown));

  if ((thread = get_current_thread ()))
    refresh_stack_trace (thread);
}

static DexFuture *
file_loaded_cb (DexFuture *completed,
                gpointer   user_data)
{
  FoundryDebuggerStackFrame *frame = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(FoundryTextDocument) document = dex_await_object (dex_ref (completed), &error);
  g_autoptr(GtkTextBuffer) buffer = NULL;
  FoundrySourceView *view;
  GtkTextIter iter;
  guint begin_line;

  if (error != NULL)
    {
      g_printerr ("Error: %s\n", error->message);
      return dex_future_new_true ();
    }

  buffer = GTK_TEXT_BUFFER (foundry_text_document_dup_buffer (document));
  foundry_debugger_stack_frame_get_source_range (frame, &begin_line, NULL, NULL, NULL);
  gtk_text_buffer_get_iter_at_line (buffer, &iter, begin_line);
  gtk_text_buffer_select_range (buffer, &iter, &iter);

  view = FOUNDRY_SOURCE_VIEW (foundry_source_view_new (document));
  gtk_scrolled_window_set_child (scroller, GTK_WIDGET (view));

  return dex_future_new_true ();
}

static DexFuture *
disassembly_loaded_cb (DexFuture *completed,
                      gpointer   user_data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GListModel) model = dex_await_object (dex_ref (completed), &error);
  g_autoptr(GtkTextBuffer) buffer = NULL;
  GtkTextView *view;
  guint n_items;

  if (error != NULL)
    {
      g_printerr ("Error loading disassembly: %s\n", error->message);
      return dex_future_new_true ();
    }

  buffer = gtk_text_buffer_new (NULL);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryDebuggerInstruction) instruction = g_list_model_get_item (G_LIST_MODEL (model), i);
      g_autofree char *text = foundry_debugger_instruction_dup_display_text (instruction);
      guint64 pc = foundry_debugger_instruction_get_instruction_pointer (instruction);
      g_autofree char *line = g_strdup_printf ("0x%"G_GINT64_MODIFIER"x: %s\n", pc, text);

      gtk_text_buffer_insert_at_cursor (buffer, line, -1);
    }

  view = GTK_TEXT_VIEW (gtk_text_view_new_with_buffer (buffer));
  gtk_text_view_set_editable (view, FALSE);
  gtk_text_view_set_monospace (view, TRUE);
  gtk_scrolled_window_set_child (scroller, GTK_WIDGET (view));

  return dex_future_new_true ();
}

static void
on_stack_frame_selection_changed (GtkSelectionModel *selection_model,
                                  guint              position,
                                  guint              n_items,
                                  gpointer           user_data)
{
  g_autoptr(FoundryDebuggerStackFrame) frame = NULL;
  g_autoptr(FoundryDebuggerSource) source = NULL;
  g_autoptr(FoundryOperation) op = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree char *path = NULL;
  GListModel *model;

  position = gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (selection_model));
  model = G_LIST_MODEL (gtk_single_selection_get_model (trace_selection));
  if (!(frame = g_list_model_get_item (model, position)))
    return;

  if (!(source = foundry_debugger_stack_frame_dup_source (frame)))
    {
      /* No source available, try disassembly */
      guint64 instruction_pointer = foundry_debugger_stack_frame_get_instruction_pointer (frame);
      guint64 end_address = instruction_pointer + 100;

      dex_future_disown (dex_future_finally (foundry_debugger_disassemble (debugger_instance, instruction_pointer, end_address),
                                             disassembly_loaded_cb,
                                             NULL, NULL));
      return;
    }

  if (!(path = foundry_debugger_source_dup_path (source)))
    return;

  file = g_file_new_for_path (path);
  op = foundry_operation_new ();

  dex_future_disown (dex_future_finally (foundry_text_manager_load (text_manager, file, op, NULL),
                                         file_loaded_cb,
                                         g_object_ref (frame),
                                         g_object_unref));
}

static void
threads_changed_cb (GListModel *threads,
                    guint       position,
                    guint       removed,
                    guint       added,
                    gpointer    user_data)
{
  for (guint i = 0; i < added; i++)
    {
      g_autoptr(FoundryDebuggerThread) thread = g_list_model_get_item (threads, position + i);

      g_signal_connect (thread,
                        "notify::stopped",
                        G_CALLBACK (on_thread_stopped_changed),
                        NULL);
    }
}

static void
setup_threads_model (FoundryDebugger *debugger)
{
  g_autoptr(GListModel) threads = NULL;
  guint n_threads;

  if (!(threads = foundry_debugger_list_threads (debugger)))
    return;

  gtk_drop_down_set_model (threads_dropdown, threads);

  g_signal_connect (threads_dropdown,
                    "notify::selected",
                    G_CALLBACK (on_thread_selection_changed),
                    NULL);

  g_signal_connect (trace_selection,
                    "selection-changed",
                    G_CALLBACK (on_stack_frame_selection_changed),
                    NULL);

  n_threads = g_list_model_get_n_items (threads);

  g_signal_connect (threads,
                    "items-changed",
                    G_CALLBACK (threads_changed_cb),
                    NULL);

  if (n_threads)
    threads_changed_cb (threads, 0, 0, n_threads, NULL);
}

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

  dex_await (foundry_init (), NULL);

  if (!(path = dex_await_string (foundry_context_discover (project_dir, NULL), &error)))
    g_error ("%s", error->message);

  if (!(context = dex_await_object (foundry_context_new (path, project_dir, FOUNDRY_CONTEXT_FLAGS_NONE, NULL), &error)))
    g_error ("%s", error->message);

  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_resource (builder, "/org/foundry/test-debugger-gtk/test-debugger-gtk.ui", &error))
    g_error ("Failed to load UI: %s", error->message);

  window = GTK_WINDOW (gtk_builder_get_object (builder, "main_window"));

  threads_dropdown = GTK_DROP_DOWN (gtk_builder_get_object (builder, "threads_dropdown"));
  stack_trace_listview = GTK_LIST_VIEW (gtk_builder_get_object (builder, "stack_trace_listview"));
  threads_model = GTK_STRING_LIST (gtk_builder_get_object (builder, "threads_model"));
  trace_selection = GTK_SINGLE_SELECTION (gtk_builder_get_object (builder, "trace_selection"));
  scroller = GTK_SCROLLED_WINDOW (gtk_builder_get_object (builder, "scroller"));
  text_manager = foundry_context_dup_text_manager (context);

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

  g_unsetenv ("G_MESSAGES_DEBUG");

  command = foundry_command_new (context);
  foundry_command_set_argv (command, (const char * const *)command_argv);
  foundry_command_set_cwd (command, g_get_current_dir ());

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

  dex_await (foundry_debugger_initialize (debugger), &error);
  g_assert_no_error (error);

  target = foundry_debugger_target_command_new (command);
  dex_await (foundry_debugger_connect_to_target (debugger, target), &error);
  g_assert_no_error (error);

  actions = foundry_debugger_actions_new (debugger, NULL);
  g_object_bind_property (debugger, "primary-thread", actions, "thread", G_BINDING_SYNC_CREATE);
  gtk_widget_insert_action_group (GTK_WIDGET (window), "debugger", G_ACTION_GROUP (actions));

  debugger_instance = g_object_ref (debugger);
  setup_threads_model (debugger);

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
