/* foundry-dap-client.c
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

#include "foundry-dap-client-private.h"
#include "foundry-dap-input-stream-private.h"
#include "foundry-dap-output-stream-private.h"
#include "foundry-dap-protocol-message-private.h"
#include "foundry-dap-response.h"
#include "foundry-dap-event.h"
#include "foundry-dap-waiter-private.h"
#include "foundry-json.h"
#include "foundry-util-private.h"

struct _FoundryDapClient
{
  GObject                 parent_instance;
  GHashTable             *requests;
  GIOStream              *stream;
  FoundryDapInputStream  *input;
  FoundryDapOutputStream *output;
  DexChannel             *output_channel;
  gint64                  last_seq;
};

G_DEFINE_FINAL_TYPE (FoundryDapClient, foundry_dap_client, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_STREAM,
  N_PROPS
};

enum {
  SIGNAL_EVENT,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static void
foundry_dap_client_emit_event (FoundryDapClient *self,
                               FoundryDapEvent  *event)
{
  g_assert (FOUNDRY_IS_DAP_CLIENT (self));
  g_assert (FOUNDRY_IS_DAP_EVENT (event));

  g_signal_emit (self, signals[SIGNAL_EVENT], 0, event);
}

static void
foundry_dap_client_constructed (GObject *object)
{
  FoundryDapClient *self = (FoundryDapClient *)object;

  G_OBJECT_CLASS (foundry_dap_client_parent_class)->constructed (object);

  g_return_if_fail (G_IS_IO_STREAM (self->stream));

  self->input = foundry_dap_input_stream_new (g_io_stream_get_input_stream (self->stream), TRUE);
  self->output = foundry_dap_output_stream_new (g_io_stream_get_output_stream (self->stream), TRUE);
}

static void
foundry_dap_client_finalize (GObject *object)
{
  FoundryDapClient *self = (FoundryDapClient *)object;

  g_clear_pointer (&self->requests, g_hash_table_unref);

  dex_clear (&self->output_channel);

  g_clear_object (&self->input);
  g_clear_object (&self->output);
  g_clear_object (&self->stream);

  G_OBJECT_CLASS (foundry_dap_client_parent_class)->finalize (object);
}

static void
foundry_dap_client_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryDapClient *self = FOUNDRY_DAP_CLIENT (object);

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
foundry_dap_client_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  FoundryDapClient *self = FOUNDRY_DAP_CLIENT (object);

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
foundry_dap_client_class_init (FoundryDapClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = foundry_dap_client_constructed;
  object_class->finalize = foundry_dap_client_finalize;
  object_class->get_property = foundry_dap_client_get_property;
  object_class->set_property = foundry_dap_client_set_property;

  properties[PROP_STREAM] =
    g_param_spec_object ("stream", NULL, NULL,
                         G_TYPE_IO_STREAM,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[SIGNAL_EVENT] =
    g_signal_new ("event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, FOUNDRY_TYPE_DAP_EVENT);
}

static void
foundry_dap_client_init (FoundryDapClient *self)
{
  self->output_channel = dex_channel_new (0);
  self->requests = g_hash_table_new_full (g_int64_hash, g_int64_equal, g_free, g_object_unref);
}

FoundryDapClient *
foundry_dap_client_new (GIOStream *stream)
{
  g_return_val_if_fail (G_IS_IO_STREAM (stream), NULL);

  return g_object_new (FOUNDRY_TYPE_DAP_CLIENT,
                       "stream", stream,
                       NULL);
}

static void
foundry_dap_client_handle_error (FoundryDapClient *self,
                                 gint64            request_seq,
                                 GError           *error)
{
  g_autoptr(FoundryDapWaiter) waiter = NULL;
  g_autofree gint64 *stolen_key = NULL;

  g_assert (FOUNDRY_IS_DAP_CLIENT (self));

  if (g_hash_table_steal_extended (self->requests,
                                   &request_seq,
                                   (gpointer *)&stolen_key,
                                   (gpointer *)&waiter))
    foundry_dap_waiter_reject (waiter, g_steal_pointer (&error));
  else
    g_error_free (error);
}

static const char *
get_message_type (JsonNode *node)
{
  const char *type;
  JsonObject *obj;

  if (JSON_NODE_HOLDS_OBJECT (node) &&
      (obj = json_node_get_object (node)) &&
      (type = json_object_get_string_member_with_default (obj, "type", NULL)))
    return type;

  return NULL;
}

static DexFuture *
foundry_dap_client_handle_message_dispatch (DexFuture *completed,
                                            gpointer   user_data)
{
  FoundryDapClient *self = user_data;
  g_autoptr(JsonNode) node = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (FOUNDRY_IS_DAP_CLIENT (self));

  if ((node = dex_await_boxed (dex_ref (completed), NULL)))
    {
      g_autoptr(GError) error = NULL;
      const char *message_type;

      if (!(message_type = get_message_type (node)) ||
          !g_strv_contains ((const char * const[]) { "event", "response", "request", NULL }, message_type))
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_INVALID_DATA,
                                      "Invalid message type received");


      if (strcmp (message_type, "response") == 0)
        {
          JsonObject *obj;
          gint64 request_seq;

          if (JSON_NODE_HOLDS_OBJECT (node) &&
              (obj = json_node_get_object (node)) &&
              json_object_has_member (obj, "request_seq") &&
              (request_seq = json_object_get_int_member_with_default (obj, "request_seq", 0)) > 0)
            {
              g_autoptr(FoundryDapWaiter) waiter = NULL;
              g_autofree char *stolen_key = NULL;

              if (g_hash_table_steal_extended (self->requests,
                                               &request_seq,
                                               (gpointer *)&stolen_key,
                                               (gpointer *)&waiter))
                {
                  foundry_dap_waiter_reply (waiter, node);
                  return dex_future_new_true ();
                }
            }

          return dex_future_new_true ();
        }
      else if (strcmp (message_type, "request") == 0)
        {
          /* TODO: Handle incoming requests */
        }
      else if (strcmp (message_type, "event") == 0)
        {
          g_autoptr(FoundryDapProtocolMessage) message = NULL;

          if ((message = _foundry_dap_protocol_message_new_parsed (G_TYPE_INVALID, node, &error)))
            foundry_dap_client_emit_event (self, FOUNDRY_DAP_EVENT (message));
        }
      else g_assert_not_reached ();
    }

  return dex_future_new_true ();
}

static void
foundry_dap_client_handle_message (FoundryDapClient *self,
                                   GBytes           *bytes)
{
  DexFuture *future;

  g_assert (FOUNDRY_IS_DAP_CLIENT (self));
  g_assert (bytes != NULL);

  future = dex_future_then (foundry_json_node_from_bytes (bytes),
                            foundry_dap_client_handle_message_dispatch,
                            g_object_ref (self),
                            g_object_unref);

  dex_future_disown (future);
}

typedef struct _Worker
{
  GWeakRef                self_wr;
  DexChannel             *output_channel;
  FoundryDapOutputStream *output;
  FoundryDapInputStream  *input;
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
foundry_dap_client_worker (gpointer data)
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
      g_autoptr(FoundryDapClient) self = NULL;
      g_autoptr(GError) error = NULL;

      if (next_read == NULL)
        next_read = foundry_dap_input_stream_read (state->input);

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
              g_autoptr(GBytes) bytes = NULL;

              if (!(bytes = dex_await_boxed (g_steal_pointer (&next_read), &error)))
                return dex_future_new_for_error (g_steal_pointer (&error));

              if ((self = g_weak_ref_get (&state->self_wr)))
                {
                  foundry_dap_client_handle_message (self, bytes);
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
              g_autoptr(FoundryDapProtocolMessage) message = dex_await_object (g_steal_pointer (&next_write), NULL);

              if (message != NULL)
                {
                  g_autoptr(GBytes) bytes = _foundry_dap_protocol_message_to_bytes (message, &error);

                  /* If we failed to encode the message, and there is
                   * a future awaiting a reply, then we need to tell it
                   * there was a protocol error in what they sent and
                   * we will not be delivering it.
                   */
                  if (bytes == NULL)
                    {
                      gint64 seq = _foundry_dap_protocol_message_get_seq (message);

                      if ((self = g_weak_ref_get (&state->self_wr)))
                        {
                          foundry_dap_client_handle_error (self, seq, g_steal_pointer (&error));
                          g_clear_object (&self);
                        }
                    }
                  else if (!dex_await (foundry_dap_output_stream_write (state->output, bytes), &error))
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
foundry_dap_client_start (FoundryDapClient *self)
{
  Worker *state;

  g_return_if_fail (FOUNDRY_IS_DAP_CLIENT (self));
  g_return_if_fail (G_IS_IO_STREAM (self->stream));

  state = g_new0 (Worker, 1);
  g_weak_ref_init (&state->self_wr, self);
  state->input = g_object_ref (self->input);
  state->output = g_object_ref (self->output);
  state->output_channel = dex_ref (self->output_channel);

  dex_future_disown (dex_scheduler_spawn (NULL, 0,
                                          foundry_dap_client_worker,
                                          state,
                                          (GDestroyNotify) worker_free));

}

/**
 * foundry_dap_client_call:
 *
 * Sends the request to the peer and awaits a [class@Foundry.DapResponse].
 *
 * Returns: (transfer full): a future that resolves once the message
 *   has been sent to the peer and a reply has been received. Otherwise
 *   rejects with error.
 */
DexFuture *
foundry_dap_client_call (FoundryDapClient  *self,
                         FoundryDapRequest *request)
{
  g_autoptr(FoundryDapWaiter) waiter = NULL;
  gint64 seq;

  dex_return_error_if_fail (FOUNDRY_IS_DAP_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_DAP_REQUEST (request));

  FOUNDRY_DAP_PROTOCOL_MESSAGE (request)->seq = seq = ++self->last_seq;

  waiter = foundry_dap_waiter_new (request);

  g_hash_table_replace (self->requests,
                        g_memdup2 (&seq, sizeof seq),
                        g_object_ref (waiter));

  dex_future_disown (dex_future_catch (dex_channel_send (self->output_channel,
                                                         dex_future_new_take_object (g_object_ref (request))),
                                       foundry_dap_waiter_catch,
                                       g_object_ref (waiter),
                                       g_object_unref));

  return foundry_dap_waiter_await (waiter);
}

/**
 * foundry_dap_client_send:
 *
 * Sends a message to the peer without any handling of replies.
 *
 * Returns: (transfer full): a future that resolves once the message
 *   has been sent to the peer. This does not guarantee delivery.
 */
DexFuture *
foundry_dap_client_send (FoundryDapClient          *self,
                         FoundryDapProtocolMessage *message)
{
  dex_return_error_if_fail (FOUNDRY_IS_DAP_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_DAP_PROTOCOL_MESSAGE (message));

  FOUNDRY_DAP_PROTOCOL_MESSAGE (message)->seq = ++self->last_seq;

  return dex_channel_send (self->output_channel,
                           dex_future_new_take_object (g_object_ref (message)));
}


