/* foundry-dap-debugger.c
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

#include "foundry-command.h"
#include "foundry-dap-debugger.h"
#include "foundry-dap-debugger-log-message-private.h"
#include "foundry-dap-debugger-module-private.h"
#include "foundry-dap-debugger-stop-event-private.h"
#include "foundry-dap-debugger-thread-private.h"
#include "foundry-dap-driver-private.h"
#include "foundry-dap-protocol.h"
#include "foundry-debugger-target.h"
#include "foundry-debugger-target-command.h"
#include "foundry-debugger-target-process.h"
#include "foundry-json-node.h"
#include "foundry-util-private.h"

typedef struct
{
  GIOStream        *stream;
  GSubprocess      *subprocess;
  FoundryDapDriver *driver;
  GListStore       *log_messages;
  GListStore       *modules;
  GListStore       *threads;
} FoundryDapDebuggerPrivate;

enum {
  PROP_0,
  PROP_STREAM,
  PROP_SUBPROCESS,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryDapDebugger, foundry_dap_debugger, FOUNDRY_TYPE_DEBUGGER)

static GParamSpec *properties[N_PROPS];

static void
foundry_dap_debugger_handle_output_event (FoundryDapDebugger *self,
                                          JsonNode           *node)
{
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);
  g_autoptr(FoundryDebuggerLogMessage) message = NULL;

  g_assert (FOUNDRY_IS_DAP_DEBUGGER (self));
  g_assert (node != NULL);

  if ((message = foundry_dap_debugger_log_message_new (node)))
    g_list_store_append (priv->log_messages, message);
}

static void
foundry_dap_debugger_handle_module_event (FoundryDapDebugger *self,
                                          JsonNode           *node)
{
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);
  const char *reason = NULL;
  const char *id = NULL;
  const char *name = NULL;
  const char *path = NULL;

  g_assert (FOUNDRY_IS_DAP_DEBUGGER (self));
  g_assert (node != NULL);

  if (!FOUNDRY_JSON_OBJECT_PARSE (node,
                                  "body", "{",
                                    "reason", FOUNDRY_JSON_NODE_GET_STRING (&reason),
                                    "module", "{",
                                      "id", FOUNDRY_JSON_NODE_GET_STRING (&id),
                                      "name", FOUNDRY_JSON_NODE_GET_STRING (&name),
                                      "path", FOUNDRY_JSON_NODE_GET_STRING (&path),
                                    "}",
                                  "}"))
    return;

  if (!FOUNDRY_JSON_OBJECT_PARSE (node,
                                  "body", "{",
                                    "module", "{",
                                      "path", FOUNDRY_JSON_NODE_GET_STRING (&path),
                                    "}",
                                  "}"))
    path = NULL;

  if (g_strcmp0 (reason, "changed") == 0 ||
      g_strcmp0 (reason, "removed") == 0)
    {
      guint n_items = g_list_model_get_n_items (G_LIST_MODEL (priv->modules));

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(FoundryDebuggerModule) module = g_list_model_get_item (G_LIST_MODEL (priv->modules), i);
          g_autofree char *module_id = foundry_debugger_module_dup_id (module);

          if (g_strcmp0 (id, module_id) == 0)
            {
              g_list_store_remove (priv->modules, i);
              break;
            }
        }
    }

  if (g_strcmp0 (reason, "new") == 0 ||
      g_strcmp0 (reason, "changed") == 0)
    {
      g_autoptr(FoundryDebuggerModule) module = NULL;

      module = foundry_dap_debugger_module_new (id, name, path);
      g_list_store_append (priv->modules, module);
    }
}

static void
mark_thread_stopped (GListModel *threads,
                     gint64      thread_id,
                     gboolean    stopped)
{
  guint n_items = g_list_model_get_n_items (threads);
  g_autofree char *id_str = g_strdup_printf ("%"G_GINT64_FORMAT, thread_id);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryDapDebuggerThread) thread = g_list_model_get_item (threads, i);
      g_autofree char *id = foundry_debugger_thread_dup_id (FOUNDRY_DEBUGGER_THREAD (thread));

      if (thread_id == 0 || foundry_str_equal0 (id, id_str))
        foundry_dap_debugger_thread_set_stopped (thread, stopped);
    }
}

static void
foundry_dap_debugger_handle_stopped_event (FoundryDapDebugger *self,
                                           JsonNode           *node)
{
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);
  g_autoptr(FoundryDebuggerEvent) event = NULL;
  JsonNode *body = NULL;
  const char *reason = NULL;
  gint64 thread_id = 0;
  gboolean all_threads_stopped = FALSE;

  g_assert (FOUNDRY_IS_DAP_DEBUGGER (self));
  g_assert (node != NULL);

  if (!FOUNDRY_JSON_OBJECT_PARSE (node, "body", FOUNDRY_JSON_NODE_GET_NODE (&body)))
    return;

  if (!FOUNDRY_JSON_OBJECT_PARSE (body, "reason", FOUNDRY_JSON_NODE_GET_STRING (&reason)))
    return;

  if (!FOUNDRY_JSON_OBJECT_PARSE (body, "threadId", FOUNDRY_JSON_NODE_GET_INT (&thread_id)))
    thread_id = 0;

  if (FOUNDRY_JSON_OBJECT_PARSE (body, "allThreadsStopped", FOUNDRY_JSON_NODE_GET_BOOLEAN (&all_threads_stopped)) &&
      all_threads_stopped)
    mark_thread_stopped (G_LIST_MODEL (priv->threads), 0, TRUE);
  else
    mark_thread_stopped (G_LIST_MODEL (priv->threads), thread_id, TRUE);

  event = foundry_dap_debugger_stop_event_new (self, node);
  foundry_debugger_emit_event (FOUNDRY_DEBUGGER (self), event);
}

static void
foundry_dap_debugger_handle_continued_event (FoundryDapDebugger *self,
                                             JsonNode           *node)
{
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);
  JsonNode *body = NULL;
  gint64 thread_id = 0;
  gboolean all_threads_continued = FALSE;

  g_assert (FOUNDRY_IS_DAP_DEBUGGER (self));
  g_assert (node != NULL);

  if (!FOUNDRY_JSON_OBJECT_PARSE (node, "body", FOUNDRY_JSON_NODE_GET_NODE (&body)))
    return;

  if (!FOUNDRY_JSON_OBJECT_PARSE (body, "threadId", FOUNDRY_JSON_NODE_GET_INT (&thread_id)))
    thread_id = 0;

  if (FOUNDRY_JSON_OBJECT_PARSE (body, "allThreadsContinued", FOUNDRY_JSON_NODE_GET_BOOLEAN (&all_threads_continued)) &&
      all_threads_continued)
    mark_thread_stopped (G_LIST_MODEL (priv->threads), 0, FALSE);
  else
    mark_thread_stopped (G_LIST_MODEL (priv->threads), thread_id, FALSE);
}

static void
foundry_dap_debugger_handle_thread_event (FoundryDapDebugger *self,
                                          JsonNode           *node)
{
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);
  const char *reason = NULL;
  gint64 thread_id = 0;

  g_assert (FOUNDRY_IS_DAP_DEBUGGER (self));
  g_assert (node != NULL);

  if (!FOUNDRY_JSON_OBJECT_PARSE (node,
                                  "body", "{",
                                    "reason", FOUNDRY_JSON_NODE_GET_STRING (&reason),
                                    "threadId", FOUNDRY_JSON_NODE_GET_INT (&thread_id),
                                  "}"))
    return;

  if (foundry_str_equal0 (reason, "started"))
    {
      g_autoptr(FoundryDebuggerThread) thread = NULL;

      if ((thread = foundry_dap_debugger_thread_new (self, thread_id)))
        g_list_store_append (priv->threads, thread);
    }
  else if (foundry_str_equal0 (reason, "exited"))
    {
      g_autofree char *id_str = g_strdup_printf ("%"G_GINT64_FORMAT, thread_id);
      guint n_items = g_list_model_get_n_items (G_LIST_MODEL (priv->threads));

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(FoundryDebuggerThread) thread = g_list_model_get_item (G_LIST_MODEL (priv->threads), i);
          g_autofree char *id = foundry_debugger_thread_dup_id (thread);

          if (foundry_str_equal0 (id, id_str))
            {
              g_list_store_remove (priv->threads, i);
              break;
            }
        }
    }
}

static void
foundry_dap_debugger_driver_event_cb (FoundryDapDebugger *self,
                                      JsonNode           *node,
                                      FoundryDapDriver   *driver)
{
  const char *event = NULL;

  g_assert (FOUNDRY_IS_DAP_DEBUGGER (self));
  g_assert (node != NULL);
  g_assert (FOUNDRY_IS_DAP_DRIVER (driver));

  if (!FOUNDRY_JSON_OBJECT_PARSE (node,
                                  "type", "event",
                                  "event", FOUNDRY_JSON_NODE_GET_STRING (&event)))
    return;

  if (FALSE) {}
  else if (g_strcmp0 (event, "output") == 0)
    foundry_dap_debugger_handle_output_event (self, node);
  else if (g_strcmp0 (event, "module") == 0)
    foundry_dap_debugger_handle_module_event (self, node);
  else if (g_strcmp0 (event, "stopped") == 0)
    foundry_dap_debugger_handle_stopped_event (self, node);
  else if (g_strcmp0 (event, "thread") == 0)
    foundry_dap_debugger_handle_thread_event (self, node);
  else if (g_strcmp0 (event, "continued") == 0)
    foundry_dap_debugger_handle_continued_event (self, node);
}

static gboolean
foundry_dap_debugger_driver_handle_request_cb (FoundryDapDebugger *self,
                                               JsonNode           *node,
                                               FoundryDapDriver   *driver)
{
  g_assert (FOUNDRY_IS_DAP_DEBUGGER (self));
  g_assert (node != NULL);
  g_assert (FOUNDRY_IS_DAP_DRIVER (driver));

  return FALSE;
}

static DexFuture *
foundry_dap_debugger_exited (DexFuture *future,
                             gpointer   user_data)
{
  GWeakRef *wr = user_data;
  FoundryDapDebuggerPrivate *priv;
  g_autoptr(FoundryDapDebugger) self = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (future));

  if (!(self = g_weak_ref_get (wr)))
    return dex_future_new_true ();

  g_assert (FOUNDRY_IS_DAP_DEBUGGER (self));

  priv = foundry_dap_debugger_get_instance_private (self);

  if (!dex_await (dex_ref (future), &error))
    {
      if (priv->stream != NULL)
        g_io_stream_close (priv->stream, NULL, NULL);
    }

  return dex_ref (future);
}

static GListModel *
foundry_dap_debugger_list_log_messages (FoundryDebugger *debugger)
{
  FoundryDapDebugger *self = FOUNDRY_DAP_DEBUGGER (debugger);
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  return g_object_ref (G_LIST_MODEL (priv->log_messages));
}

static GListModel *
foundry_dap_debugger_list_modules (FoundryDebugger *debugger)
{
  FoundryDapDebugger *self = FOUNDRY_DAP_DEBUGGER (debugger);
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  return g_object_ref (G_LIST_MODEL (priv->modules));
}

static GListModel *
foundry_dap_debugger_list_threads (FoundryDebugger *debugger)
{
  FoundryDapDebugger *self = FOUNDRY_DAP_DEBUGGER (debugger);
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  return g_object_ref (G_LIST_MODEL (priv->threads));
}

DexFuture *
_foundry_dap_debugger_move (FoundryDapDebugger      *self,
                            gint64                   thread_id,
                            FoundryDebuggerMovement  movement)
{
  DexFuture *move = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_DAP_DEBUGGER (self));

  switch (movement)
    {
    case FOUNDRY_DEBUGGER_MOVEMENT_START:
      /* We "start" automatically, fallthrough to continue */
      G_GNUC_FALLTHROUGH;

    case FOUNDRY_DEBUGGER_MOVEMENT_CONTINUE:
      move = foundry_dap_debugger_call (self,
                                        FOUNDRY_JSON_OBJECT_NEW ("type", "request",
                                                                 "command", "continue",
                                                                 "arguments", "{",
                                                                   "threadId", FOUNDRY_JSON_NODE_PUT_INT (thread_id),
                                                                 "}"));
      break;

    case FOUNDRY_DEBUGGER_MOVEMENT_STEP_IN:
      move = foundry_dap_debugger_call (self,
                                        FOUNDRY_JSON_OBJECT_NEW ("type", "request",
                                                                 "command", "stepIn",
                                                                 "arguments", "{",
                                                                   "threadId", FOUNDRY_JSON_NODE_PUT_INT (thread_id),
                                                                 "}"));
      break;

    case FOUNDRY_DEBUGGER_MOVEMENT_STEP_OVER:
      move = foundry_dap_debugger_call (self,
                                        FOUNDRY_JSON_OBJECT_NEW ("type", "request",
                                                                 "command", "next",
                                                                 "arguments", "{",
                                                                   "threadId", FOUNDRY_JSON_NODE_PUT_INT (thread_id),
                                                                 "}"));
      break;

    case FOUNDRY_DEBUGGER_MOVEMENT_STEP_OUT:
      move = foundry_dap_debugger_call (self,
                                        FOUNDRY_JSON_OBJECT_NEW ("type", "request",
                                                                 "command", "stepOut",
                                                                 "arguments", "{",
                                                                   "threadId", FOUNDRY_JSON_NODE_PUT_INT (thread_id),
                                                                 "}"));
      break;

    default:
      g_assert_not_reached ();
    }

  if (move != NULL)
    move = dex_future_then (move,
                            foundry_dap_protocol_unwrap_error,
                            NULL, NULL);

  return g_steal_pointer (&move);
}

static DexFuture *
foundry_dap_debugger_move (FoundryDebugger         *debugger,
                           FoundryDebuggerMovement  movement)
{
  return _foundry_dap_debugger_move (FOUNDRY_DAP_DEBUGGER (debugger), 1, movement);
}

static void
foundry_dap_debugger_constructed (GObject *object)
{
  FoundryDapDebugger *self = (FoundryDapDebugger *)object;
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  G_OBJECT_CLASS (foundry_dap_debugger_parent_class)->constructed (object);

  if (priv->subprocess != NULL)
    dex_future_disown (dex_future_finally (dex_subprocess_wait_check (priv->subprocess),
                                           foundry_dap_debugger_exited,
                                           foundry_weak_ref_new (self),
                                           (GDestroyNotify) foundry_weak_ref_free));

  if (priv->stream == NULL)
    {
      g_warning ("`%s` at %p created without a stream, this cannot work!",
                 G_OBJECT_TYPE_NAME (self), self);
      return;
    }

  priv->driver = foundry_dap_driver_new (priv->stream, FOUNDRY_JSONRPC_STYLE_HTTP);
  g_signal_connect_object (priv->driver,
                           "event",
                           G_CALLBACK (foundry_dap_debugger_driver_event_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->driver,
                           "handle-request",
                           G_CALLBACK (foundry_dap_debugger_driver_handle_request_cb),
                           self,
                           G_CONNECT_SWAPPED);
  foundry_dap_driver_start (priv->driver);
}

static void
foundry_dap_debugger_dispose (GObject *object)
{
  FoundryDapDebugger *self = (FoundryDapDebugger *)object;
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  if (priv->subprocess != NULL)
    g_subprocess_force_exit (priv->subprocess);

  if (priv->stream != NULL)
    g_io_stream_close (priv->stream, NULL, NULL);

  g_clear_object (&priv->driver);
  g_clear_object (&priv->stream);
  g_clear_object (&priv->subprocess);
  g_clear_object (&priv->log_messages);
  g_clear_object (&priv->modules);
  g_clear_object (&priv->threads);

  G_OBJECT_CLASS (foundry_dap_debugger_parent_class)->dispose (object);
}

static void
foundry_dap_debugger_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  FoundryDapDebugger *self = FOUNDRY_DAP_DEBUGGER (object);

  switch (prop_id)
    {
    case PROP_STREAM:
      g_value_take_object (value, foundry_dap_debugger_dup_stream (self));
      break;

    case PROP_SUBPROCESS:
      g_value_take_object (value, foundry_dap_debugger_dup_subprocess (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_dap_debugger_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  FoundryDapDebugger *self = FOUNDRY_DAP_DEBUGGER (object);
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_STREAM:
      priv->stream = g_value_dup_object (value);
      break;

    case PROP_SUBPROCESS:
      priv->subprocess = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_dap_debugger_class_init (FoundryDapDebuggerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryDebuggerClass *debugger_class = FOUNDRY_DEBUGGER_CLASS (klass);

  object_class->constructed = foundry_dap_debugger_constructed;
  object_class->dispose = foundry_dap_debugger_dispose;
  object_class->get_property = foundry_dap_debugger_get_property;
  object_class->set_property = foundry_dap_debugger_set_property;

  debugger_class->list_log_messages = foundry_dap_debugger_list_log_messages;
  debugger_class->list_modules = foundry_dap_debugger_list_modules;
  debugger_class->list_threads = foundry_dap_debugger_list_threads;
  debugger_class->move = foundry_dap_debugger_move;

  properties[PROP_STREAM] =
    g_param_spec_object ("stream", NULL, NULL,
                         G_TYPE_IO_STREAM,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SUBPROCESS] =
    g_param_spec_object ("subprocess", NULL, NULL,
                         G_TYPE_SUBPROCESS,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_dap_debugger_init (FoundryDapDebugger *self)
{
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  priv->log_messages = g_list_store_new (FOUNDRY_TYPE_DAP_DEBUGGER_LOG_MESSAGE);
  priv->modules = g_list_store_new (FOUNDRY_TYPE_DAP_DEBUGGER_MODULE);
  priv->threads = g_list_store_new (FOUNDRY_TYPE_DAP_DEBUGGER_THREAD);
}

/**
 * foundry_dap_debugger_dup_subprocess:
 * @self: a [class@Foundry.DapDebugger]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
GSubprocess *
foundry_dap_debugger_dup_subprocess (FoundryDapDebugger *self)
{
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_DAP_DEBUGGER (self), NULL);

  if (priv->subprocess)
    return g_object_ref (priv->subprocess);

  return NULL;
}

/**
 * foundry_dap_debugger_dup_stream:
 * @self: a [class@Foundry.DapDebugger]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
GIOStream *
foundry_dap_debugger_dup_stream (FoundryDapDebugger *self)
{
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_DAP_DEBUGGER (self), NULL);

  if (priv->stream)
    return g_object_ref (priv->stream);

  return NULL;
}

/**
 * foundry_dap_debugger_call:
 * @self: a [class@Foundry.DapDebugger]
 * @node: (transfer full):
 *
 * Makes a request to the DAP server. The reply will be provided
 * via the resulting future, even if the reply contains an error.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [struct@Json.Node] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_dap_debugger_call (FoundryDapDebugger *self,
                           JsonNode           *node)
{
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  dex_return_error_if_fail (FOUNDRY_IS_DAP_DEBUGGER (self));
  dex_return_error_if_fail (node != NULL);
  dex_return_error_if_fail (JSON_NODE_HOLDS_OBJECT (node));

  return foundry_dap_driver_call (priv->driver, node);
}

/**
 * foundry_dap_debugger_send:
 * @self: a [class@Foundry.DapDebugger]
 * @node: (transfer full):
 *
 * Send a message to the peer without expecting a reply.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   any value or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_dap_debugger_send (FoundryDapDebugger *self,
                           JsonNode           *node)
{
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  dex_return_error_if_fail (FOUNDRY_IS_DAP_DEBUGGER (self));
  dex_return_error_if_fail (node != NULL);
  dex_return_error_if_fail (JSON_NODE_HOLDS_OBJECT (node));

  return foundry_dap_driver_send (priv->driver, node);
}
