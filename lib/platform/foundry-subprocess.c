/* foundry-subprocess.c
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

#include "foundry-subprocess.h"

static void
foundry_subprocess_communicate_utf8_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  GSubprocess *subprocess = (GSubprocess *)object;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *stdout_buf = NULL;

  g_assert (G_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!g_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_string (promise, g_steal_pointer (&stdout_buf));
}

/**
 * foundry_subprocess_communicate_utf8:
 * @subprocess: a #FoundrySubprocess
 * @stdin_buf: the standard input buffer
 *
 * Like g_subprocess_communicate_utf8() but only supports stdout and is
 * returned as a future to a string.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a string
 */
DexFuture *
foundry_subprocess_communicate_utf8 (GSubprocess *subprocess,
                                     const char  *stdin_buf)
{
  DexPromise *promise;

  dex_return_error_if_fail (G_IS_SUBPROCESS (subprocess));

  promise = dex_promise_new_cancellable ();
  g_subprocess_communicate_utf8_async (subprocess,
                                       stdin_buf,
                                       dex_promise_get_cancellable (promise),
                                       foundry_subprocess_communicate_utf8_cb,
                                       dex_ref (promise));
  return DEX_FUTURE (promise);
}
