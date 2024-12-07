/* test-future-list-model.c
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

#include <foundry.h>

static void
test_basic1 (void)
{
  g_autoptr(GListStore) base = NULL;
  g_autoptr(FoundryFutureListModel) model = NULL;
  g_autoptr(DexPromise) promise = NULL;
  g_autoptr(DexFuture) other = NULL;

  base = g_list_store_new (G_TYPE_OBJECT);
  promise = dex_promise_new ();
  model = foundry_future_list_model_new (G_LIST_MODEL (base), DEX_FUTURE (promise));
  other = foundry_future_list_model_await (model);

  g_assert_true (other == DEX_FUTURE (promise));
}

int
main (int argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Foundry/FutureListModel/basic", test_basic1);
  return g_test_run ();
}
