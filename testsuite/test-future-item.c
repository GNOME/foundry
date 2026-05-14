/* test-future-item.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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
increment_uint (GObject    *object,
                GParamSpec *pspec,
                gpointer    user_data)
{
  guint *count = user_data;

  (*count)++;
}

static void
test_future_item_resolve (void)
{
  g_autoptr(FoundryFutureItem) item = NULL;
  g_autoptr(GObject) object = NULL;
  g_autoptr(GObject) resolved = NULL;

  object = g_object_new (G_TYPE_OBJECT, NULL);
  item = foundry_future_item_new (dex_future_new_for_object (object));

  resolved = foundry_future_item_dup_item (item);

  g_assert_true (resolved == object);
}

static void
test_future_item_reject (void)
{
  g_autoptr(FoundryFutureItem) item = NULL;
  g_autoptr(GObject) resolved = NULL;

  item = foundry_future_item_new (dex_future_new_reject (G_IO_ERROR,
                                                         G_IO_ERROR_FAILED,
                                                         "Failed"));

  resolved = foundry_future_item_dup_item (item);

  g_assert_null (resolved);
}

static void
test_future_item_replace (void)
{
  g_autoptr(FoundryFutureItem) item = NULL;
  g_autoptr(DexPromise) first = NULL;
  g_autoptr(DexPromise) second = NULL;
  g_autoptr(GObject) first_object = NULL;
  g_autoptr(GObject) second_object = NULL;
  g_autoptr(GObject) resolved = NULL;

  first = dex_promise_new ();
  second = dex_promise_new ();
  first_object = g_object_new (G_TYPE_OBJECT, NULL);
  second_object = g_object_new (G_TYPE_OBJECT, NULL);

  item = foundry_future_item_new (dex_ref (DEX_FUTURE (first)));
  foundry_future_item_set_future (item, DEX_FUTURE (second));

  dex_promise_resolve_object (first, g_object_ref (first_object));
  dex_promise_resolve_object (second, g_object_ref (second_object));

  resolved = foundry_future_item_dup_item (item);

  g_assert_true (resolved == second_object);
}

static void
test_future_item_null (void)
{
  g_autoptr(FoundryFutureItem) item = NULL;
  g_autoptr(GObject) resolved = NULL;

  item = foundry_future_item_new (NULL);
  resolved = foundry_future_item_dup_item (item);

  g_assert_null (resolved);
}

static void
test_future_item_notify (void)
{
  g_autoptr(FoundryFutureItem) item = NULL;
  g_autoptr(DexPromise) promise = NULL;
  g_autoptr(GObject) object = NULL;
  guint count = 0;

  item = foundry_future_item_new (NULL);
  promise = dex_promise_new ();
  object = g_object_new (G_TYPE_OBJECT, NULL);

  g_signal_connect (item, "notify::item", G_CALLBACK (increment_uint), &count);

  foundry_future_item_set_future (item, DEX_FUTURE (promise));
  g_assert_cmpuint (count, ==, 1);

  dex_promise_resolve_object (promise, g_object_ref (object));

  while (count < 2)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpuint (count, ==, 2);
}

int
main (int   argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Foundry/FutureItem/resolve", test_future_item_resolve);
  g_test_add_func ("/Foundry/FutureItem/reject", test_future_item_reject);
  g_test_add_func ("/Foundry/FutureItem/replace", test_future_item_replace);
  g_test_add_func ("/Foundry/FutureItem/null", test_future_item_null);
  g_test_add_func ("/Foundry/FutureItem/notify", test_future_item_notify);
  return g_test_run ();
}
