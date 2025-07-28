/* foundry-template-manager.c
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

#include "foundry-debug.h"
#include "foundry-model-manager.h"
#include "foundry-template-manager.h"
#include "foundry-template-provider-private.h"
#include "foundry-service-private.h"
#include "foundry-util-private.h"

struct _FoundryTemplateManager
{
  FoundryService    parent_instance;
  PeasExtensionSet *addins;
};

struct _FoundryTemplateManagerClass
{
  FoundryServiceClass parent_class;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryTemplateManager, foundry_template_manager, FOUNDRY_TYPE_SERVICE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
foundry_template_manager_provider_added (PeasExtensionSet *set,
                                         PeasPluginInfo   *plugin_info,
                                         GObject          *addin,
                                         gpointer          user_data)
{
  FoundryTemplateManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_TEMPLATE_PROVIDER (addin));
  g_assert (FOUNDRY_IS_TEMPLATE_MANAGER (self));

  g_debug ("Adding FoundryTemplateProvider of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_template_provider_load (FOUNDRY_TEMPLATE_PROVIDER (addin)));
}

static void
foundry_template_manager_provider_removed (PeasExtensionSet *set,
                                           PeasPluginInfo   *plugin_info,
                                           GObject          *addin,
                                           gpointer          user_data)
{
  FoundryTemplateManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_TEMPLATE_PROVIDER (addin));
  g_assert (FOUNDRY_IS_TEMPLATE_MANAGER (self));

  g_debug ("Removing FoundryTemplateProvider of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_template_provider_unload (FOUNDRY_TEMPLATE_PROVIDER (addin)));
}

static DexFuture *
foundry_template_manager_start_fiber (gpointer user_data)
{
  FoundryTemplateManager *self = user_data;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_TEMPLATE_MANAGER (self));
  g_assert (PEAS_IS_EXTENSION_SET (self->addins));

  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (foundry_template_manager_provider_added),
                           self,
                           0);
  g_signal_connect_object (self->addins,
                           "extension-removed",
                           G_CALLBACK (foundry_template_manager_provider_removed),
                           self,
                           0);
  g_signal_connect_object (self->addins,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryTemplateProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);
      g_ptr_array_add (futures, foundry_template_provider_load (provider));
    }

  g_list_model_items_changed (G_LIST_MODEL (self), 0, 0, n_items);

  if (futures->len > 0)
    dex_await (foundry_future_all (futures), NULL);

  return dex_future_new_true ();
}

static DexFuture *
foundry_template_manager_start (FoundryService *service)
{
  FoundryTemplateManager *self = (FoundryTemplateManager *)service;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_TEMPLATE_MANAGER (self));
  g_assert (PEAS_IS_EXTENSION_SET (self->addins));

  return dex_scheduler_spawn (NULL, 0,
                              foundry_template_manager_start_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static DexFuture *
foundry_template_manager_stop (FoundryService *service)
{
  FoundryTemplateManager *self = (FoundryTemplateManager *)service;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_SERVICE (service));

  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_template_manager_provider_added),
                                        self);
  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_template_manager_provider_removed),
                                        self);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryTemplateProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);
      g_ptr_array_add (futures, foundry_template_provider_unload (provider));
    }

  g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, 0);

  g_clear_object (&self->addins);

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static void
foundry_template_manager_constructed (GObject *object)
{
  FoundryTemplateManager *self = (FoundryTemplateManager *)object;
  g_autoptr(FoundryContext) context = NULL;

  G_OBJECT_CLASS (foundry_template_manager_parent_class)->constructed (object);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  self->addins = peas_extension_set_new (NULL,
                                         FOUNDRY_TYPE_TEMPLATE_PROVIDER,
                                         "context", context,
                                         NULL);
}

static void
foundry_template_manager_finalize (GObject *object)
{
  FoundryTemplateManager *self = (FoundryTemplateManager *)object;

  g_clear_object (&self->addins);

  G_OBJECT_CLASS (foundry_template_manager_parent_class)->finalize (object);
}

static void
foundry_template_manager_class_init (FoundryTemplateManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  object_class->constructed = foundry_template_manager_constructed;
  object_class->finalize = foundry_template_manager_finalize;

  service_class->start = foundry_template_manager_start;
  service_class->stop = foundry_template_manager_stop;
}

static void
foundry_template_manager_init (FoundryTemplateManager *self)
{
}

static GType
foundry_template_manager_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_TEMPLATE_PROVIDER;
}

static guint
foundry_template_manager_get_n_items (GListModel *model)
{
  FoundryTemplateManager *self = FOUNDRY_TEMPLATE_MANAGER (model);

  if (self->addins == NULL)
    return 0;

  return g_list_model_get_n_items (G_LIST_MODEL (self->addins));
}

static gpointer
foundry_template_manager_get_item (GListModel *model,
                                   guint       position)
{
  FoundryTemplateManager *self = FOUNDRY_TEMPLATE_MANAGER (model);

  if (self->addins == NULL)
    return NULL;

  return g_list_model_get_item (G_LIST_MODEL (self->addins), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_template_manager_get_item_type;
  iface->get_n_items = foundry_template_manager_get_n_items;
  iface->get_item = foundry_template_manager_get_item;
}

/**
 * foundry_template_manager_list_project_templates:
 * @self: a [class@Foundry.TemplateManager]
 *
 * Queries all [class@Foundry.TemplateProvider] for available
 * [class@Foundry.ProjectTemplate].
 *
 * The resulting module may not be fully populated by all providers
 * by time it resolves. You may await the completion of all providers
 * by awaiting [func@Foundry.list_model_await] for the completion
 * of all providers.
 *
 * This allows the consumer to get a dynamically populating list model
 * for user interfaces without delay.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.ProjectTemplate]
 */
DexFuture *
foundry_template_manager_list_project_templates (FoundryTemplateManager *self)
{
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  dex_return_error_if_fail (FOUNDRY_IS_TEMPLATE_MANAGER (self));

  if (self->addins == NULL)
    return foundry_future_new_not_supported ();

  futures = g_ptr_array_new_with_free_func (dex_unref);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryTemplateProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);
      g_ptr_array_add (futures, foundry_template_provider_list_project_templates (provider));
    }

  return _foundry_flatten_list_model_new_from_futures (futures);
}
