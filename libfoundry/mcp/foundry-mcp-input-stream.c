/*
 * foundry-mcp-input-stream.c
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <json-glib/json-glib.h>

#include "foundry-mcp-input-stream-private.h"

struct _FoundryMcpInputStream
{
  GDataInputStream parent_instance;
};

G_DEFINE_FINAL_TYPE (FoundryMcpInputStream, foundry_mcp_input_stream, G_TYPE_DATA_INPUT_STREAM)

FoundryMcpInputStream *
foundry_mcp_input_stream_new (GInputStream *base_stream,
                              gboolean      close_base_stream)
{
  g_return_val_if_fail (G_IS_INPUT_STREAM (base_stream), NULL);

  return g_object_new (FOUNDRY_TYPE_MCP_INPUT_STREAM,
                       "base-stream", base_stream,
                       "close-base-stream", close_base_stream,
                       NULL);
}

static void
foundry_mcp_input_stream_dispose (GObject *object)
{
  G_OBJECT_CLASS (foundry_mcp_input_stream_parent_class)->dispose (object);
}

static void
foundry_mcp_input_stream_class_init (FoundryMcpInputStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_mcp_input_stream_dispose;
}

static void
foundry_mcp_input_stream_init (FoundryMcpInputStream *self)
{
}

static GVariant *
foundry_mcp_input_stream_parse (FoundryMcpInputStream  *self,
                                const char             *data,
                                gsize                   len,
                                GError                **error)
{
  g_autoptr(JsonParser) parser = NULL;
  JsonNode *root = NULL;

  g_assert (FOUNDRY_IS_MCP_INPUT_STREAM (self));

  parser = json_parser_new ();

  if (!json_parser_load_from_data (parser, data, len, error))
    return NULL;

  root = json_parser_get_root (parser);

  if (JSON_NODE_HOLDS_ARRAY (root))
    return json_gvariant_deserialize (root, "aa{sv}", error);
  else
    return json_gvariant_deserialize (root, "a{sv}", error);
}

static void
foundry_mcp_input_stream_read_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  FoundryMcpInputStream *self = (FoundryMcpInputStream *)object;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GVariant) message = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *contents = NULL;
  gsize len = 0;

  g_assert (FOUNDRY_IS_MCP_INPUT_STREAM (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!(contents = g_data_input_stream_read_upto_finish (G_DATA_INPUT_STREAM (self), result, &len, &error)) ||
      !(message = foundry_mcp_input_stream_parse (self, contents, len, &error)))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_variant (promise, g_steal_pointer (&message));

  g_data_input_stream_read_byte (G_DATA_INPUT_STREAM (self), NULL, NULL);
}

/**
 * foundry_mcp_input_stream_read:
 *
 * Gets the next message from the stream.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [struct@GLib.Variant] or rejects with error.
 */
DexFuture *
foundry_mcp_input_stream_read (FoundryMcpInputStream *self)
{
  DexPromise *promise;

  dex_return_error_if_fail (FOUNDRY_IS_MCP_INPUT_STREAM (self));

  promise = dex_promise_new_cancellable ();

  g_data_input_stream_read_upto_async (G_DATA_INPUT_STREAM (self),
                                       "\n", 1,
                                       G_PRIORITY_DEFAULT,
                                       dex_promise_get_cancellable (promise),
                                       foundry_mcp_input_stream_read_cb,
                                       dex_ref (promise));

  return DEX_FUTURE (promise);
}
