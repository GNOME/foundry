/* foundry-mcp-output-stream.c
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

#include "foundry-mcp-output-stream-private.h"

struct _FoundryMcpOutputStream
{
  GDataOutputStream parent_instance;
};

G_DEFINE_FINAL_TYPE (FoundryMcpOutputStream, foundry_mcp_output_stream, G_TYPE_DATA_OUTPUT_STREAM)

static void
foundry_mcp_output_stream_class_init (FoundryMcpOutputStreamClass *klass)
{
}

static void
foundry_mcp_output_stream_init (FoundryMcpOutputStream *self)
{
}

FoundryMcpOutputStream *
foundry_mcp_output_stream_new (GOutputStream *base_stream,
                               gboolean       close_base_stream)
{
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (base_stream), NULL);

  return g_object_new (FOUNDRY_TYPE_MCP_OUTPUT_STREAM,
                       "base-stream", base_stream,
                       "close-base-stream", close_base_stream,
                       NULL);
}

typedef struct _Write
{
  char       *headers;
  GBytes     *bytes;
  DexPromise *promise;
} Write;

static void
write_free (Write *state)
{
  g_clear_pointer (&state->headers, g_free);
  g_clear_pointer (&state->bytes, g_bytes_unref);
  dex_clear (&state->promise);
  g_free (state);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (Write, write_free)

static void
foundry_mcp_output_stream_send_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  g_autoptr(Write) state = user_data;
  g_autoptr(GError) error = NULL;
  gsize n_written = 0;

  g_assert (FOUNDRY_IS_MCP_OUTPUT_STREAM (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);

  if (!g_output_stream_writev_all_finish (G_OUTPUT_STREAM (object), result, &n_written, &error))
    dex_promise_reject (state->promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_boolean (state->promise, TRUE);
}

static DexFuture *
foundry_mcp_output_stream_send (DexFuture *completed,
                                gpointer   user_data)
{
  FoundryMcpOutputStream *self = user_data;
  g_autoptr(GBytes) bytes = NULL;
  DexPromise *promise = NULL;
  GOutputVector vectors[2];
  Write *state;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (FOUNDRY_IS_MCP_OUTPUT_STREAM (self));

  promise = dex_promise_new_cancellable ();
  bytes = dex_await_boxed (dex_ref (completed), NULL);

  state = g_new0 (Write, 1);
  state->bytes = g_bytes_ref (bytes);
  state->promise = dex_ref (promise);

  vectors[0].buffer = g_bytes_get_data (bytes, NULL);
  vectors[0].size = g_bytes_get_size (bytes);
  vectors[1].buffer = "\n";
  vectors[1].size = 1;

  g_output_stream_writev_all_async (G_OUTPUT_STREAM (self),
                                    vectors,
                                    G_N_ELEMENTS (vectors),
                                    G_PRIORITY_DEFAULT,
                                    dex_promise_get_cancellable (promise),
                                    foundry_mcp_output_stream_send_cb,
                                    g_steal_pointer (&state));

  return DEX_FUTURE (promise);
}

typedef struct
{
  GVariant   *message;
  DexPromise *promise;
} Serialize;

static void
foundry_mcp_output_stream_serialize (gpointer data)
{
  g_autoptr(JsonGenerator) generator = NULL;
  Serialize *state = data;
  g_autofree char *contents = NULL;
  JsonNode *node;
  gsize len = 0;

  g_assert (state != NULL);
  g_assert (state->message != NULL);
  g_assert (DEX_IS_PROMISE (state->promise));

  node = json_gvariant_serialize (state->message);
  generator = json_generator_new ();
  json_generator_set_root (generator, node);
  contents = json_generator_to_data (generator, &len);

  dex_promise_resolve_boxed (state->promise,
                             G_TYPE_BYTES,
                             g_bytes_new_take (g_steal_pointer (&contents), len));

  g_clear_pointer (&state->message, g_variant_unref);
  dex_clear (&state->promise);
  g_free (state);
}

/**
 * foundry_mcp_output_stream_write:
 * @self: a [class@Foundry.DapOutputStream]
 *
 * Returns: (transfer full):
 */
DexFuture *
foundry_mcp_output_stream_write (FoundryMcpOutputStream *self,
                                 GVariant               *message)
{
  g_autoptr(DexPromise) promise = NULL;
  Serialize *state;

  dex_return_error_if_fail (FOUNDRY_IS_MCP_OUTPUT_STREAM (self));
  dex_return_error_if_fail (message != NULL);

  promise = dex_promise_new ();

  state = g_new0 (Serialize, 1);
  state->promise = dex_ref (promise);
  state->message = g_variant_ref (message);

  dex_scheduler_push (dex_thread_pool_scheduler_get_default (),
                      foundry_mcp_output_stream_serialize,
                      state);

  return dex_future_then (dex_ref (DEX_FUTURE (promise)),
                          foundry_mcp_output_stream_send,
                          g_object_ref (self),
                          g_object_unref);
}
