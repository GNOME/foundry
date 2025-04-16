/* foundry-dap-input-stream.c
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

#include <string.h>

#include "foundry-dap-input-stream-private.h"
#include "foundry-util-private.h"

struct _FoundryDapInputStream
{
  GDataInputStream parent_instance;
  gint64 max_size_bytes;
};

G_DEFINE_FINAL_TYPE (FoundryDapInputStream, foundry_dap_input_stream, G_TYPE_DATA_INPUT_STREAM)

static void
foundry_dap_input_stream_class_init (FoundryDapInputStreamClass *klass)
{
}

static void
foundry_dap_input_stream_init (FoundryDapInputStream *self)
{
  self->max_size_bytes = 1024 * 1024 * 1024; /* 1 GB */

  g_data_input_stream_set_newline_type (G_DATA_INPUT_STREAM (self),
                                        G_DATA_STREAM_NEWLINE_TYPE_ANY);
}

FoundryDapInputStream *
foundry_dap_input_stream_new (GInputStream *base_stream,
                              gboolean      close_base_stream)
{
  g_return_val_if_fail (G_IS_INPUT_STREAM (base_stream), NULL);

  return g_object_new (FOUNDRY_TYPE_DAP_INPUT_STREAM,
                       "base-stream", base_stream,
                       "close-base-stream", close_base_stream,
                       NULL);
}

static DexFuture *
foundry_dap_input_stream_read_fiber (gpointer user_data)
{
  FoundryDapInputStream *self = user_data;
  g_autoptr(GError) error = NULL;
  gint64 content_length = 0;

  g_assert (FOUNDRY_IS_DAP_INPUT_STREAM (self));

  for (;;)
    {
      g_autofree char *line = NULL;

      if (!(line = dex_await_string (_g_data_input_stream_read_line_utf8 (G_DATA_INPUT_STREAM (self)), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      /* Check for end of headers */
      if (line[0] == 0)
        break;

      if (strncasecmp ("Content-Length: ", line, 16) == 0)
        {
          const gchar *lenptr = line + 16;

          content_length = g_ascii_strtoll (lenptr, NULL, 10);

          if (((content_length == G_MININT64 || content_length == G_MAXINT64) && errno == ERANGE) ||
              (content_length < 0) ||
              (content_length == G_MAXSSIZE) ||
              (content_length > self->max_size_bytes))
            return dex_future_new_reject (G_IO_ERROR,
                                          G_IO_ERROR_INVALID_DATA,
                                          "Invalid Content-Length received from peer");
        }
    }

  if (content_length == 0)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "Content-Length invalid or missing");

  return _g_input_stream_read_bytes (G_INPUT_STREAM (self), content_length);
}

/**
 * foundry_dap_input_stream_read:
 * @self: a [class@Foundry.DapInputStream]
 *
 * Reads the next message from the peer.
 *
 * The result is a [class@GLib.Bytes] based on the Content-Length
 * provided by the peer. The bytes only contains the message
 * contents, not any headers.
 *
 * Returns: (transfer full):
 */
DexFuture *
foundry_dap_input_stream_read (FoundryDapInputStream *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_DAP_INPUT_STREAM (self));

  return dex_scheduler_spawn (NULL, 0,
                              foundry_dap_input_stream_read_fiber,
                              g_object_ref (self),
                              g_object_unref);
}
