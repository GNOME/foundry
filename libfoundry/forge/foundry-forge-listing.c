/* foundry-forge-listing.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-forge-listing.h"
#include "foundry-util.h"

G_DEFINE_ABSTRACT_TYPE (FoundryForgeListing, foundry_forge_listing, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_N_PAGES,
  PROP_PAGE_SIZE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_forge_listing_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundryForgeListing *self = FOUNDRY_FORGE_LISTING (object);

  switch (prop_id)
    {
    case PROP_N_PAGES:
      g_value_set_uint (value, foundry_forge_listing_get_n_pages (self));
      break;

    case PROP_PAGE_SIZE:
      g_value_set_uint (value, foundry_forge_listing_get_page_size (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_forge_listing_class_init (FoundryForgeListingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_forge_listing_get_property;

  properties[PROP_N_PAGES] =
    g_param_spec_uint ("n-pages", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_PAGE_SIZE] =
    g_param_spec_uint ("page-size", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_forge_listing_init (FoundryForgeListing *self)
{
}

/**
 * foundry_forge_listing_get_n_pages:
 * @self: a [class@Foundry.ForgeListing]
 *
 * Gets the number of pages.
 *
 * Since: 1.1
 */
guint
foundry_forge_listing_get_n_pages (FoundryForgeListing *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_LISTING (self), 0);

  if (FOUNDRY_FORGE_LISTING_GET_CLASS (self)->get_n_pages)
    return FOUNDRY_FORGE_LISTING_GET_CLASS (self)->get_n_pages (self);

  return 0;
}

/**
 * foundry_forge_listing_get_page_size:
 * @self: a [class@Foundry.ForgeListing]
 *
 * Gets the number of items per page.
 *
 * Since: 1.1
 */
guint
foundry_forge_listing_get_page_size (FoundryForgeListing *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_LISTING (self), 0);

  if (FOUNDRY_FORGE_LISTING_GET_CLASS (self)->get_page_size)
    return FOUNDRY_FORGE_LISTING_GET_CLASS (self)->get_page_size (self);

  return 0;
}

/**
 * foundry_forge_listing_load_page:
 * @self: a [class@Foundry.ForgeListing]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_forge_listing_load_page (FoundryForgeListing *self,
                                 guint                page)
{
  dex_return_error_if_fail (FOUNDRY_IS_FORGE_LISTING (self));

  if (FOUNDRY_FORGE_LISTING_GET_CLASS (self)->load_page)
    return FOUNDRY_FORGE_LISTING_GET_CLASS (self)->load_page (self, page);

  return foundry_future_new_not_supported ();
}

static DexFuture *
foundry_forge_listing_load_all_fiber (gpointer data)
{
  FoundryForgeListing *self = data;
  guint n_pages;

  g_assert (FOUNDRY_IS_FORGE_LISTING (self));

  n_pages = foundry_forge_listing_get_n_pages (self);

  for (guint i = 0; i < n_pages; i++)
    {
      g_autoptr(GError) error = NULL;

      if (!dex_await (foundry_forge_listing_load_page (self, i), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_take_object (g_object_ref (self));
}

/**
 * foundry_forge_listing_load_all:
 * @self: a [class@Foundry.ForgeListing]
 *
 * Tries to load all pages of results.
 *
 * This is done sequentially.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to @self
 *   or rejects with error.
 */
DexFuture *
foundry_forge_listing_load_all (FoundryForgeListing *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_FORGE_LISTING (self));

  return dex_scheduler_spawn (NULL, 0,
                              foundry_forge_listing_load_all_fiber,
                              g_object_ref (self),
                              g_object_unref);
}
