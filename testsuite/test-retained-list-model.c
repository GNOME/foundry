/* test-retained-list-model.c
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

#include <gio/gio.h>

#include "foundry-retained-list-model-private.h"

static void
test_basic (void)
{
  g_autoptr(FoundryRetainedListModel) model = NULL;
  g_autoptr(GListStore) base_store = NULL;
  g_autoptr(GObject) obj1 = NULL;
  g_autoptr(GObject) obj2 = NULL;
  g_autoptr(GObject) obj3 = NULL;
  guint n_items;

  base_store = g_list_store_new (G_TYPE_OBJECT);
  obj1 = g_object_new (G_TYPE_OBJECT, NULL);
  obj2 = g_object_new (G_TYPE_OBJECT, NULL);
  obj3 = g_object_new (G_TYPE_OBJECT, NULL);

  g_list_store_append (base_store, obj1);
  g_list_store_append (base_store, obj2);
  g_list_store_append (base_store, obj3);

  model = foundry_retained_list_model_new (g_object_ref (G_LIST_MODEL (base_store)));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  g_assert_cmpuint (n_items, ==, 3);

  {
    FoundryRetainedListItem *item = NULL;

    item = g_list_model_get_item (G_LIST_MODEL (model), 0);
    g_assert_nonnull (item);
    g_assert_true (foundry_retained_list_item_get_item (item) == obj1);
    g_object_unref (item);
  }

  {
    FoundryRetainedListItem *item = NULL;

    item = g_list_model_get_item (G_LIST_MODEL (model), 1);
    g_assert_nonnull (item);
    g_assert_true (foundry_retained_list_item_get_item (item) == obj2);
    g_object_unref (item);
  }

  {
    FoundryRetainedListItem *item = NULL;

    item = g_list_model_get_item (G_LIST_MODEL (model), 2);
    g_assert_nonnull (item);
    g_assert_true (foundry_retained_list_item_get_item (item) == obj3);
    g_object_unref (item);
  }

  g_assert_finalize_object (g_steal_pointer (&model));
  g_assert_finalize_object (g_steal_pointer (&base_store));
  g_assert_finalize_object (g_steal_pointer (&obj1));
  g_assert_finalize_object (g_steal_pointer (&obj2));
  g_assert_finalize_object (g_steal_pointer (&obj3));
}

static void
test_hold_release (void)
{
  g_autoptr(GListStore) base_store = NULL;
  g_autoptr(FoundryRetainedListModel) model = NULL;
  g_autoptr(FoundryRetainedListItem) item = NULL;
  g_autoptr(GObject) obj1 = NULL;
  g_autoptr(GObject) obj2 = NULL;
  guint n_items;

  base_store = g_list_store_new (G_TYPE_OBJECT);
  obj1 = g_object_new (G_TYPE_OBJECT, NULL);
  obj2 = g_object_new (G_TYPE_OBJECT, NULL);

  g_list_store_append (base_store, obj1);
  g_list_store_append (base_store, obj2);

  model = foundry_retained_list_model_new (g_object_ref (G_LIST_MODEL (base_store)));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  g_assert_cmpuint (n_items, ==, 2);

  item = g_list_model_get_item (G_LIST_MODEL (model), 0);
  g_assert_nonnull (item);

  foundry_retained_list_item_hold (item);
  foundry_retained_list_item_hold (item);

  g_list_store_remove (base_store, 0);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  g_assert_cmpuint (n_items, ==, 2);

  foundry_retained_list_item_release (item);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  g_assert_cmpuint (n_items, ==, 2);

  foundry_retained_list_item_release (item);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  g_assert_cmpuint (n_items, ==, 1);

  g_assert_finalize_object (g_steal_pointer (&model));
  g_assert_finalize_object (g_steal_pointer (&item));
  g_assert_finalize_object (g_steal_pointer (&base_store));
  g_assert_finalize_object (g_steal_pointer (&obj1));
  g_assert_finalize_object (g_steal_pointer (&obj2));
}

static void
test_multiple_hold_release (void)
{
  g_autoptr(GListStore) base_store = NULL;
  g_autoptr(FoundryRetainedListModel) model = NULL;
  g_autoptr(FoundryRetainedListItem) item = NULL;
  g_autoptr(GObject) obj1 = NULL;
  guint n_items;

  base_store = g_list_store_new (G_TYPE_OBJECT);
  obj1 = g_object_new (G_TYPE_OBJECT, NULL);

  g_list_store_append (base_store, obj1);

  model = foundry_retained_list_model_new (g_object_ref (G_LIST_MODEL (base_store)));

  item = g_list_model_get_item (G_LIST_MODEL (model), 0);
  g_assert_nonnull (item);

  foundry_retained_list_item_hold (item);
  foundry_retained_list_item_hold (item);
  foundry_retained_list_item_hold (item);

  g_list_store_remove (base_store, 0);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  g_assert_cmpuint (n_items, ==, 1);

  foundry_retained_list_item_release (item);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  g_assert_cmpuint (n_items, ==, 1);

  foundry_retained_list_item_release (item);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  g_assert_cmpuint (n_items, ==, 1);

  foundry_retained_list_item_release (item);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  g_assert_cmpuint (n_items, ==, 0);

  g_assert_finalize_object (g_steal_pointer (&model));
  g_assert_finalize_object (g_steal_pointer (&item));
  g_assert_finalize_object (g_steal_pointer (&base_store));
  g_assert_finalize_object (g_steal_pointer (&obj1));
}

static void
test_finalize_after_release (void)
{
  g_autoptr(GListStore) base_store = NULL;
  g_autoptr(FoundryRetainedListModel) model = NULL;
  g_autoptr(FoundryRetainedListItem) item = NULL;
  g_autoptr(GObject) obj1 = NULL;
  guint n_items;

  base_store = g_list_store_new (G_TYPE_OBJECT);
  obj1 = g_object_new (G_TYPE_OBJECT, NULL);

  g_list_store_append (base_store, obj1);

  model = foundry_retained_list_model_new (g_object_ref (G_LIST_MODEL (base_store)));

  item = g_list_model_get_item (G_LIST_MODEL (model), 0);
  g_assert_nonnull (item);

  foundry_retained_list_item_hold (item);

  g_list_store_remove (base_store, 0);

  foundry_retained_list_item_release (item);

  /* Verify the item was removed from the model */
  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  g_assert_cmpuint (n_items, ==, 0);

  g_assert_finalize_object (g_steal_pointer (&model));
  g_assert_finalize_object (g_steal_pointer (&item));
  g_assert_finalize_object (g_steal_pointer (&base_store));
  g_assert_finalize_object (g_steal_pointer (&obj1));
}

static void
test_items_changed (void)
{
  g_autoptr(GListStore) base_store = NULL;
  g_autoptr(FoundryRetainedListModel) model = NULL;
  g_autoptr(GObject) obj1 = NULL;
  g_autoptr(GObject) obj2 = NULL;
  g_autoptr(GObject) obj3 = NULL;
  guint n_items;

  base_store = g_list_store_new (G_TYPE_OBJECT);
  obj1 = g_object_new (G_TYPE_OBJECT, NULL);
  obj2 = g_object_new (G_TYPE_OBJECT, NULL);
  obj3 = g_object_new (G_TYPE_OBJECT, NULL);

  g_list_store_append (base_store, obj1);
  g_list_store_append (base_store, obj2);

  model = foundry_retained_list_model_new (g_object_ref (G_LIST_MODEL (base_store)));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  g_assert_cmpuint (n_items, ==, 2);

  g_list_store_insert (base_store, 1, obj3);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  g_assert_cmpuint (n_items, ==, 3);

  g_list_store_remove (base_store, 0);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  g_assert_cmpuint (n_items, ==, 2);

  g_assert_finalize_object (g_steal_pointer (&model));
  g_assert_finalize_object (g_steal_pointer (&base_store));
  g_assert_finalize_object (g_steal_pointer (&obj1));
  g_assert_finalize_object (g_steal_pointer (&obj2));
  g_assert_finalize_object (g_steal_pointer (&obj3));
}

static void
test_insert_inbetween (void)
{
  g_autoptr(GListStore) base_store = NULL;
  g_autoptr(FoundryRetainedListModel) model = NULL;
  g_autoptr(FoundryRetainedListItem) retain1 = NULL;
  g_autoptr(FoundryRetainedListItem) retain2 = NULL;
  g_autoptr(FoundryRetainedListItem) retain2_2 = NULL;
  g_autoptr(FoundryRetainedListItem) retain3 = NULL;
  g_autoptr(FoundryRetainedListItem) retain4 = NULL;
  g_autoptr(FoundryRetainedListItem) retain5 = NULL;
  g_autoptr(GObject) obj1 = NULL;
  g_autoptr(GObject) obj2 = NULL;
  g_autoptr(GObject) obj3 = NULL;
  g_autoptr(GObject) obj4 = NULL;
  g_autoptr(GObject) obj5 = NULL;

  base_store = g_list_store_new (G_TYPE_OBJECT);
  obj1 = g_object_new (G_TYPE_OBJECT, NULL);
  obj2 = g_object_new (G_TYPE_OBJECT, NULL);
  obj3 = g_object_new (G_TYPE_OBJECT, NULL);
  obj4 = g_object_new (G_TYPE_OBJECT, NULL);
  obj5 = g_object_new (G_TYPE_OBJECT, NULL);

  g_list_store_append (base_store, obj1);
  g_list_store_append (base_store, obj2);
  g_list_store_append (base_store, obj3);

  g_print ("%p %p %p %p %p\n",
           obj1, obj2, obj3, obj4, obj5);

  model = foundry_retained_list_model_new (g_object_ref (G_LIST_MODEL (base_store)));

  retain1 = g_list_model_get_item (G_LIST_MODEL (model), 0);
  g_assert_true (FOUNDRY_IS_RETAINED_LIST_ITEM (retain1));
  g_assert_true (obj1 == foundry_retained_list_item_get_item (FOUNDRY_RETAINED_LIST_ITEM (retain1)));
  g_clear_object (&retain1);

  retain2 = g_list_model_get_item (G_LIST_MODEL (model), 1);
  g_assert_true (FOUNDRY_IS_RETAINED_LIST_ITEM (retain2));
  g_assert_true (obj2 == foundry_retained_list_item_get_item (FOUNDRY_RETAINED_LIST_ITEM (retain2)));
  foundry_retained_list_item_hold (retain2);
  g_list_store_remove (base_store, 1);
  g_list_store_insert (base_store, 1, obj4);
  g_list_store_append (base_store, obj5);
  retain2_2 = g_list_model_get_item (G_LIST_MODEL (model), 1);
  g_assert_true (retain2 == retain2_2);
  g_clear_object (&retain2_2);
  retain4 = g_list_model_get_item (G_LIST_MODEL (model), 2);
  g_assert_true (obj4 == foundry_retained_list_item_get_item (FOUNDRY_RETAINED_LIST_ITEM (retain4)));
  g_clear_object (&retain4);
  foundry_retained_list_item_release (retain2);
  g_assert_finalize_object (g_steal_pointer (&retain2));
  retain4 = g_list_model_get_item (G_LIST_MODEL (model), 1);
  g_assert_true (obj4 == foundry_retained_list_item_get_item (FOUNDRY_RETAINED_LIST_ITEM (retain4)));
  g_clear_object (&retain4);

  g_assert_cmpint (4, ==, g_list_model_get_n_items (G_LIST_MODEL (model)));
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (model), 4));

  retain1 = g_list_model_get_item (G_LIST_MODEL (model), 0);
  g_assert_true (obj1 == foundry_retained_list_item_get_item (FOUNDRY_RETAINED_LIST_ITEM (retain1)));
  g_clear_object (&retain1);

  retain4 = g_list_model_get_item (G_LIST_MODEL (model), 1);
  g_assert_true (obj4 == foundry_retained_list_item_get_item (FOUNDRY_RETAINED_LIST_ITEM (retain4)));
  g_clear_object (&retain4);

  retain3 = g_list_model_get_item (G_LIST_MODEL (model), 2);
  g_assert_true (obj3 == foundry_retained_list_item_get_item (FOUNDRY_RETAINED_LIST_ITEM (retain3)));
  g_clear_object (&retain3);

  retain5 = g_list_model_get_item (G_LIST_MODEL (model), 3);
  g_assert_nonnull (retain5);
  g_print ("%p\n", foundry_retained_list_item_get_item (FOUNDRY_RETAINED_LIST_ITEM (retain5)));
  g_assert_true (obj5 == foundry_retained_list_item_get_item (FOUNDRY_RETAINED_LIST_ITEM (retain5)));
  g_clear_object (&retain5);

  g_assert_finalize_object (g_steal_pointer (&model));
  g_assert_finalize_object (g_steal_pointer (&base_store));
  g_assert_finalize_object (g_steal_pointer (&obj1));
  g_assert_finalize_object (g_steal_pointer (&obj2));
  g_assert_finalize_object (g_steal_pointer (&obj3));
  g_assert_finalize_object (g_steal_pointer (&obj4));
  g_assert_finalize_object (g_steal_pointer (&obj5));
}

static void
test_release_after_finalize (void)
{
  g_autoptr(FoundryRetainedListModel) model = NULL;
  g_autoptr(FoundryRetainedListItem) retain1 = NULL;
  g_autoptr(GListStore) base_store = NULL;
  g_autoptr(GObject) obj1 = NULL;

  base_store = g_list_store_new (G_TYPE_OBJECT);
  model = foundry_retained_list_model_new (g_object_ref (G_LIST_MODEL (base_store)));
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (model), 0));

  obj1 = g_object_new (G_TYPE_OBJECT, NULL);
  g_list_store_append (base_store, obj1);

  retain1 = g_list_model_get_item (G_LIST_MODEL (model), 0);
  g_assert_true (G_IS_LIST_STORE (base_store));
  g_assert_true (FOUNDRY_IS_RETAINED_LIST_MODEL (model));
  g_assert_true (FOUNDRY_IS_RETAINED_LIST_ITEM (retain1));
  g_assert_true (G_IS_OBJECT (obj1));

  g_assert_finalize_object (g_steal_pointer (&model));
  g_assert_finalize_object (g_steal_pointer (&base_store));
  g_assert_finalize_object (g_steal_pointer (&retain1));
  g_assert_finalize_object (g_steal_pointer (&obj1));
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Foundry/RetainedListModel/basic", test_basic);
  g_test_add_func ("/Foundry/RetainedListModel/hold-release", test_hold_release);
  g_test_add_func ("/Foundry/RetainedListModel/multiple-hold-release", test_multiple_hold_release);
  g_test_add_func ("/Foundry/RetainedListModel/finalize-after-release", test_finalize_after_release);
  g_test_add_func ("/Foundry/RetainedListModel/items-changed", test_items_changed);
  g_test_add_func ("/Foundry/RetainedListModel/release-after-finalize", test_release_after_finalize);
  g_test_add_func ("/Foundry/RetainedListModel/insert-inbetween", test_insert_inbetween);
  return g_test_run ();
}
