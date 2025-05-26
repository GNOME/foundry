/* foundry-jsonrpc.c
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

#include "foundry-jsonrpc-private.h"

static void
_jsonrpc_client_call_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  g_autoptr(DexPromise) promise = user_data;
  GVariant *variant = NULL;
  GError *error = NULL;

  if (!jsonrpc_client_call_finish (JSONRPC_CLIENT (object), result, &variant, &error))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_variant (promise, g_steal_pointer (&variant));
}

DexFuture *
_jsonrpc_client_call (JsonrpcClient *client,
                      const char    *method,
                      GVariant      *params)
{
  DexPromise *promise;

  dex_return_error_if_fail (JSONRPC_IS_CLIENT (client));
  dex_return_error_if_fail (method != NULL);

  promise = dex_promise_new_cancellable ();

  jsonrpc_client_call_async (client,
                             method,
                             params,
                             dex_promise_get_cancellable (promise),
                             _jsonrpc_client_call_cb,
                             dex_ref (promise));

  return DEX_FUTURE (promise);
}

static void
_jsonrpc_client_send_notification_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  g_autoptr(DexPromise) promise = user_data;
  GError *error = NULL;

  if (!jsonrpc_client_send_notification_finish (JSONRPC_CLIENT (object), result, &error))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_boolean (promise, TRUE);
}

DexFuture *
_jsonrpc_client_send_notification (JsonrpcClient *client,
                                   const char    *method,
                                   GVariant      *params)
{
  DexPromise *promise;

  dex_return_error_if_fail (JSONRPC_IS_CLIENT (client));
  dex_return_error_if_fail (method != NULL);

  promise = dex_promise_new_cancellable ();

  jsonrpc_client_send_notification_async (client,
                                          method,
                                          params,
                                          dex_promise_get_cancellable (promise),
                                          _jsonrpc_client_send_notification_cb,
                                          dex_ref (promise));

  return DEX_FUTURE (promise);
}
