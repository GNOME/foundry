/* foundry-json.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

static DexFuture *
foundry_json_read_complete (DexFuture *completed,
                            gpointer   user_data)
{
  JsonParser *parser = user_data;
  g_autoptr(GInputStream) stream = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (JSON_IS_PARSER (parser));

  stream = dex_await_object (dex_ref (completed), NULL);
  g_assert (G_IS_INPUT_STREAM (stream));

  return foundry_json_parser_load_from_stream (parser, stream);
}

/**
 * foundry_json_parser_load_from_file:
 * @parser: a [class@Json.Parser]
 * @file: a [iface@Gio.File]
 *
 * Loads @file into @parser and returns a future that resolves
 * when that has completed.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves
 *   to a boolean
 */
DexFuture *
foundry_json_parser_load_from_file (JsonParser *parser,
                                    GFile      *file)
{
  DexFuture *future;

  dex_return_error_if_fail (JSON_IS_PARSER (parser));
  dex_return_error_if_fail (G_IS_FILE (file));

  future = dex_file_read (file, G_PRIORITY_DEFAULT);
  future = dex_future_then (future,
                            foundry_json_read_complete,
                            g_object_ref (parser),
                            g_object_unref);

  return future;
}

static void
foundry_json_parser_load_from_stream_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  JsonParser *parser = (JsonParser *)object;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (JSON_IS_PARSER (parser));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!json_parser_load_from_stream_finish (parser, result, &error))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_boolean (promise, TRUE);
}

/**
 * foundry_json_parser_load_from_stream:
 * @parser: a [class@Json.Parser]
 * @stream: a [iface@Gio.InputStream]
 *
 * Like json_parser_load_from_stream() but asynchronous and returns
 * a [class@Dex.Future] which can be awaited upon.
 *
 * Returns: (transfer full): a [class@Dex.Future].
 */
DexFuture *
foundry_json_parser_load_from_stream (JsonParser   *parser,
                                      GInputStream *stream)
{
  DexPromise *promise;

  dex_return_error_if_fail (JSON_IS_PARSER (parser));
  dex_return_error_if_fail (G_IS_INPUT_STREAM (stream));

  promise = dex_promise_new_cancellable ();

  json_parser_load_from_stream_async (parser,
                                      stream,
                                      dex_promise_get_cancellable (promise),
                                      foundry_json_parser_load_from_stream_cb,
                                      dex_ref (promise));

  return DEX_FUTURE (promise);
}

const char *
foundry_json_node_get_string_at (JsonNode     *node,
                                 const char   *first_key,
                                 ...)
{
  const char *key = first_key;
  va_list args;

  if (node == NULL)
    return NULL;

  va_start (args, first_key);

  while (node != NULL && key != NULL)
    {
      JsonObject *object;

      if (!JSON_NODE_HOLDS_OBJECT (node))
        {
          node = NULL;
          break;
        }

      object = json_node_get_object (node);

      if (!json_object_has_member (object, key))
        {
          node = NULL;
          break;
        }

      node = json_object_get_member (object, key);
      key = va_arg (args, const char *);
    }

  va_end (args);

  if (node != NULL && JSON_NODE_HOLDS_VALUE (node))
    return json_node_get_string (node);

  return NULL;
}
