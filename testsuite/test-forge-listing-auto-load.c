/* test-forge-listing-auto-load.c
 *
 * Copyright 2025 Christian Hergert
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

#include <libdex.h>

#include "foundry.h"

G_DECLARE_FINAL_TYPE (MockForgeListing, mock_forge_listing, MOCK, FORGE_LISTING, FoundryForgeListing)

struct _MockForgeListing
{
  FoundryForgeListing parent_instance;
  guint n_pages;
  guint page_size;
  guint load_page_calls;
};

G_DEFINE_FINAL_TYPE (MockForgeListing, mock_forge_listing, FOUNDRY_TYPE_FORGE_LISTING)

static guint
mock_forge_listing_get_n_pages (FoundryForgeListing *self)
{
  MockForgeListing *mock = (MockForgeListing *)self;
  return mock->n_pages;
}

static guint
mock_forge_listing_get_page_size (FoundryForgeListing *self)
{
  MockForgeListing *mock = (MockForgeListing *)self;
  return mock->page_size;
}

static DexFuture *
mock_forge_listing_load_page (FoundryForgeListing *self,
                              guint                page)
{
  MockForgeListing *mock = (MockForgeListing *)self;
  GListStore *store;
  guint i;

  mock->load_page_calls++;

  /* Simulate loading a page with some items */
  store = g_list_store_new (G_TYPE_OBJECT);

  for (i = 0; i < mock->page_size; i++)
    {
      g_autoptr(GObject) item = g_object_new (G_TYPE_OBJECT, NULL);

      g_list_store_append (store, item);
    }

  return dex_future_new_take_object (g_object_ref (G_LIST_MODEL (store)));
}

static void
mock_forge_listing_class_init (MockForgeListingClass *klass)
{
  FoundryForgeListingClass *listing_class = FOUNDRY_FORGE_LISTING_CLASS (klass);

  listing_class->get_n_pages = mock_forge_listing_get_n_pages;
  listing_class->get_page_size = mock_forge_listing_get_page_size;
  listing_class->load_page = mock_forge_listing_load_page;
}

static void
mock_forge_listing_init (MockForgeListing *self)
{
  self->n_pages = 3;
  self->page_size = 5;
  self->load_page_calls = 0;
}

static void
test_auto_load_disabled (void)
{
  MockForgeListing *listing;
  GError *error = NULL;

  listing = g_object_new (mock_forge_listing_get_type (), NULL);
  g_assert_nonnull (listing);

  /* Auto-load should be disabled by default */
  g_assert_false (foundry_forge_listing_get_auto_load (FOUNDRY_FORGE_LISTING (listing)));

  /* Load first page */
  g_assert_true (dex_await (foundry_forge_listing_load_page (FOUNDRY_FORGE_LISTING (listing), 0), &error));
  g_assert_no_error (error);
  g_assert_cmpuint (listing->load_page_calls, ==, 1);

  /* Access items from the first page - should not trigger auto-load */
  g_assert_nonnull (g_list_model_get_item (G_LIST_MODEL (listing), 0));
  g_assert_nonnull (g_list_model_get_item (G_LIST_MODEL (listing), 4));
  g_assert_cmpuint (listing->load_page_calls, ==, 1); /* Still only 1 call */

  g_object_unref (listing);
}

static void
test_auto_load_enabled (void)
{
  MockForgeListing *listing;
  GError *error = NULL;

  listing = g_object_new (mock_forge_listing_get_type (), NULL);
  g_assert_nonnull (listing);

  /* Enable auto-load */
  foundry_forge_listing_set_auto_load (FOUNDRY_FORGE_LISTING (listing), TRUE);
  g_assert_true (foundry_forge_listing_get_auto_load (FOUNDRY_FORGE_LISTING (listing)));

  /* Load first page */
  g_assert_true (dex_await (foundry_forge_listing_load_page (FOUNDRY_FORGE_LISTING (listing), 0), &error));
  g_assert_no_error (error);
  g_assert_cmpuint (listing->load_page_calls, ==, 1);

  /* Access items from the first page - should not trigger auto-load yet */
  g_object_unref (g_list_model_get_item (G_LIST_MODEL (listing), 0));
  g_assert_cmpuint (listing->load_page_calls, ==, 1);

  /* Access the last item of the first page - should trigger auto-load of next page */
  g_object_unref (g_list_model_get_item (G_LIST_MODEL (listing), 4));
  g_assert_cmpuint (listing->load_page_calls, ==, 2); /* Should have loaded page 1 */

  /* Access items from the second page - should trigger auto-load of third page */
  g_object_unref (g_list_model_get_item (G_LIST_MODEL (listing), 9));
  g_assert_cmpuint (listing->load_page_calls, ==, 3); /* Should have loaded page 2 */

  g_object_unref (listing);
}

int
main (int argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Foundry/ForgeListing/auto_load_disabled", test_auto_load_disabled);
  g_test_add_func ("/Foundry/ForgeListing/auto_load_enabled", test_auto_load_enabled);

  return g_test_run ();
}
