/* foundry-future-list-model.c
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

#include "foundry-future-list-model.h"

struct _FoundryFutureListModel
{
  GObject     parent_instance;
  GListModel *model;
  DexFuture *future;
};

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

static GType
foundry_future_list_model_get_item_type (GListModel *model)
{
  return g_list_model_get_item_type (FOUNDRY_FUTURE_LIST_MODEL (model)->model);
}

static guint
foundry_future_list_model_get_n_items (GListModel *model)
{
  return g_list_model_get_n_items (FOUNDRY_FUTURE_LIST_MODEL (model)->model);
}

static gpointer
foundry_future_list_model_get_item (GListModel *model,
                                    guint       position)
{
  return g_list_model_get_item (FOUNDRY_FUTURE_LIST_MODEL (model)->model, position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_future_list_model_get_item_type;
  iface->get_n_items = foundry_future_list_model_get_n_items;
  iface->get_item = foundry_future_list_model_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryFutureListModel, foundry_future_list_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties[N_PROPS];

static void
foundry_future_list_model_finalize (GObject *object)
{
  FoundryFutureListModel *self = (FoundryFutureListModel *)object;

  g_clear_object (&self->model);
  dex_clear (&self->future);

  G_OBJECT_CLASS (foundry_future_list_model_parent_class)->finalize (object);
}

static void
foundry_future_list_model_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  FoundryFutureListModel *self = FOUNDRY_FUTURE_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_future_list_model_class_init (FoundryFutureListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_future_list_model_finalize;
  object_class->get_property = foundry_future_list_model_get_property;

  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_future_list_model_init (FoundryFutureListModel *self)
{
}

FoundryFutureListModel *
foundry_future_list_model_new (GListModel *model,
                               DexFuture  *future)
{
  FoundryFutureListModel *self;

  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);
  g_return_val_if_fail (DEX_IS_FUTURE (future), NULL);

  self = g_object_new (FOUNDRY_TYPE_FUTURE_LIST_MODEL, NULL);
  self->model = g_object_ref (model);
  self->future = dex_ref (future);

  g_signal_connect_object (model,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);

  return self;
}

/**
 * foundry_future_list_model_await:
 * @self: a #FoundryFutureListModel
 *
 * Returns: (transfer full): a #DexFuture that resolves when the
 *   model is considered populated.
 */
DexFuture *
foundry_future_list_model_await (FoundryFutureListModel *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FUTURE_LIST_MODEL (self), NULL);

  return dex_ref (self->future);
}
