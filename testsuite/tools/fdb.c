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

#include "egg-line.h"

static const char *dirpath;
static const char * const *command_argv;
static GMainLoop *main_loop;
static FoundryDebugger *g_debugger;
static char *current_thread;
static char *current_frame;

static gboolean
await (DexFuture  *future,
       GError    **error)
{
  return dex_await (dex_future_first (future,
                                      dex_unix_signal_new (SIGINT),
                                      NULL),
                    error);
}

static FoundryDebuggerThread *
get_thread (void)
{
  g_autoptr(GListModel) threads = NULL;

  if (current_thread == NULL)
    current_thread = g_strdup ("1");

  if ((threads = foundry_debugger_list_threads (g_debugger)))
    {
      guint n_threads = g_list_model_get_n_items (threads);

      for (guint i = 0; i < n_threads; i++)
        {
          g_autoptr(FoundryDebuggerThread) thread = g_list_model_get_item (threads, i);
          g_autofree char *thread_id = foundry_debugger_thread_dup_id (thread);

          if (g_strcmp0 (thread_id, current_thread) == 0)
            return g_steal_pointer (&thread);
        }
    }

  if (!g_str_equal (current_thread, "1"))
    {
      g_clear_pointer (&current_thread, g_free);
      return get_thread ();
    }

  return NULL;
}

static FoundryDebuggerStackFrame *
get_frame (void)
{
  g_autoptr(FoundryDebuggerThread) thread = NULL;
  g_autoptr(GListModel) frames = NULL;
  guint n_frames;

  if (!(thread = get_thread ()))
    return NULL;

  if (!(frames = dex_await_object (foundry_debugger_thread_list_frames (thread), NULL)))
    return NULL;

  n_frames = g_list_model_get_n_items (frames);

  for (guint i = 0; i < n_frames; i++)
    {
      g_autoptr(FoundryDebuggerStackFrame) frame = g_list_model_get_item (frames, i);
      g_autofree char *id = g_strdup_printf ("%u", i);

      if (current_frame == NULL || g_strcmp0 (id, current_frame) == 0)
        return g_steal_pointer (&frame);
    }

  return NULL;
}

static gboolean
movement (FoundryDebuggerMovement   movement,
          GError                  **error)
{
  g_autoptr(FoundryDebuggerThread) thread = get_thread ();
  DexFuture *future;

  g_set_str (&current_frame, NULL);

  if (thread)
    future = foundry_debugger_thread_move (thread, movement);
  else
    future = foundry_debugger_move (g_debugger, movement);

  if (!await (future, error))
    return EGG_LINE_STATUS_FAILURE;

  return EGG_LINE_STATUS_OK;
}

static EggLineStatus
fdb_step_over (EggLine  *line,
               int       argc,
               char    **argv,
               GError  **error)
{
  return movement (FOUNDRY_DEBUGGER_MOVEMENT_STEP_OVER, error);
}

static EggLineStatus
fdb_step_in (EggLine  *line,
             int       argc,
             char    **argv,
             GError  **error)
{
  return movement (FOUNDRY_DEBUGGER_MOVEMENT_STEP_IN, error);
}

static EggLineStatus
fdb_step_out (EggLine  *line,
              int       argc,
              char    **argv,
              GError  **error)
{
  return movement (FOUNDRY_DEBUGGER_MOVEMENT_STEP_OUT, error);
}

static EggLineStatus
fdb_backtrace (EggLine  *line,
               int       argc,
               char    **argv,
               GError  **error)
{
  g_autoptr(FoundryDebuggerThread) thread = get_thread ();
  g_autoptr(GListModel) frames = NULL;
  g_autofree char *thread_id = NULL;
  guint n_frames;

  if (thread == NULL)
    {
      g_print ("No threads\n");
      return EGG_LINE_STATUS_OK;
    }

  frames = dex_await_object (foundry_debugger_thread_list_frames (thread), error);
  thread_id = foundry_debugger_thread_dup_id (thread);

  if (frames == NULL)
    return EGG_LINE_STATUS_OK;

  n_frames = g_list_model_get_n_items (frames);

  for (guint j = 0; j < n_frames; j++)
    {
      g_autoptr(FoundryDebuggerStackFrame) frame = g_list_model_get_item (frames, j);
      g_autoptr(FoundryDebuggerSource) source = NULL;
      g_autoptr(GListModel) params = NULL;
      g_autofree char *name = foundry_debugger_stack_frame_dup_name (frame);
      g_autofree char *module_id = foundry_debugger_stack_frame_dup_module_id (frame);
      g_autofree char *id = foundry_debugger_stack_frame_dup_id (frame);
      g_autofree char *path = NULL;
      guint64 pc = foundry_debugger_stack_frame_get_instruction_pointer (frame);
      guint bl = 0, el = 0, bc = 0, ec = 0;

      if ((source = foundry_debugger_stack_frame_dup_source (frame)))
        {
          foundry_debugger_stack_frame_get_source_range (frame, &bl, &bc, &el, &ec);
          path = foundry_debugger_source_dup_path (source);
        }

      g_print ("%s: #%02u (%s): %s: %s (@ 0x%"G_GINT64_MODIFIER"x): [%s %u:%u-%u:%u]\n",
               thread_id, j, id, module_id, name, pc,
               path ? path : "no source", bl, bc, el, ec);

      if ((params = dex_await_object (foundry_debugger_stack_frame_list_params (frame), NULL)))
        {
          guint n_params = g_list_model_get_n_items (params);

          if (n_params > 0)
            g_print ("  ");

          for (guint p = 0; p < n_params; p++)
            {
              g_autoptr(FoundryDebuggerVariable) variable = g_list_model_get_item (params, p);
              g_autofree char *vname = foundry_debugger_variable_dup_name (variable);
              g_autofree char *vvalue = foundry_debugger_variable_dup_value (variable);
              g_autofree char *type_name = foundry_debugger_variable_dup_type_name (variable);

              if (type_name)
                g_print ("%s %s = %s", type_name, vname, vvalue);
              else
                g_print ("%s = %s", vname, vvalue);

              if (p + 1 < n_params)
                g_print (", ");
            }

          if (n_params > 0)
            g_print ("\n");
        }
    }

  return EGG_LINE_STATUS_OK;
}

static EggLineStatus
fdb_threads (EggLine  *line,
             int       argc,
             char    **argv,
             GError  **error)
{
  g_autoptr(FoundryDebuggerThread) current = get_thread ();
  g_autoptr(GListModel) threads = foundry_debugger_list_threads (g_debugger);
  guint n_threads = g_list_model_get_n_items (threads);

  for (guint i = 0; i < n_threads; i++)
    {
      g_autoptr(FoundryDebuggerThread) thread = g_list_model_get_item (threads, i);
      g_autofree char *thread_id = foundry_debugger_thread_dup_id (thread);
      gboolean stopped = foundry_debugger_thread_is_stopped (thread);

      if (current == thread)
        g_print ("> ");

      g_print ("Thread %s: %s\n", thread_id, stopped ? "stopped" : "running");
    }

  g_print ("%u threads.\n", n_threads);

  return EGG_LINE_STATUS_OK;
}

static EggLineStatus
fdb_switch (EggLine  *line,
            int       argc,
            char    **argv,
            GError  **error)
{
  if (argc > 0)
    g_set_str (&current_thread, argv[0]);

  return EGG_LINE_STATUS_OK;
}

static EggLineStatus
fdb_frame (EggLine  *line,
           int       argc,
           char    **argv,
           GError  **error)
{
  if (argc > 0)
    g_set_str (&current_frame, argv[0]);

  return EGG_LINE_STATUS_OK;
}

static EggLineStatus
fdb_variables (EggLine     *line,
               const char  *kind,
               GError     **error)
{
  g_autoptr(FoundryDebuggerStackFrame) stack_frame = get_frame ();
  g_autoptr(GListModel) model = NULL;
  guint n_items;

  if (!stack_frame)
    return EGG_LINE_STATUS_OK;

  if (g_str_equal (kind, "locals"))
    model = dex_await_object (foundry_debugger_stack_frame_list_locals (stack_frame), error);
  else if (g_str_equal (kind, "registers"))
    model = dex_await_object (foundry_debugger_stack_frame_list_registers (stack_frame), error);
  else if (g_str_equal (kind, "params"))
    model = dex_await_object (foundry_debugger_stack_frame_list_params (stack_frame), error);
  else
    return EGG_LINE_STATUS_BAD_ARGS;

  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryDebuggerVariable) var = g_list_model_get_item (model, i);
      g_autofree char *name = foundry_debugger_variable_dup_name (var);
      g_autofree char *value = foundry_debugger_variable_dup_value (var);
      g_autofree char *type_name = foundry_debugger_variable_dup_type_name (var);

      g_print ("%s %s = %s\n", type_name, name, value);
    }

  return EGG_LINE_STATUS_OK;
}

static EggLineStatus
fdb_locals (EggLine  *line,
            int       argc,
            char    **argv,
            GError  **error)
{
  return fdb_variables (line, "locals", error);
}

static EggLineStatus
fdb_registers (EggLine  *line,
               int       argc,
               char    **argv,
               GError  **error)
{
  return fdb_variables (line, "registers", error);
}

static EggLineStatus
fdb_params (EggLine  *line,
            int       argc,
            char    **argv,
            GError  **error)
{
  return fdb_variables (line, "params", error);
}

static EggLineStatus
fdb_quit (EggLine  *line,
          int       argc,
          char    **argv,
          GError  **error)
{
  exit (0);
  return EGG_LINE_STATUS_OK;
}

static EggLineStatus
fdb_iterate (EggLine  *line,
             int       argc,
             char    **argv,
             GError  **error)
{
  dex_await (dex_timeout_new_msec (50), NULL);
  return EGG_LINE_STATUS_OK;
}

static const EggLineCommand commands[] = {
  { .name = "step-over", .callback = fdb_step_over, },
  { .name = "next", .callback = fdb_step_over, },

  { .name = "step-in", .callback = fdb_step_in, },

  { .name = "step-out", .callback = fdb_step_out, },
  { .name = "finish", .callback = fdb_step_out, },

  { .name = "backtrace", .callback = fdb_backtrace , },
  { .name = "bt", .callback = fdb_backtrace , },

  { .name = "frame", .callback = fdb_frame, },
  { .name = "switch", .callback = fdb_switch, },
  { .name = "threads", .callback = fdb_threads, },

  { .name = "locals", .callback = fdb_locals, },
  { .name = "params", .callback = fdb_params, },
  { .name = "registers", .callback = fdb_registers, },

  { .name = "iterate", .callback = fdb_iterate, },

  { .name = "quit", .callback = fdb_quit, },

  {NULL}
};

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

static void
handle_module (GListModel *model,
               guint       position,
               guint       removed,
               guint       added,
               gpointer    user_data)
{
  if (added == 0)
    return;

  for (guint i = 0; i < added; i++)
    {
      g_autoptr(FoundryDebuggerModule) item = g_list_model_get_item (model, position + i);
      g_autofree char *id = foundry_debugger_module_dup_id (item);

      g_print ("Module %s added\n", id);
    }
}

static void
handle_thread (GListModel *model,
               guint       position,
               guint       removed,
               guint       added,
               gpointer    user_data)
{
  if (removed)
    g_print ("%u thread(s) exited\n", removed);

  if (added == 0)
    return;

  for (guint i = 0; i < added; i++)
    {
      g_autoptr(FoundryDebuggerThread) item = g_list_model_get_item (model, position + i);
      g_autofree char *id = foundry_debugger_thread_dup_id (item);

      g_print ("Thread %s added\n", id);
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
  g_autoptr(GListModel) modules = NULL;
  g_autoptr(GListModel) threads = NULL;
  g_autoptr(EggLine) egg_line = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  g_autofree char *title = NULL;
  g_autofree char *name = NULL;
  g_autofree char *prompt = NULL;

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

  modules = foundry_debugger_list_modules (debugger);
  g_signal_connect (modules, "items-changed", G_CALLBACK (handle_module), NULL);
  handle_module (modules, 0, 0, g_list_model_get_n_items (modules), NULL);

  threads = foundry_debugger_list_threads (debugger);
  g_signal_connect (threads, "items-changed", G_CALLBACK (handle_thread), NULL);
  handle_thread (threads, 0, 0, g_list_model_get_n_items (threads), NULL);

  if (!dex_await (foundry_debugger_initialize (debugger), &error))
    g_error ("Failed to initialize debugger: %s", error->message);

  g_print ("\n");
  g_print ("Commands:\n");
  g_print ("  next / step-over\n");
  g_print ("  step-in\n");
  g_print ("  finish / step-out\n");
  g_print ("  switch THREAD_NR\n");
  g_print ("  frame FRAME_NR\n");
  g_print ("  threads\n");
  g_print ("  backtrace\n");
  g_print ("  quit\n");

  stdin_stream = g_unix_input_stream_new (STDIN_FILENO, FALSE);
  target = foundry_debugger_target_command_new (command);

  if (!dex_await (foundry_debugger_connect_to_target (debugger, target), &error))
    g_error ("Failed to connect to target: %s", error->message);

  g_debugger = debugger;

  name = foundry_debugger_dup_name (debugger);
  prompt = g_strdup_printf ("Foundry Debugger (%s) ", name);

  egg_line = egg_line_new ();
  egg_line_set_commands (egg_line, commands);
  egg_line_set_prompt (egg_line, prompt);

  egg_line_run (egg_line);

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
