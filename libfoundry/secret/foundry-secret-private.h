/* foundry-secret-private.h
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

#pragma once

#include <libdex.h>
#include <libsecret/secret.h>

G_BEGIN_DECLS

static inline void
foundry_secret_password_store_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!secret_password_store_finish (result, &error))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_boolean (promise, TRUE);
}

static inline DexFuture *
foundry_secret_password_storev (const SecretSchema *schema,
                                GHashTable         *attributes,
                                const gchar        *collection,
                                const gchar        *label,
                                const gchar        *password)
{
  DexPromise *promise = dex_promise_new_cancellable ();
  secret_password_storev (schema, attributes, collection, label, password,
                          dex_promise_get_cancellable (promise),
                          foundry_secret_password_store_cb,
                          dex_ref (promise));
  return DEX_FUTURE (promise);
}

static inline void
foundry_secret_password_lookup_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *value = NULL;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!(value = secret_password_lookup_finish (result, &error)))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_string (promise, g_steal_pointer (&value));
}

static inline DexFuture *
foundry_secret_password_lookupv (const SecretSchema *schema,
                                 GHashTable         *attributes)
{
  DexPromise *promise = dex_promise_new_cancellable ();
  secret_password_lookupv (schema, attributes,
                           dex_promise_get_cancellable (promise),
                           foundry_secret_password_lookup_cb,
                           dex_ref (promise));
  return DEX_FUTURE (promise);
}

static inline void
foundry_secret_password_clear_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!secret_password_clear_finish (result, &error))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_boolean (promise, TRUE);
}

static inline DexFuture *
foundry_secret_password_clearv (const SecretSchema *schema,
                                GHashTable         *attributes)
{
  DexPromise *promise = dex_promise_new_cancellable ();
  secret_password_clearv (schema, attributes,
                          dex_promise_get_cancellable (promise),
                          foundry_secret_password_clear_cb,
                          dex_ref (promise));
  return DEX_FUTURE (promise);
}

G_END_DECLS
