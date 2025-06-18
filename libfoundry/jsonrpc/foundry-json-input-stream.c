/* foundry-json-input-stream.c
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

#include "foundry-json.h"
#include "foundry-json-input-stream.h"

struct _FoundryJsonInputStream
{
  GDataInputStream parent_instance;
};

G_DEFINE_FINAL_TYPE (FoundryJsonInputStream, foundry_json_input_stream, G_TYPE_DATA_INPUT_STREAM)

static void
foundry_json_input_stream_class_init (FoundryJsonInputStreamClass *klass)
{
}

static void
foundry_json_input_stream_init (FoundryJsonInputStream *self)
{
}

static DexFuture *
foundry_json_input_stream_deserialize_cb (DexFuture *completed,
                                          gpointer   user_data)
{
  DexPromise *promise = user_data;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (DEX_IS_PROMISE (promise));

  if (!(node = dex_await_boxed (dex_ref (completed), &error)))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_boxed (promise, JSON_TYPE_NODE, g_steal_pointer (&node));

  return dex_future_new_true ();
}

static void
foundry_json_input_stream_read_upto_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *contents = NULL;
  gsize len;

  g_assert (FOUNDRY_IS_JSON_INPUT_STREAM (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!(contents = g_data_input_stream_read_upto_finish (G_DATA_INPUT_STREAM (object), result, &len, &error)))
    {
      dex_promise_reject (promise, g_steal_pointer (&error));
    }
  else
    {
      g_autoptr(GBytes) bytes = NULL;

      bytes = g_bytes_new_take (g_steal_pointer (&contents), len);
      dex_future_disown (dex_future_finally (foundry_json_node_from_bytes (bytes),
                                             foundry_json_input_stream_deserialize_cb,
                                             dex_ref (promise),
                                             dex_unref));
    }
}

/**
 * foundry_json_input_stream_read_upto:
 * @self: a [class@Foundry.JsonInputStream]
 * @stop_chars: the characters that delimit json messages, such as \n.
 * @stop_chars_len: then length of @stop_chars or -1 if it is \0 terminated
 *
 * Reads the next JSON message from the stream.
 *
 * If stop_chars_len is > 0, then you man use `\0` as a delimiter.
 *
 * Use this form when you do not have HTTP headers containing the content
 * length of the JSON message.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [struct@Json.Node] or rejects with error.
 */
DexFuture *
foundry_json_input_stream_read_upto (FoundryJsonInputStream *self,
                                     const char             *stop_chars,
                                     gssize                  stop_chars_len)
{
  DexPromise *promise;

  dex_return_error_if_fail (FOUNDRY_IS_JSON_INPUT_STREAM (self));

  promise = dex_promise_new_cancellable ();
  g_data_input_stream_read_upto_async (G_DATA_INPUT_STREAM (self),
                                       stop_chars,
                                       stop_chars_len,
                                       G_PRIORITY_DEFAULT,
                                       dex_promise_get_cancellable (promise),
                                       foundry_json_input_stream_read_upto_cb,
                                       dex_ref (promise));
  return DEX_FUTURE (promise);
}

FoundryJsonInputStream *
foundry_json_input_stream_new (GInputStream *base_stream,
                               gboolean      close_base_stream)
{
  g_return_val_if_fail (G_IS_INPUT_STREAM (base_stream), NULL);

  return g_object_new (FOUNDRY_TYPE_JSON_INPUT_STREAM,
                       "base-stream", base_stream,
                       "close-base-stream", close_base_stream,
                       NULL);
}
