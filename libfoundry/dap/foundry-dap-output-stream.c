/* foundry-dap-output-stream.c
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

#include "foundry-dap-output-stream-private.h"

struct _FoundryDapOutputStream
{
  GDataOutputStream parent_instance;
};

G_DEFINE_FINAL_TYPE (FoundryDapOutputStream, foundry_dap_output_stream, G_TYPE_DATA_OUTPUT_STREAM)

static void
foundry_dap_output_stream_class_init (FoundryDapOutputStreamClass *klass)
{
}

static void
foundry_dap_output_stream_init (FoundryDapOutputStream *self)
{
}

FoundryDapOutputStream *
foundry_dap_output_stream_new (GOutputStream *base_stream,
                               gboolean       close_base_stream)
{
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (base_stream), NULL);

  return g_object_new (FOUNDRY_TYPE_DAP_OUTPUT_STREAM,
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
foundry_dap_output_stream_writev_all_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  g_autoptr(Write) state = user_data;
  g_autoptr(GError) error = NULL;
  gsize n_written = 0;

  g_assert (FOUNDRY_IS_DAP_OUTPUT_STREAM (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);

  if (!g_output_stream_writev_all_finish (G_OUTPUT_STREAM (object), result, &n_written, &error))
    dex_promise_reject (state->promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_boolean (state->promise, TRUE);
}

/**
 * foundry_dap_output_stream_write:
 * @self: a [class@Foundry.DapOutputStream]
 *
 * Returns: (transfer full):
 */
DexFuture *
foundry_dap_output_stream_write (FoundryDapOutputStream *self,
                                 GBytes                 *bytes)
{
  DexPromise *promise;
  GOutputVector vectors[2];
  Write *state;
  gsize len;

  dex_return_error_if_fail (FOUNDRY_IS_DAP_OUTPUT_STREAM (self));
  dex_return_error_if_fail (bytes != NULL);

  len = g_bytes_get_size (bytes);
  promise = dex_promise_new_cancellable ();

  state = g_new0 (Write, 1);
  state->headers = g_strdup_printf ("Content-Length: %"G_GSIZE_FORMAT"\r\n\r\n", len);
  state->bytes = g_bytes_ref (bytes);
  state->promise = dex_ref (promise);

  vectors[0].buffer = state->headers;
  vectors[0].size = strlen (state->headers);
  vectors[1].buffer = g_bytes_get_data (bytes, NULL);
  vectors[1].size = len;

  g_output_stream_writev_all_async (G_OUTPUT_STREAM (self),
                                    vectors,
                                    G_N_ELEMENTS (vectors),
                                    G_PRIORITY_DEFAULT,
                                    dex_promise_get_cancellable (promise),
                                    foundry_dap_output_stream_writev_all_cb,
                                    g_steal_pointer (&state));

  return DEX_FUTURE (promise);
}
