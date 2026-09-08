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
  gboolean           stop_before_reply;
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
  g_assert_cmpstr (command, ==, "continue");

  if (session->stop_before_reply)
    send_stop (session);

  send_message (session,
                FOUNDRY_JSON_OBJECT_NEW ("type", "response", "command", command,
                                         "request_seq", FOUNDRY_JSON_NODE_PUT_INT (seq),
                                         "success", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                         "body", "{", "allThreadsContinued", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE), "}"));
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
}

static void
run_continue (gboolean stop_before_reply)
{
  Session session = { 0 };
  g_autoptr(GListModel) threads = NULL;
  g_autoptr(FoundryDebuggerThread) thread = NULL;
  g_autoptr(GError) error = NULL;

  session_init (&session);
  session.stop_before_reply = stop_before_reply;
  threads = foundry_debugger_list_threads (FOUNDRY_DEBUGGER (session.debugger));
  g_assert_cmpuint (g_list_model_get_n_items (threads), ==, 1);
  thread = g_list_model_get_item (threads, 0);
  g_assert_true (foundry_debugger_thread_is_stopped (thread));
  g_assert_true (dex_await (dex_future_with_timeout_seconds (
                            foundry_debugger_thread_move (thread, FOUNDRY_DEBUGGER_MOVEMENT_CONTINUE), 5),
                           &error));
  g_assert_no_error (error);
  g_assert_cmpint (foundry_debugger_thread_is_stopped (thread), ==, stop_before_reply);
  session_clear (&session);
}

static void
test_continue (void)
{
  run_continue (FALSE);
}

static void
test_continue_stopped (void)
{
  run_continue (TRUE);
}

int
main (int argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_data_func ("/Foundry/Dap/continue", test_continue, (GTestDataFunc) test_from_fiber);
  g_test_add_data_func ("/Foundry/Dap/continue-stopped", test_continue_stopped, (GTestDataFunc) test_from_fiber);
  return g_test_run ();
}
