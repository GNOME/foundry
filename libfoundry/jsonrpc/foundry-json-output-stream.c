/* foundry-json-output-stream.c
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
#include "foundry-json-output-stream.h"
#include "foundry-util-private.h"

struct _FoundryJsonOutputStream
{
  GDataOutputStream parent_instance;
};

G_DEFINE_FINAL_TYPE (FoundryJsonOutputStream, foundry_json_output_stream, G_TYPE_DATA_OUTPUT_STREAM)

static void
foundry_json_output_stream_class_init (FoundryJsonOutputStreamClass *klass)
{
}

static void
foundry_json_output_stream_init (FoundryJsonOutputStream *self)
{
}

typedef struct _Write
{
  GOutputStream *stream;
  GBytes *delimiter;
} Write;

static void
write_free (Write *state)
{
  g_clear_object (&state->stream);
  g_clear_pointer (&state->delimiter, g_bytes_unref);
  g_free (state);
}

static DexFuture *
foundry_json_output_stream_serialize_cb (DexFuture *completed,
                                         gpointer   user_data)
{
  Write *state = user_data;
  g_autoptr(GBytes) bytes = NULL;

  bytes = dex_await_boxed (dex_ref (completed), NULL);

  return _foundry_write_all_bytes (state->stream,
                                   (GBytes *[]) { bytes, state->delimiter },
                                   2);
}

/**
 * foundry_json_output_stream_write:
 * @self: a [class@Foundry.JsonOutputStream]
 * @node: the JSON node to be written
 * @delimiter: the delimiter to use as the message suffix
 *
 * The caller must not mutate @node after calling this function
 * until after the operation has completed.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   any value or rejects with error.
 */
DexFuture *
foundry_json_output_stream_write (FoundryJsonOutputStream *self,
                                  JsonNode                *node,
                                  GBytes                  *delimiter)
{
  Write *state;

  dex_return_error_if_fail (FOUNDRY_IS_JSON_OUTPUT_STREAM (self));
  dex_return_error_if_fail (node != NULL);
  dex_return_error_if_fail (delimiter != NULL);

  state = g_new0 (Write, 1);
  state->delimiter = g_bytes_ref (delimiter);
  state->stream = g_object_ref (G_OUTPUT_STREAM (self));

  return dex_future_then (foundry_json_node_to_bytes (node),
                          foundry_json_output_stream_serialize_cb,
                          state,
                          (GDestroyNotify) write_free);
}
