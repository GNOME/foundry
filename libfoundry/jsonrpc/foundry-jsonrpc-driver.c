/* foundry-jsonrpc-driver.c
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

#include <json-glib/json-glib.h>

#include "foundry-json-input-stream.h"
#include "foundry-json-output-stream.h"
#include "foundry-jsonrpc-driver-private.h"
#include "foundry-jsonrpc-waiter-private.h"

/* TODO:
 *
 * We probably need to move to "id" using strings instead of
 * numbers. This appears to be a thing in some places and we can
 * largely paper over it by treating ids as opaque.
 */

struct _FoundryJsonrpcDriver
{
  GObject                  parent_instance;
  GIOStream               *stream;
  FoundryJsonInputStream  *input;
  FoundryJsonOutputStream *output;
  DexChannel              *output_channel;
  GHashTable              *requests;
  GBytes                  *delimiter;
  gint64                   last_seq;
};

enum {
  PROP_0,
  PROP_STREAM,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryJsonrpcDriver, foundry_jsonrpc_driver, G_TYPE_OBJECT)

enum {
  SIGNAL_HANDLE_METHOD_CALL,
  SIGNAL_HANDLE_NOTIFICATION,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static gboolean
check_string (JsonNode   *node,
              const char *value)
{
  if (node == NULL)
    return FALSE;

  if (!JSON_NODE_HOLDS_VALUE (node))
    return FALSE;

  return g_strcmp0 (value, json_node_get_string (node)) == 0;
}

static gboolean
is_jsonrpc (JsonNode *node)
{
  JsonObject *obj;

  return JSON_NODE_HOLDS_OBJECT (node) &&
         (obj = json_node_get_object (node)) &&
         json_object_has_member (obj, "jsonrpc") &&
         check_string (json_object_get_member (obj, "jsonrpc"), "2.0");
}

static gboolean
is_jsonrpc_notification (JsonNode *node)
{
  JsonObject *obj;

  return JSON_NODE_HOLDS_OBJECT (node) &&
         (obj = json_node_get_object (node)) &&
         !json_object_has_member (obj, "id") &&
         json_object_has_member (obj, "method");
}

static gboolean
is_jsonrpc_method_call (JsonNode *node)
{
  JsonObject *obj;

  return JSON_NODE_HOLDS_OBJECT (node) &&
         (obj = json_node_get_object (node)) &&
         json_object_has_member (obj, "id") &&
         json_object_has_member (obj, "method") &&
         json_object_has_member (obj, "params");
}

static gboolean
is_jsonrpc_method_reply (JsonNode *node)
{
  JsonObject *obj;

  return JSON_NODE_HOLDS_OBJECT (node) &&
         (obj = json_node_get_object (node)) &&
         json_object_has_member (obj, "id") &&
         json_object_has_member (obj, "result");
}

static void
foundry_jsonrpc_driver_handle_message (FoundryJsonrpcDriver *self,
                                       JsonNode             *node)
{
  g_assert (FOUNDRY_IS_JSONRPC_DRIVER (self));
  g_assert (node != NULL);

  if (JSON_NODE_HOLDS_ARRAY (node))
    {
      JsonArray *ar = json_node_get_array (node);
      gsize len = json_array_get_length (ar);

      for (gsize i = 0; i < len; i++)
        {
          JsonNode *child = json_array_get_element (ar, i);

          foundry_jsonrpc_driver_handle_message (self, child);
        }

      return;
    }

  if (is_jsonrpc (node))
    {
      if (is_jsonrpc_notification (node))
        {
          JsonObject *obj = json_node_get_object (node);
          const char *method = json_object_get_string_member (obj, "method");
          JsonNode *params = json_object_get_member (obj, "params");

          g_signal_emit (self, signals[SIGNAL_HANDLE_NOTIFICATION], 0, method, params);

          return;
        }

      if (is_jsonrpc_method_reply (node))
        {
          JsonObject *obj = json_node_get_object (node);
          gint64 seq = json_object_get_int_member (obj, "id");
          JsonNode *result = json_object_get_member (obj, "result");
          JsonNode *error = json_object_get_member (obj, "error");
          g_autofree gint64 *stolen_key = NULL;
          g_autoptr(FoundryJsonrpcWaiter) waiter = NULL;

          if (g_hash_table_steal_extended (self->requests,
                                           &seq,
                                           (gpointer *)&stolen_key,
                                           (gpointer *)&waiter))
            {
              if (error != NULL && JSON_NODE_HOLDS_OBJECT (error))
                {
                  JsonObject *err = json_node_get_object (error);
                  const char *message = json_object_get_string_member (err, "message");
                  gint64 code = json_object_get_int_member (err, "code");

                  foundry_jsonrpc_waiter_reject (waiter,
                                                 g_error_new_literal (g_quark_from_static_string ("foundry-jsonrpc-error"),
                                                                      code,
                                                                      message ? message : "unknown error"));
                }
              else
                {
                  foundry_jsonrpc_waiter_reply (waiter, result);
                }
            }

          return;
        }

      if (is_jsonrpc_method_call (node))
        {
          JsonObject *obj = json_node_get_object (node);
          const char *method = json_object_get_string_member (obj, "method");
          JsonNode *params = json_object_get_member (obj, "params");
          gint64 id = json_object_get_int_member (obj, "id");
          gboolean ret = FALSE;

          g_signal_emit (self, signals[SIGNAL_HANDLE_METHOD_CALL], 0, method, params, id, &ret);

          if (ret == FALSE)
            foundry_jsonrpc_driver_reply_with_error (self, id, -32601, "Method not found");

          return;
        }
    }

  /* Protocol error */
  g_io_stream_close_async (self->stream, 0, NULL, NULL, NULL);
}

static void
foundry_jsonrpc_driver_dispose (GObject *object)
{
  FoundryJsonrpcDriver *self = (FoundryJsonrpcDriver *)object;

  if (self->output_channel)
    {
      dex_channel_close_send (self->output_channel);
      dex_channel_close_receive (self->output_channel);
      dex_clear (&self->output_channel);
    }

  g_clear_object (&self->input);
  g_clear_object (&self->output);
  g_clear_object (&self->stream);

  g_clear_pointer (&self->requests, g_hash_table_unref);
  g_clear_pointer (&self->delimiter, g_bytes_unref);

  G_OBJECT_CLASS (foundry_jsonrpc_driver_parent_class)->dispose (object);
}

static void
foundry_jsonrpc_driver_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  FoundryJsonrpcDriver *self = FOUNDRY_JSONRPC_DRIVER (object);

  switch (prop_id)
    {
    case PROP_STREAM:
      g_value_set_object (value, self->stream);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_jsonrpc_driver_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  FoundryJsonrpcDriver *self = FOUNDRY_JSONRPC_DRIVER (object);

  switch (prop_id)
    {
    case PROP_STREAM:
      self->stream = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_jsonrpc_driver_class_init (FoundryJsonrpcDriverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_jsonrpc_driver_dispose;
  object_class->get_property = foundry_jsonrpc_driver_get_property;
  object_class->set_property = foundry_jsonrpc_driver_set_property;

  properties[PROP_STREAM] =
    g_param_spec_object ("stream", NULL, NULL,
                         G_TYPE_IO_STREAM,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[SIGNAL_HANDLE_METHOD_CALL] =
    g_signal_new ("handle-method-call",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_first_wins, NULL,
                  NULL,
                  G_TYPE_BOOLEAN, 3, G_TYPE_STRING, JSON_TYPE_NODE, G_TYPE_INT64);

  signals[SIGNAL_HANDLE_NOTIFICATION] =
    g_signal_new ("handle-notification",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 2, G_TYPE_STRING, JSON_TYPE_NODE);
}

static void
foundry_jsonrpc_driver_init (FoundryJsonrpcDriver *self)
{
  self->output_channel = dex_channel_new (0);
  self->requests = g_hash_table_new_full (g_int64_hash, g_int64_equal, g_free, g_object_unref);
  self->delimiter = g_bytes_new ("\n", 1);
}

FoundryJsonrpcDriver *
foundry_jsonrpc_driver_new (GIOStream *stream)
{
  g_return_val_if_fail (G_IS_IO_STREAM (stream), NULL);

  return g_object_new (FOUNDRY_TYPE_JSONRPC_DRIVER,
                       "stream", stream,
                       NULL);
}

/**
 * foundry_jsonrpc_driver_call:
 * @self: a [class@Foundry.JsonrpcDriver]
 *
 * Returns: (transfer full): a future that resolves to a [struct@Json.Node]
 *   containing the reply.
 */
DexFuture *
foundry_jsonrpc_driver_call (FoundryJsonrpcDriver *self,
                             const char           *method,
                             JsonNode             *params)
{
  g_autoptr(FoundryJsonrpcWaiter) waiter = NULL;
  g_autoptr(JsonObject) object = NULL;
  g_autoptr(JsonNode) node = NULL;
  gint64 seq;

  dex_return_error_if_fail (FOUNDRY_IS_JSONRPC_DRIVER (self));
  dex_return_error_if_fail (method != NULL);

  seq = ++self->last_seq;

  object = json_object_new ();

  json_object_set_string_member (object, "jsonrpc", "2.0");
  json_object_set_int_member (object, "id", seq);
  json_object_set_string_member (object, "method", method);
  json_object_set_member (object, "params", params);

  node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (node, object);

  waiter = foundry_jsonrpc_waiter_new (node, seq);

  g_hash_table_replace (self->requests,
                        g_memdup2 (&seq, sizeof seq),
                        g_object_ref (waiter));

  dex_future_disown (dex_future_catch (dex_channel_send (self->output_channel,
                                                         dex_future_new_take_object (g_object_ref (waiter))),
                                       foundry_jsonrpc_waiter_catch,
                                       g_object_ref (waiter),
                                       g_object_unref));

  return foundry_jsonrpc_waiter_await (waiter);
}

/**
 * foundry_jsonrpc_driver_notify:
 * @self: a [class@Foundry.JsonrpcDriver]
 *
 * Returns: (transfer full): a future that resolves to any value once
 *   the message has been queued for delivery
 */
DexFuture *
foundry_jsonrpc_driver_notify (FoundryJsonrpcDriver *self,
                               const char           *method,
                               JsonNode             *params)
{
  g_autoptr(FoundryJsonrpcWaiter) waiter = NULL;
  g_autoptr(JsonObject) object = NULL;
  g_autoptr(JsonNode) node = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_JSONRPC_DRIVER (self));
  dex_return_error_if_fail (method != NULL);

  object = json_object_new ();

  json_object_set_string_member (object, "jsonrpc", "2.0");
  json_object_set_string_member (object, "method", method);
  json_object_set_member (object, "params", params);

  node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (node, object);

  waiter = foundry_jsonrpc_waiter_new (node, 0);

  return dex_future_catch (dex_channel_send (self->output_channel,
                                             dex_future_new_take_object (g_object_ref (waiter))),
                           foundry_jsonrpc_waiter_catch,
                           g_object_ref (waiter),
                           g_object_unref);
}

/**
 * foundry_jsonrpc_driver_reply_with_error:
 * @self: a [class@Foundry.JsonrpcDriver]
 *
 * Returns: (transfer full): a future that resolves to any value once
 *   the message has been queued for delivery
 */
DexFuture *
foundry_jsonrpc_driver_reply_with_error (FoundryJsonrpcDriver *self,
                                         gint64                seq,
                                         int                   code,
                                         const char           *message)
{
  g_autoptr(FoundryJsonrpcWaiter) waiter = NULL;
  g_autoptr(JsonObject) object = NULL;
  g_autoptr(JsonObject) error = NULL;
  g_autoptr(JsonNode) node = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_JSONRPC_DRIVER (self));

  object = json_object_new ();
  error = json_object_new ();

  json_object_set_int_member (error, "code", code);
  json_object_set_string_member (error, "message", message);

  json_object_set_string_member (object, "jsonrpc", "2.0");
  json_object_set_int_member (object, "id", seq);
  json_object_set_object_member (object, "error", error);

  node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (node, object);

  waiter = foundry_jsonrpc_waiter_new (node, 0);

  return dex_future_catch (dex_channel_send (self->output_channel,
                                             dex_future_new_take_object (g_object_ref (waiter))),
                           foundry_jsonrpc_waiter_catch,
                           g_object_ref (waiter),
                           g_object_unref);
}

typedef struct _Worker
{
  GWeakRef                 self_wr;
  DexChannel              *output_channel;
  FoundryJsonOutputStream *output;
  FoundryJsonInputStream  *input;
} Worker;

static void
worker_free (Worker *state)
{
  g_weak_ref_clear (&state->self_wr);
  dex_clear (&state->output_channel);
  g_clear_object (&state->output);
  g_clear_object (&state->input);
  g_free (state);
}

static DexFuture *
foundry_jsonrpc_driver_worker (gpointer data)
{
  Worker *state = data;
  g_autoptr(DexFuture) next_read = NULL;
  g_autoptr(DexFuture) next_write = NULL;

  g_assert (state != NULL);
  g_assert (state->output_channel != NULL);
  g_assert (state->output != NULL);
  g_assert (state->input != NULL);

  for (;;)
    {
      g_autoptr(FoundryJsonrpcDriver) self = NULL;
      g_autoptr(GError) error = NULL;

      if (next_read == NULL)
        next_read = foundry_json_input_stream_read (state->input,
                                                    g_bytes_get_data (self->delimiter, NULL),
                                                    g_bytes_get_size (self->delimiter));

      if (next_write == NULL)
        next_write = dex_channel_receive (state->output_channel);

      /* Wait until there is something to read or write */
      if (dex_await (dex_future_any (dex_ref (next_read),
                                     dex_ref (next_write),
                                     NULL),
                     NULL))
        {
          /* If we read a message, get the bytes and decode it for
           * delivering to the application.
           */
          if (dex_future_is_resolved (next_read))
            {
              g_autoptr(JsonNode) node = NULL;

              if (!(node = dex_await_boxed (g_steal_pointer (&next_read), &error)))
                return dex_future_new_for_error (g_steal_pointer (&error));

              if ((self = g_weak_ref_get (&state->self_wr)))
                {
                  foundry_jsonrpc_driver_handle_message (self, node);
                  g_clear_object (&self);
                }

              dex_clear (&next_read);
            }

          /* If we got a message to write, then submit it now. This
           * awaits for the message to be buffered because otherwise
           * we could end up in a situation where we try to submit
           * two outgoing messages at the same time.
           */
          if (dex_future_is_resolved (next_write))
            {
              g_autoptr(FoundryJsonrpcWaiter) waiter = dex_await_object (g_steal_pointer (&next_write), NULL);

              g_assert (!waiter || FOUNDRY_IS_JSONRPC_WAITER (waiter));

              if (waiter != NULL)
                {
                  JsonNode *node = foundry_jsonrpc_waiter_get_node (waiter);

                  if (!dex_await (foundry_json_output_stream_write (state->output, node, self->delimiter), &error))
                    return dex_future_new_for_error (g_steal_pointer (&error));
                }

              dex_clear (&next_write);
            }
        }

      /* Before we try to run again, make sure that our client
       * has not been disposed. If so, then we can just bail.
       */
      if (!(self = g_weak_ref_get (&state->self_wr)))
        break;
    }

  return dex_future_new_true ();
}

void
foundry_jsonrpc_driver_start (FoundryJsonrpcDriver *self)
{
  Worker *state;

  g_return_if_fail (FOUNDRY_IS_JSONRPC_DRIVER (self));
  g_return_if_fail (G_IS_IO_STREAM (self->stream));

  state = g_new0 (Worker, 1);
  g_weak_ref_init (&state->self_wr, self);
  state->input = g_object_ref (self->input);
  state->output = g_object_ref (self->output);
  state->output_channel = dex_ref (self->output_channel);

  dex_future_disown (dex_scheduler_spawn (NULL, 0,
                                          foundry_jsonrpc_driver_worker,
                                          state,
                                          (GDestroyNotify) worker_free));

}
