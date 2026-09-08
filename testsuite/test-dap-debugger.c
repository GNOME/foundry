/*
 * Copyright 2026 Christian Hergert
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <foundry.h>
#include <sys/socket.h>

#include "foundry-dap-driver-private.h"
#include "test-util.h"

typedef struct
{
  FoundryDapDebugger parent_instance;
} TestDebugger;

typedef FoundryDapDebuggerClass TestDebuggerClass;

GType test_debugger_get_type (void);

G_DEFINE_TYPE (TestDebugger, test_debugger, FOUNDRY_TYPE_DAP_DEBUGGER)

static void
test_debugger_class_init (TestDebuggerClass *klass)
{
}

static void
test_debugger_init (TestDebugger *self)
{
}

typedef struct
{
  FoundryDapDebugger *debugger;
  FoundryDapDriver   *adapter;
  DexPromise        *stopped;
  DexPromise        *breakpoints_received;
  const char        *movement_command;
  gboolean           stop_before_reply;
  gboolean           movement_success;
  gboolean           all_threads_continued;
  gboolean           include_all_threads_continued;
  gboolean           breakpoint_success;
  gboolean           scope_hints;
  gint64             variables_reference;
} Session;

static void
send_message (Session  *session,
              JsonNode *node)
{
  g_autoptr(JsonNode) owned = node;

  dex_future_disown (foundry_dap_driver_send (session->adapter, node));
}

static void
send_stop (Session *session)
{
  send_message (session,
                FOUNDRY_JSON_OBJECT_NEW ("type", "event", "event", "stopped",
                                         "body", "{", "reason", "breakpoint",
                                         "threadId", FOUNDRY_JSON_NODE_PUT_INT (1), "allThreadsStopped", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE), "}"));
}

static gboolean
handle_request (FoundryDapDriver *adapter,
                JsonNode         *request,
                Session          *session)
{
  const char *command = NULL;
  gint64 seq = 0;

  g_assert_true (FOUNDRY_JSON_OBJECT_PARSE (request,
                                            "command", FOUNDRY_JSON_NODE_GET_STRING (&command),
                                            "seq", FOUNDRY_JSON_NODE_GET_INT (&seq)));
  if (g_str_equal (command, "setBreakpoints"))
    {
      JsonObject *arguments = json_object_get_object_member (json_node_get_object (request), "arguments");
      JsonObject *source = json_object_get_object_member (arguments, "source");
      JsonArray *breakpoints = json_object_get_array_member (arguments, "breakpoints");
      JsonObject *first;
      JsonObject *second = NULL;
      g_autoptr(JsonNode) body = NULL;

      g_assert_nonnull (session->breakpoints_received);
      if (!session->breakpoint_success)
        {
          send_message (session,
                        FOUNDRY_JSON_OBJECT_NEW ("type", "response", "command", command,
                                                 "request_seq", FOUNDRY_JSON_NODE_PUT_INT (seq),
                                                 "success", FOUNDRY_JSON_NODE_PUT_BOOLEAN (FALSE),
                                                 "message", "Breakpoint update failed"));
          return TRUE;
        }

      g_assert_cmpstr (json_object_get_string_member (source, "path"), ==, "/tmp/test-source.c");
      g_assert_cmpuint (json_array_get_length (breakpoints), >=, 1);
      g_assert_cmpuint (json_array_get_length (breakpoints), <=, 2);
      first = json_array_get_object_element (breakpoints, 0);
      if (json_array_get_length (breakpoints) == 2)
        {
          second = json_array_get_object_element (breakpoints, 1);
          g_assert_cmpint (json_object_get_int_member (first, "line"), ==, 13);
          g_assert_cmpint (json_object_get_int_member (first, "column"), ==, 7);
          g_assert_cmpint (json_object_get_int_member (second, "line"), ==, 22);
          g_assert_false (json_object_has_member (second, "column"));
          body = json_from_string (
            "{\"breakpoints\":[{\"id\":1,\"verified\":true,\"line\":13},"
            "{\"id\":2,\"verified\":true,\"line\":22}]}", NULL);
        }
      else
        {
          g_assert_cmpint (json_object_get_int_member (first, "line"), ==, 22);
          body = json_from_string (
            "{\"breakpoints\":[{\"id\":2,\"verified\":true,\"line\":22}]}", NULL);
        }
      send_message (session,
                    FOUNDRY_JSON_OBJECT_NEW ("type", "response", "command", command,
                                             "request_seq", FOUNDRY_JSON_NODE_PUT_INT (seq),
                                             "success", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                             "body", FOUNDRY_JSON_NODE_PUT_NODE (body)));
      if (dex_future_is_pending (DEX_FUTURE (session->breakpoints_received)))
        dex_promise_resolve_boolean (session->breakpoints_received, TRUE);
      return TRUE;
    }

  if (g_str_equal (command, "stackTrace"))
    {
      g_autoptr(JsonNode) body = json_from_string (
        "{\"stackFrames\":[{\"id\":7,\"name\":\"test\",\"line\":1,\"column\":1}]}", NULL);

      send_message (session,
                    FOUNDRY_JSON_OBJECT_NEW ("type", "response", "command", command,
                                             "request_seq", FOUNDRY_JSON_NODE_PUT_INT (seq),
                                             "success", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                             "body", FOUNDRY_JSON_NODE_PUT_NODE (body)));
      return TRUE;
    }

  if (g_str_equal (command, "scopes"))
    {
      g_autoptr(JsonNode) body = NULL;
      gint64 frame_id = 0;

      g_assert_true (FOUNDRY_JSON_OBJECT_PARSE (request, "arguments", "{",
                                                "frameId", FOUNDRY_JSON_NODE_GET_INT (&frame_id), "}"));
      g_assert_cmpint (frame_id, ==, 7);
      body = json_from_string (session->scope_hints ?
        "{\"scopes\":["
        "{\"name\":\"Locals\",\"presentationHint\":\"arguments\",\"variablesReference\":11},"
        "{\"name\":\"Local variables\",\"presentationHint\":\"locals\",\"variablesReference\":12},"
        "{\"name\":\"CPU\",\"presentationHint\":\"registers\",\"variablesReference\":13}]}" :
        "{\"scopes\":["
        "{\"name\":\"Arguments\",\"variablesReference\":11},"
        "{\"name\":\"Locals\",\"variablesReference\":12},"
        "{\"name\":\"Registers\",\"variablesReference\":13}]}", NULL);
      send_message (session,
                    FOUNDRY_JSON_OBJECT_NEW ("type", "response", "command", command,
                                             "request_seq", FOUNDRY_JSON_NODE_PUT_INT (seq),
                                             "success", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                             "body", FOUNDRY_JSON_NODE_PUT_NODE (body)));
      return TRUE;
    }

  if (g_str_equal (command, "variables"))
    {
      g_autoptr(JsonNode) body = json_from_string (
        "{\"variables\":[{\"name\":\"value\",\"value\":\"42\",\"variablesReference\":0}]}", NULL);

      g_assert_true (FOUNDRY_JSON_OBJECT_PARSE (request, "arguments", "{",
                                                "variablesReference", FOUNDRY_JSON_NODE_GET_INT (&session->variables_reference), "}"));
      send_message (session,
                    FOUNDRY_JSON_OBJECT_NEW ("type", "response", "command", command,
                                             "request_seq", FOUNDRY_JSON_NODE_PUT_INT (seq),
                                             "success", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                             "body", FOUNDRY_JSON_NODE_PUT_NODE (body)));
      return TRUE;
    }

  g_assert_nonnull (session->movement_command);
  g_assert_cmpstr (command, ==, session->movement_command);

  if (session->stop_before_reply)
    send_stop (session);

  if (!session->movement_success)
    send_message (session,
                  FOUNDRY_JSON_OBJECT_NEW ("type", "response", "command", command,
                                           "request_seq", FOUNDRY_JSON_NODE_PUT_INT (seq),
                                           "success", FOUNDRY_JSON_NODE_PUT_BOOLEAN (FALSE),
                                           "message", "Movement failed",
                                           "body", "{",
                                             "allThreadsContinued", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                           "}"));
  else if (session->include_all_threads_continued)
    send_message (session,
                  FOUNDRY_JSON_OBJECT_NEW (
                    "type", "response", "command", command,
                    "request_seq", FOUNDRY_JSON_NODE_PUT_INT (seq),
                    "success", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                    "body", "{",
                      "allThreadsContinued", FOUNDRY_JSON_NODE_PUT_BOOLEAN (session->all_threads_continued),
                    "}"));
  else
    send_message (session,
                  FOUNDRY_JSON_OBJECT_NEW ("type", "response", "command", command,
                                           "request_seq", FOUNDRY_JSON_NODE_PUT_INT (seq),
                                           "success", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE)));
  return TRUE;
}

static void
on_event (FoundryDebugger      *debugger,
          FoundryDebuggerEvent *event,
          Session              *session)
{
  if (FOUNDRY_IS_DEBUGGER_STOP_EVENT (event) &&
      dex_future_is_pending (DEX_FUTURE (session->stopped)))
    dex_promise_resolve_boolean (session->stopped, TRUE);
}

static void
session_init (Session *session)
{
  g_autoptr(GSocket) client = NULL;
  g_autoptr(GSocket) server = NULL;
  g_autoptr(GSocketConnection) client_connection = NULL;
  g_autoptr(GSocketConnection) server_connection = NULL;
  g_autoptr(GError) error = NULL;
  int fds[2];

  g_assert_cmpint (socketpair (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds), ==, 0);
  client = g_socket_new_from_fd (fds[0], &error);
  g_assert_no_error (error);
  server = g_socket_new_from_fd (fds[1], &error);
  g_assert_no_error (error);
  client_connection = g_socket_connection_factory_create_connection (client);
  server_connection = g_socket_connection_factory_create_connection (server);

  session->stopped = dex_promise_new ();
  session->breakpoint_success = TRUE;
  session->debugger = g_object_new (test_debugger_get_type (), "stream", client_connection, NULL);
  session->adapter = foundry_dap_driver_new (G_IO_STREAM (server_connection), FOUNDRY_JSONRPC_STYLE_HTTP);
  g_signal_connect (session->adapter, "handle-request", G_CALLBACK (handle_request), session);
  g_signal_connect (session->debugger, "event", G_CALLBACK (on_event), session);
  foundry_dap_driver_start (session->adapter);

  send_message (session,
                FOUNDRY_JSON_OBJECT_NEW ("type", "event", "event", "thread",
                                         "body", "{", "reason", "started", "threadId", FOUNDRY_JSON_NODE_PUT_INT (1), "}"));
  send_stop (session);
  g_assert_true (dex_await (dex_future_with_timeout_seconds (dex_ref (session->stopped), 5), &error));
  g_assert_no_error (error);
}

static void
session_clear (Session *session)
{
  g_signal_handlers_disconnect_by_data (session->debugger, session);
  g_signal_handlers_disconnect_by_data (session->adapter, session);
  foundry_dap_driver_stop (session->adapter);
  g_clear_object (&session->debugger);
  g_clear_object (&session->adapter);
  dex_clear (&session->stopped);
  dex_clear (&session->breakpoints_received);
}

static void
run_movement (FoundryDebuggerMovement  movement,
              const char              *command,
              gboolean                 success,
              gboolean                 stop_before_reply,
              gboolean                 include_all_threads_continued,
              gboolean                 all_threads_continued)
{
  Session session = { 0 };
  g_autoptr(GListModel) threads = NULL;
  g_autoptr(FoundryDebuggerThread) thread = NULL;
  g_autoptr(GError) error = NULL;

  session_init (&session);
  session.movement_command = command;
  session.movement_success = success;
  session.stop_before_reply = stop_before_reply;
  session.include_all_threads_continued = include_all_threads_continued;
  session.all_threads_continued = all_threads_continued;
  threads = foundry_debugger_list_threads (FOUNDRY_DEBUGGER (session.debugger));
  g_assert_cmpuint (g_list_model_get_n_items (threads), ==, 1);
  thread = g_list_model_get_item (threads, 0);
  g_assert_true (foundry_debugger_thread_is_stopped (thread));
  g_assert_cmpint (dex_await (dex_future_with_timeout_seconds (
                              foundry_debugger_thread_move (thread, movement), 5),
                             &error),
                   ==, success);
  if (success)
    g_assert_no_error (error);
  else
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_cmpint (foundry_debugger_thread_is_stopped (thread), ==,
                   !success || stop_before_reply);
  session_clear (&session);
}

static void
test_continue (void)
{
  run_movement (FOUNDRY_DEBUGGER_MOVEMENT_CONTINUE, "continue",
                TRUE, FALSE, TRUE, TRUE);
}

static void
test_continue_stopped (void)
{
  run_movement (FOUNDRY_DEBUGGER_MOVEMENT_CONTINUE, "continue",
                TRUE, TRUE, TRUE, TRUE);
}

static void
test_continue_single_thread (void)
{
  run_movement (FOUNDRY_DEBUGGER_MOVEMENT_CONTINUE, "continue",
                TRUE, FALSE, TRUE, FALSE);
}

static void
test_movement_failed (void)
{
  run_movement (FOUNDRY_DEBUGGER_MOVEMENT_CONTINUE, "continue",
                FALSE, FALSE, FALSE, FALSE);
}

static void
test_steps (void)
{
  const struct
  {
    FoundryDebuggerMovement movement;
    const char             *command;
  } steps[] = {
    { FOUNDRY_DEBUGGER_MOVEMENT_STEP_IN, "stepIn" },
    { FOUNDRY_DEBUGGER_MOVEMENT_STEP_OVER, "next" },
    { FOUNDRY_DEBUGGER_MOVEMENT_STEP_OUT, "stepOut" },
  };

  for (guint i = 0; i < G_N_ELEMENTS (steps); i++)
    run_movement (steps[i].movement, steps[i].command,
                  TRUE, FALSE, FALSE, FALSE);
}

static void
run_scopes (gboolean hints)
{
  Session session = { 0 };
  g_autoptr(GListModel) threads = NULL;
  g_autoptr(GListModel) frames = NULL;
  g_autoptr(FoundryDebuggerThread) thread = NULL;
  g_autoptr(FoundryDebuggerStackFrame) frame = NULL;
  g_autoptr(GError) error = NULL;
  DexFuture *(*list_variables[]) (FoundryDebuggerStackFrame *) = {
    foundry_debugger_stack_frame_list_params,
    foundry_debugger_stack_frame_list_locals,
    foundry_debugger_stack_frame_list_registers,
  };

  session_init (&session);
  session.scope_hints = hints;
  threads = foundry_debugger_list_threads (FOUNDRY_DEBUGGER (session.debugger));
  thread = g_list_model_get_item (threads, 0);
  frames = dex_await_object (dex_future_with_timeout_seconds (
                              foundry_debugger_thread_list_frames (thread), 5), &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_list_model_get_n_items (frames), ==, 1);
  frame = g_list_model_get_item (frames, 0);

  for (guint i = 0; i < G_N_ELEMENTS (list_variables); i++)
    {
      g_autoptr(GListModel) variables = NULL;

      session.variables_reference = 0;
      variables = dex_await_object (dex_future_with_timeout_seconds (list_variables[i] (frame), 5), &error);
      g_assert_no_error (error);
      g_assert_cmpuint (g_list_model_get_n_items (variables), ==, 1);
      g_assert_cmpint (session.variables_reference, ==, 11 + i);
    }

  session_clear (&session);
}

static void
test_scope_hints (void)
{
  run_scopes (TRUE);
}

static void
test_scope_names (void)
{
  run_scopes (FALSE);
}

static void
test_source_breakpoints (void)
{
  Session session = { 0 };
  g_autoptr(FoundryDebuggerTrapParams) params = NULL;
  g_autoptr(GListModel) traps = NULL;
  g_autoptr(FoundryDebuggerTrap) first_trap = NULL;
  g_autoptr(FoundryDebuggerTrap) second_trap = NULL;
  g_autoptr(DexFuture) first = NULL;
  g_autoptr(DexFuture) second = NULL;
  g_autofree char *first_id = NULL;
  g_autofree char *second_id = NULL;
  g_autofree char *first_path = NULL;
  g_autofree char *second_path = NULL;
  g_autoptr(GError) error = NULL;
  FoundryDebuggerTrapParams *first_params;
  guint first_line;
  guint second_line;

  session_init (&session);
  session.breakpoints_received = dex_promise_new ();
  params = foundry_debugger_trap_params_new ();
  foundry_debugger_trap_params_set_path (params, "/tmp/test-source.c");
  foundry_debugger_trap_params_set_line (params, 13);
  foundry_debugger_trap_params_set_line_offset (params, 7);
  first = foundry_debugger_trap (FOUNDRY_DEBUGGER (session.debugger), params);
  foundry_debugger_trap_params_set_line (params, 22);
  foundry_debugger_trap_params_set_line_offset (params, G_MAXUINT);
  second = foundry_debugger_trap (FOUNDRY_DEBUGGER (session.debugger), params);

  g_assert_true (dex_await (dex_future_with_timeout_seconds (dex_ref (first), 5), &error));
  g_assert_no_error (error);
  g_assert_true (dex_await (dex_future_with_timeout_seconds (dex_ref (second), 5), &error));
  g_assert_no_error (error);
  g_assert_true (dex_await (dex_future_with_timeout_seconds (dex_ref (session.breakpoints_received), 5), &error));
  g_assert_no_error (error);

  traps = foundry_debugger_list_traps (FOUNDRY_DEBUGGER (session.debugger));
  g_assert_cmpuint (g_list_model_get_n_items (traps), ==, 2);
  first_trap = g_list_model_get_item (traps, 0);
  second_trap = g_list_model_get_item (traps, 1);
  g_assert_true (FOUNDRY_IS_DEBUGGER_BREAKPOINT (first_trap));
  g_assert_true (FOUNDRY_IS_DEBUGGER_BREAKPOINT (second_trap));
  g_assert_true (foundry_debugger_trap_is_armed (first_trap));
  g_assert_true (foundry_debugger_trap_is_armed (second_trap));
  first_params = g_object_get_data (G_OBJECT (first_trap), "dap-params");
  g_assert_nonnull (first_params);
  first_id = foundry_debugger_trap_dup_id (first_trap);
  second_id = foundry_debugger_trap_dup_id (second_trap);
  g_assert_cmpstr (first_id, ==, "1");
  g_assert_cmpstr (second_id, ==, "2");
  g_object_get (first_trap,
                "line", &first_line,
                "source-path", &first_path,
                NULL);
  g_object_get (second_trap,
                "line", &second_line,
                "source-path", &second_path,
                NULL);
  g_assert_cmpuint (first_line, ==, 13);
  g_assert_cmpuint (second_line, ==, 22);
  g_assert_cmpstr (first_path, ==, "/tmp/test-source.c");
  g_assert_cmpstr (second_path, ==, "/tmp/test-source.c");

  session.breakpoint_success = FALSE;
  dex_clear (&first);
  first = foundry_debugger_trap_disarm (first_trap);
  g_assert_false (dex_await (dex_future_with_timeout_seconds (dex_ref (first), 5), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_clear_error (&error);
  g_assert_null (g_object_get_data (G_OBJECT (first_params), "dap-disabled"));

  dex_clear (&first);
  first = foundry_debugger_trap_remove (first_trap);
  g_assert_false (dex_await (dex_future_with_timeout_seconds (dex_ref (first), 5), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_clear_error (&error);
  g_assert_cmpuint (g_list_model_get_n_items (traps), ==, 2);

  session.breakpoint_success = TRUE;
  dex_clear (&first);
  first = foundry_debugger_trap_remove (first_trap);
  g_assert_true (dex_await (dex_future_with_timeout_seconds (dex_ref (first), 5), &error));
  g_assert_no_error (error);
  g_assert_cmpuint (g_list_model_get_n_items (traps), ==, 1);
  session_clear (&session);
}

static void
test_dispose_pending_breakpoints (void)
{
  Session session = { 0 };
  g_autoptr(FoundryDebuggerTrapParams) params = NULL;
  g_autoptr(DexFuture) pending = NULL;
  g_autoptr(GError) error = NULL;

  session_init (&session);
  params = foundry_debugger_trap_params_new ();
  foundry_debugger_trap_params_set_function (params, "main");
  pending = foundry_debugger_trap (FOUNDRY_DEBUGGER (session.debugger), params);
  g_assert_true (dex_future_is_pending (pending));

  /* Dispose before the idle callback can submit the request. */
  g_object_run_dispose (G_OBJECT (session.debugger));
  g_assert_false (dex_await (dex_future_with_timeout_seconds (dex_ref (pending), 5), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CLOSED);

  /* Repeated disposal must also be harmless. */
  g_object_run_dispose (G_OBJECT (session.debugger));
  session_clear (&session);
}

int
main (int argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_data_func ("/Foundry/Dap/continue", test_continue, (GTestDataFunc) test_from_fiber);
  g_test_add_data_func ("/Foundry/Dap/continue-stopped", test_continue_stopped, (GTestDataFunc) test_from_fiber);
  g_test_add_data_func ("/Foundry/Dap/continue-single-thread", test_continue_single_thread,
                        (GTestDataFunc) test_from_fiber);
  g_test_add_data_func ("/Foundry/Dap/movement-failed", test_movement_failed,
                        (GTestDataFunc) test_from_fiber);
  g_test_add_data_func ("/Foundry/Dap/steps", test_steps, (GTestDataFunc) test_from_fiber);
  g_test_add_data_func ("/Foundry/Dap/scope-hints", test_scope_hints, (GTestDataFunc) test_from_fiber);
  g_test_add_data_func ("/Foundry/Dap/scope-names", test_scope_names, (GTestDataFunc) test_from_fiber);
  g_test_add_data_func ("/Foundry/Dap/source-breakpoints", test_source_breakpoints, (GTestDataFunc) test_from_fiber);
  g_test_add_data_func ("/Foundry/Dap/dispose-pending-breakpoints", test_dispose_pending_breakpoints, (GTestDataFunc) test_from_fiber);
  return g_test_run ();
}
