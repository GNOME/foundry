/* foundry-documentation-manager.c
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

#include <glib/gstdio.h>

#include <libpeas.h>

#include "eggflattenlistmodel.h"

#include "foundry-config.h"
#include "foundry-documentation.h"
#include "foundry-documentation-manager.h"
#include "foundry-documentation-provider-private.h"
#include "foundry-documentation-provider.h"
#include "foundry-future-list-model.h"
#include "foundry-inhibitor.h"
#include "foundry-service-private.h"
#include "foundry-util-private.h"

struct _FoundryDocumentationManager
{
  FoundryService    parent_instance;
  PeasExtensionSet *addins;
};

struct _FoundryDocumentationManagerClass
{
  FoundryServiceClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryDocumentationManager, foundry_documentation_manager, FOUNDRY_TYPE_SERVICE)

static void
foundry_documentation_manager_provider_added (PeasExtensionSet *set,
                                              PeasPluginInfo   *plugin_info,
                                              GObject          *addin,
                                              gpointer          user_data)
{
  FoundryDocumentationManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_DOCUMENTATION_PROVIDER (addin));
  g_assert (FOUNDRY_IS_DOCUMENTATION_MANAGER (self));

  g_debug ("Adding FoundryDocumentationProvider of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_documentation_provider_load (FOUNDRY_DOCUMENTATION_PROVIDER (addin)));
}

static void
foundry_documentation_manager_provider_removed (PeasExtensionSet *set,
                                                PeasPluginInfo   *plugin_info,
                                                GObject          *addin,
                                                gpointer          user_data)
{
  FoundryDocumentationManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_DOCUMENTATION_PROVIDER (addin));
  g_assert (FOUNDRY_IS_DOCUMENTATION_MANAGER (self));

  g_debug ("Removing FoundryDocumentationProvider of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_documentation_provider_unload (FOUNDRY_DOCUMENTATION_PROVIDER (addin)));
}

static DexFuture *
foundry_documentation_manager_start_fiber (gpointer user_data)
{
  FoundryDocumentationManager *self = user_data;
  g_autoptr(FoundryDocumentation) documentation = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(EggFlattenListModel) flatten_roots = NULL;
  g_autoptr(GListStore) all_roots = NULL;
  g_autoptr(GPtrArray) futures = NULL;
  g_autofree char *documentation_id = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_DOCUMENTATION_MANAGER (self));
  g_assert (PEAS_IS_EXTENSION_SET (self->addins));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (foundry_documentation_manager_provider_added),
                           self,
                           0);
  g_signal_connect_object (self->addins,
                           "extension-removed",
                           G_CALLBACK (foundry_documentation_manager_provider_removed),
                           self,
                           0);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  /* First request that all of the providers pass the load phase */
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryDocumentationProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures, foundry_documentation_provider_load (provider));
    }

  if (futures->len > 0)
    {
      dex_await (foundry_future_all (futures), NULL);
      g_ptr_array_remove_range (futures, 0, futures->len);
    }

  /* Now collect all of the roots from various providers */
  all_roots = g_list_store_new (G_TYPE_LIST_MODEL);
  flatten_roots = egg_flatten_list_model_new (g_object_ref (G_LIST_MODEL (all_roots)));
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryDocumentationProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);
      g_autoptr(GListModel) roots = foundry_documentation_provider_list_roots (provider);

      g_list_store_append (all_roots, roots);
    }

  /* Now request that all the providers re-index the known bases */
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryDocumentationProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures, foundry_documentation_provider_index (provider, G_LIST_MODEL (flatten_roots)));
    }

  if (futures->len > 0)
    dex_await (foundry_future_all (futures), NULL);

  return dex_future_new_true ();
}

static DexFuture *
foundry_documentation_manager_start (FoundryService *service)
{
  FoundryDocumentationManager *self = (FoundryDocumentationManager *)service;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_DOCUMENTATION_MANAGER (self));
  g_assert (PEAS_IS_EXTENSION_SET (self->addins));

  return dex_scheduler_spawn (NULL, 0,
                              foundry_documentation_manager_start_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static DexFuture *
foundry_documentation_manager_stop (FoundryService *service)
{
  FoundryDocumentationManager *self = (FoundryDocumentationManager *)service;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_SERVICE (service));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_documentation_manager_provider_added),
                                        self);
  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_documentation_manager_provider_removed),
                                        self);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryDocumentationProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures, foundry_documentation_provider_unload (provider));
    }

  g_clear_object (&self->addins);

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static void
foundry_documentation_manager_constructed (GObject *object)
{
  FoundryDocumentationManager *self = (FoundryDocumentationManager *)object;
  g_autoptr(FoundryContext) context = NULL;

  G_OBJECT_CLASS (foundry_documentation_manager_parent_class)->constructed (object);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  self->addins = peas_extension_set_new (NULL,
                                         FOUNDRY_TYPE_DOCUMENTATION_PROVIDER,
                                         "context", context,
                                         NULL);
}

static void
foundry_documentation_manager_finalize (GObject *object)
{
  FoundryDocumentationManager *self = (FoundryDocumentationManager *)object;

  g_clear_object (&self->addins);

  G_OBJECT_CLASS (foundry_documentation_manager_parent_class)->finalize (object);
}

static void
foundry_documentation_manager_class_init (FoundryDocumentationManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  object_class->constructed = foundry_documentation_manager_constructed;
  object_class->finalize = foundry_documentation_manager_finalize;

  service_class->start = foundry_documentation_manager_start;
  service_class->stop = foundry_documentation_manager_stop;
}

static void
foundry_documentation_manager_init (FoundryDocumentationManager *self)
{
}
