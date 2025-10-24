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
#include "foundry-forge-listing-page-private.h"
#include "foundry-model-manager.h"
#include "foundry-util.h"

typedef struct
{
  GListModel *flatten;
  GListStore *pages;
  GHashTable *page_to_model;
  guint       last_page;
  guint       auto_load : 1;
} FoundryForgeListingPrivate;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (FoundryForgeListing, foundry_forge_listing, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (FoundryForgeListing)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_N_PAGES,
  PROP_PAGE_SIZE,
  PROP_AUTO_LOAD,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_forge_listing_dispose (GObject *object)
{
  FoundryForgeListing *self = (FoundryForgeListing *)object;
  FoundryForgeListingPrivate *priv = foundry_forge_listing_get_instance_private (self);

  g_hash_table_remove_all (priv->page_to_model);
  g_list_store_remove_all (priv->pages);
  g_clear_object (&priv->flatten);

  G_OBJECT_CLASS (foundry_forge_listing_parent_class)->dispose (object);
}

static void
foundry_forge_listing_finalize (GObject *object)
{
  FoundryForgeListing *self = (FoundryForgeListing *)object;
  FoundryForgeListingPrivate *priv = foundry_forge_listing_get_instance_private (self);

  g_clear_pointer (&priv->page_to_model, g_hash_table_unref);
  g_clear_object (&priv->pages);

  G_OBJECT_CLASS (foundry_forge_listing_parent_class)->finalize (object);
}

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

    case PROP_AUTO_LOAD:
      g_value_set_boolean (value, foundry_forge_listing_get_auto_load (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_forge_listing_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  FoundryForgeListing *self = FOUNDRY_FORGE_LISTING (object);

  switch (prop_id)
    {
    case PROP_AUTO_LOAD:
      foundry_forge_listing_set_auto_load (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_forge_listing_class_init (FoundryForgeListingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_forge_listing_dispose;
  object_class->finalize = foundry_forge_listing_finalize;
  object_class->get_property = foundry_forge_listing_get_property;
  object_class->set_property = foundry_forge_listing_set_property;

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

  properties[PROP_AUTO_LOAD] =
    g_param_spec_boolean ("auto-load", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_forge_listing_init (FoundryForgeListing *self)
{
  FoundryForgeListingPrivate *priv = foundry_forge_listing_get_instance_private (self);

  priv->page_to_model = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
  priv->pages = g_list_store_new (FOUNDRY_TYPE_FORGE_LISTING_PAGE);
  priv->flatten = foundry_flatten_list_model_new (g_object_ref (G_LIST_MODEL (priv->pages)));

  g_signal_connect_object (priv->flatten,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);
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

static int
compare_page (gconstpointer a,
              gconstpointer b,
              gpointer      user_data)
{
  FoundryForgeListingPage *page_a = FOUNDRY_FORGE_LISTING_PAGE ((gpointer)a);
  FoundryForgeListingPage *page_b = FOUNDRY_FORGE_LISTING_PAGE ((gpointer)b);
  guint page_a_num = foundry_forge_listing_page_get_page (page_a);
  guint page_b_num = foundry_forge_listing_page_get_page (page_b);

  if (page_a_num < page_b_num)
    return -1;

  if (page_a_num > page_b_num)
    return 1;

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
  FoundryForgeListingPrivate *priv = foundry_forge_listing_get_instance_private (self);
  FoundryForgeListingPage *p;

  dex_return_error_if_fail (FOUNDRY_IS_FORGE_LISTING (self));

  if (FOUNDRY_FORGE_LISTING_GET_CLASS (self)->load_page == NULL)
    return foundry_future_new_not_supported ();

  if (!(p = g_hash_table_lookup (priv->page_to_model, GUINT_TO_POINTER (page))))
    {
      DexFuture *future = FOUNDRY_FORGE_LISTING_GET_CLASS (self)->load_page (self, page);

      p = foundry_forge_listing_page_new (future, page);
      g_hash_table_replace (priv->page_to_model, GUINT_TO_POINTER (page), p);
      g_list_store_insert_sorted (priv->pages, p, compare_page, NULL);
    }

  return foundry_forge_listing_page_await (p);
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

static GType
foundry_forge_listing_get_item_type (GListModel *model)
{
  return G_TYPE_OBJECT;
}

static guint
foundry_forge_listing_get_n_items (GListModel *model)
{
  FoundryForgeListing *self = FOUNDRY_FORGE_LISTING (model);
  FoundryForgeListingPrivate *priv = foundry_forge_listing_get_instance_private (self);

  if (priv->flatten != NULL)
    return g_list_model_get_n_items (priv->flatten);

  return 0;
}

static gpointer
foundry_forge_listing_get_item (GListModel *model,
                                guint       position)
{
  FoundryForgeListing *self = FOUNDRY_FORGE_LISTING (model);
  FoundryForgeListingPrivate *priv = foundry_forge_listing_get_instance_private (self);

  g_assert (FOUNDRY_IS_FORGE_LISTING (self));

  if (priv->flatten == NULL)
    return NULL;

  if (priv->auto_load &&
      position + 1 == g_list_model_get_n_items (G_LIST_MODEL (self)))
    {
      if (priv->last_page + 1 < foundry_forge_listing_get_n_pages (self))
        {
          priv->last_page++;
          dex_future_disown (foundry_forge_listing_load_page (self, priv->last_page));
        }
    }

  return g_list_model_get_item (priv->flatten, position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_forge_listing_get_item_type;
  iface->get_n_items = foundry_forge_listing_get_n_items;
  iface->get_item = foundry_forge_listing_get_item;
}

/**
 * foundry_forge_listing_get_auto_load:
 * @self: a [class@Foundry.ForgeListing]
 *
 * Gets the auto-load property.
 *
 * Returns: %TRUE if auto-loading is enabled
 *
 * Since: 1.1
 */
gboolean
foundry_forge_listing_get_auto_load (FoundryForgeListing *self)
{
  FoundryForgeListingPrivate *priv = foundry_forge_listing_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_FORGE_LISTING (self), FALSE);

  return priv->auto_load;
}

/**
 * foundry_forge_listing_set_auto_load:
 * @self: a [class@Foundry.ForgeListing]
 * @auto_load: %TRUE to enable auto-loading
 *
 * Sets the auto-load property.
 *
 * When enabled, the listing will automatically fetch the next page
 * when a request for an item falls into the last currently loaded page.
 *
 * Since: 1.1
 */
void
foundry_forge_listing_set_auto_load (FoundryForgeListing *self,
                                     gboolean             auto_load)
{
  FoundryForgeListingPrivate *priv = foundry_forge_listing_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_FORGE_LISTING (self));

  if (priv->auto_load != auto_load)
    {
      priv->auto_load = auto_load;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUTO_LOAD]);
    }
}
