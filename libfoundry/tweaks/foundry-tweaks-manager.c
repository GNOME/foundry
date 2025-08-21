/* foundry-tweaks-manager.c
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

#include <libpeas.h>

#include "foundry-contextual-private.h"
#include "foundry-debug.h"
#include "foundry-model-manager.h"
#include "foundry-tweaks-manager.h"
#include "foundry-tweaks-provider-private.h"
#include "foundry-service-private.h"
#include "foundry-util-private.h"

struct _FoundryTweaksManager
{
  FoundryService    parent_instance;
  PeasExtensionSet *addins;
};

struct _FoundryTweaksManagerClass
{
  FoundryServiceClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryTweaksManager, foundry_tweaks_manager, FOUNDRY_TYPE_SERVICE)

static void
foundry_tweaks_manager_provider_added (PeasExtensionSet *set,
                                       PeasPluginInfo   *plugin_info,
                                       GObject          *addin,
                                       gpointer          user_data)
{
  FoundryTweaksManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_TWEAKS_PROVIDER (addin));
  g_assert (FOUNDRY_IS_TWEAKS_MANAGER (self));

  g_debug ("Adding FoundryTweaksProvider of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (_foundry_tweaks_provider_load (FOUNDRY_TWEAKS_PROVIDER (addin)));
}

static void
foundry_tweaks_manager_provider_removed (PeasExtensionSet *set,
                                         PeasPluginInfo   *plugin_info,
                                         GObject          *addin,
                                         gpointer          user_data)
{
  FoundryTweaksManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_TWEAKS_PROVIDER (addin));
  g_assert (FOUNDRY_IS_TWEAKS_MANAGER (self));

  g_debug ("Removing FoundryTweaksProvider of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (_foundry_tweaks_provider_unload (FOUNDRY_TWEAKS_PROVIDER (addin)));
}

static DexFuture *
foundry_tweaks_manager_start (FoundryService *service)
{
  FoundryTweaksManager *self = (FoundryTweaksManager *)service;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_SERVICE (service));
  g_assert (PEAS_IS_EXTENSION_SET (self->addins));

  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (foundry_tweaks_manager_provider_added),
                           self,
                           0);
  g_signal_connect_object (self->addins,
                           "extension-removed",
                           G_CALLBACK (foundry_tweaks_manager_provider_removed),
                           self,
                           0);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryTweaksProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures, _foundry_tweaks_provider_load (provider));
    }

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static DexFuture *
foundry_tweaks_manager_stop (FoundryService *service)
{
  FoundryTweaksManager *self = (FoundryTweaksManager *)service;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_SERVICE (service));

  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_tweaks_manager_provider_added),
                                        self);
  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_tweaks_manager_provider_removed),
                                        self);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryTweaksProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures, _foundry_tweaks_provider_unload (provider));
    }

  g_clear_object (&self->addins);

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static void
foundry_tweaks_manager_constructed (GObject *object)
{
  FoundryTweaksManager *self = (FoundryTweaksManager *)object;
  g_autoptr(FoundryContext) context = NULL;

  G_OBJECT_CLASS (foundry_tweaks_manager_parent_class)->constructed (object);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  self->addins = peas_extension_set_new (NULL,
                                         FOUNDRY_TYPE_TWEAKS_PROVIDER,
                                         "context", context,
                                         NULL);
}

static void
foundry_tweaks_manager_dispose (GObject *object)
{
  FoundryTweaksManager *self = (FoundryTweaksManager *)object;

  g_clear_object (&self->addins);

  G_OBJECT_CLASS (foundry_tweaks_manager_parent_class)->dispose (object);
}

static void
foundry_tweaks_manager_class_init (FoundryTweaksManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  object_class->constructed = foundry_tweaks_manager_constructed;
  object_class->dispose = foundry_tweaks_manager_dispose;

  service_class->start = foundry_tweaks_manager_start;
  service_class->stop = foundry_tweaks_manager_stop;
}

static void
foundry_tweaks_manager_init (FoundryTweaksManager *self)
{
}

static DexFuture *
foundry_tweaks_manager_list_children_fiber (FoundryTweaksManager *self,
                                            FoundryTweaksPath    *path)
{
  g_autoptr(GPtrArray) futures = NULL;
  g_autoptr(GListStore) store = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_TWEAKS_MANAGER (self));
  g_assert (path != NULL);

  futures = g_ptr_array_new_with_free_func (dex_unref);
  store = g_list_store_new (G_TYPE_LIST_MODEL);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryTweaksProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures, foundry_tweaks_provider_list_children (provider, path));
    }

  if (futures->len > 0)
    dex_await (foundry_future_all (futures), NULL);

  for (guint i = 0; i < futures->len; i++)
    {
      DexFuture *future = g_ptr_array_index (futures, i);
      g_autoptr(GListModel) model = dex_await_object (dex_ref (future), NULL);

      if (model != NULL)
        g_list_store_append (store, model);
    }

  return dex_future_new_take_object (foundry_flatten_list_model_new (G_LIST_MODEL (g_steal_pointer (&store))));
}

/**
 * foundry_tweaks_manager_list_children:
 * @self: a [class@Foundry.TweaksManager]
 * @path: a [struct@Foundry.TweaksPath]
 *
 * Lists the tweaks found at @path.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.Tweak].
 */
DexFuture *
foundry_tweaks_manager_list_children (FoundryTweaksManager *self,
                                      FoundryTweaksPath    *path)
{
  dex_return_error_if_fail (FOUNDRY_IS_TWEAKS_MANAGER (self));
  dex_return_error_if_fail (path != NULL);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (foundry_tweaks_manager_list_children_fiber),
                                  2,
                                  FOUNDRY_TYPE_TWEAKS_MANAGER, self,
                                  FOUNDRY_TYPE_TWEAKS_PATH, path);
}
