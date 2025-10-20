/* foundry-forge-listing-page.c
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

#include "foundry-forge-listing-page-private.h"

struct _FoundryForgeListingPage
{
  GObject     parent_instance;
  DexPromise *promise;
  GListModel *model;
  guint       page;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_PAGE,
  N_PROPS
};

static GType
foundry_forge_listing_page_get_item_type (GListModel *model)
{
  return G_TYPE_OBJECT;
}

static guint
foundry_forge_listing_page_get_n_items (GListModel *model)
{
  FoundryForgeListingPage *self = FOUNDRY_FORGE_LISTING_PAGE (model);

  if (self->model != NULL)
    return g_list_model_get_n_items (self->model);

  return 0;
}

static gpointer
foundry_forge_listing_page_get_item (GListModel *model,
                                     guint       position)
{
  FoundryForgeListingPage *self = FOUNDRY_FORGE_LISTING_PAGE (model);

  if (self->model != NULL)
    return g_list_model_get_item (self->model, position);

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_forge_listing_page_get_item_type;
  iface->get_n_items = foundry_forge_listing_page_get_n_items;
  iface->get_item = foundry_forge_listing_page_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryForgeListingPage, foundry_forge_listing_page, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties[N_PROPS];

static void
foundry_forge_listing_page_set_model (FoundryForgeListingPage *self,
                                      GListModel              *model)
{
  guint n_items;

  g_assert (FOUNDRY_IS_FORGE_LISTING_PAGE (self));
  g_assert (!model || G_IS_LIST_MODEL (model));
  g_assert (self->model == NULL);

  if (model == NULL)
    return;

  g_signal_connect_object (model,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_set_object (&self->model, model);

  n_items = g_list_model_get_n_items (model);

  if (n_items > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, 0, n_items);
}

static void
foundry_forge_listing_page_finalize (GObject *object)
{
  FoundryForgeListingPage *self = (FoundryForgeListingPage *)object;

  g_clear_object (&self->model);
  dex_clear (&self->promise);

  G_OBJECT_CLASS (foundry_forge_listing_page_parent_class)->finalize (object);
}

static void
foundry_forge_listing_page_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  FoundryForgeListingPage *self = FOUNDRY_FORGE_LISTING_PAGE (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, foundry_forge_listing_page_get_model (self));
      break;

    case PROP_PAGE:
      g_value_set_uint (value, foundry_forge_listing_page_get_page (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_forge_listing_page_class_init (FoundryForgeListingPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_forge_listing_page_finalize;
  object_class->get_property = foundry_forge_listing_page_get_property;

  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PAGE] =
    g_param_spec_uint ("page", NULL, NULL,
                       0, G_MAXUINT-1, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_forge_listing_page_init (FoundryForgeListingPage *self)
{
  self->promise = dex_promise_new ();
}

static DexFuture *
model_acquired (DexFuture *completed,
                gpointer   user_data)
{
  FoundryForgeListingPage *self = user_data;
  g_autoptr(GListModel) model = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (FOUNDRY_IS_FORGE_LISTING_PAGE (self));

  if ((model = dex_await_object (dex_ref (completed), &error)))
    {
      foundry_forge_listing_page_set_model (self, model);
      dex_promise_resolve_boolean (self->promise, TRUE);
    }
  else
    {
      dex_promise_reject (self->promise, g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

FoundryForgeListingPage *
foundry_forge_listing_page_new (DexFuture *future,
                                guint      page)
{
  FoundryForgeListingPage *self;

  g_return_val_if_fail (DEX_IS_FUTURE (future), NULL);
  g_return_val_if_fail (page < G_MAXUINT, NULL);

  self = g_object_new (FOUNDRY_TYPE_FORGE_LISTING_PAGE, NULL);
  self->page = page;

  dex_future_disown (dex_future_finally (future,
                                         model_acquired,
                                         g_object_ref (self),
                                         g_object_unref));

  return self;
}

GListModel *
foundry_forge_listing_page_get_model (FoundryForgeListingPage *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_LISTING_PAGE (self), NULL);

  return self->model;
}

guint
foundry_forge_listing_page_get_page (FoundryForgeListingPage *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_LISTING_PAGE (self), 0);

  return self->page;
}

DexFuture *
foundry_forge_listing_page_await (FoundryForgeListingPage *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_FORGE_LISTING_PAGE (self));

  return dex_ref (DEX_FUTURE (self->promise));
}
