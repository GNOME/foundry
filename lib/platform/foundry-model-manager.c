/* foundry-model-manager.c
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

#include "eggflattenlistmodel.h"
#include "eggmaplistmodel.h"

#include "foundry-model-manager.h"

G_DEFINE_TYPE (FoundryModelManager, foundry_model_manager, G_TYPE_OBJECT)

static GListModel *
foundry_model_manager_real_flatten (FoundryModelManager *self,
                                    GListModel          *model)
{
  return G_LIST_MODEL (egg_flatten_list_model_new (model));
}

static GListModel *
foundry_model_manager_real_map (FoundryModelManager     *self,
                                GListModel              *model,
                                FoundryListModelMapFunc  map_func,
                                gpointer                 user_data,
                                GDestroyNotify           user_destroy)
{
  return G_LIST_MODEL (egg_map_list_model_new (model, map_func, user_data, user_destroy));
}

static void
foundry_model_manager_class_init (FoundryModelManagerClass *klass)
{
  klass->flatten = foundry_model_manager_real_flatten;
  klass->map = foundry_model_manager_real_map;
}

static void
foundry_model_manager_init (FoundryModelManager *self)
{
}

/**
 * foundry_model_manager_flatten:
 * @self: a [class@Foundry.ModelManager]
 * @model: (transfer full) (nullable): a [iface@Gio.ListModel]
 *
 * Returns: (transfer full):
 */
GListModel *
foundry_model_manager_flatten (FoundryModelManager *self,
                               GListModel          *model)
{
  return FOUNDRY_MODEL_MANAGER_GET_CLASS (self)->flatten (self, model);
}

/**
 * foundry_model_manager_map:
 * @self: a [class@Foundry.ModelManager]
 * @model: (transfer full) (nullable): a [iface@Gio.ListModel]
 *
 * Returns: (transfer full):
 */
GListModel *
foundry_model_manager_map (FoundryModelManager     *self,
                           GListModel              *model,
                           FoundryListModelMapFunc  map_func,
                           gpointer                 user_data,
                           GDestroyNotify           user_destroy)
{
  return FOUNDRY_MODEL_MANAGER_GET_CLASS (self)->map (self, model, map_func, user_data, user_destroy);
}
