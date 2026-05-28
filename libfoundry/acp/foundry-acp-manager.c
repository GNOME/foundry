/* foundry-acp-manager.c
 *
 * Copyright 2026 Christian Hergert
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <libpeas.h>

#include "foundry-acp-agent.h"
#include "foundry-acp-manager.h"
#include "foundry-acp-provider-private.h"
#include "foundry-contextual-private.h"
#include "foundry-debug.h"
#include "foundry-model-manager.h"
#include "foundry-service-private.h"
#include "foundry-util.h"

struct _FoundryAcpManager
{
  FoundryService    parent_instance;
  GListModel       *flatten;
  PeasExtensionSet *addins;
  DexFuture        *load_agents;
  guint             agents_requested : 1;
};

struct _FoundryAcpManagerClass
{
  FoundryServiceClass parent_class;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryAcpManager, foundry_acp_manager, FOUNDRY_TYPE_SERVICE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
foundry_acp_manager_provider_added (PeasExtensionSet *set,
                                    PeasPluginInfo   *plugin_info,
                                    GObject          *addin,
                                    gpointer          user_data)
{
  FoundryAcpManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_ACP_PROVIDER (addin));
  g_assert (FOUNDRY_IS_ACP_MANAGER (self));

  g_debug ("Adding FoundryAcpProvider of type `%s`", G_OBJECT_TYPE_NAME (addin));

  if (self->agents_requested)
    dex_future_disown (foundry_acp_provider_load (FOUNDRY_ACP_PROVIDER (addin)));
}

static void
foundry_acp_manager_provider_removed (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      GObject          *addin,
                                      gpointer          user_data)
{
  FoundryAcpManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_ACP_PROVIDER (addin));
  g_assert (FOUNDRY_IS_ACP_MANAGER (self));

  g_debug ("Removing FoundryAcpProvider of type `%s`", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_acp_provider_unload (FOUNDRY_ACP_PROVIDER (addin)));
}

static DexFuture *
foundry_acp_manager_start (FoundryService *service)
{
  FoundryAcpManager *self = (FoundryAcpManager *)service;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_ACP_MANAGER (self));
  g_assert (PEAS_IS_EXTENSION_SET (self->addins));

  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (foundry_acp_manager_provider_added),
                           self,
                           0);
  g_signal_connect_object (self->addins,
                           "extension-removed",
                           G_CALLBACK (foundry_acp_manager_provider_removed),
                           self,
                           0);

  return dex_future_new_true ();
}

static DexFuture *
foundry_acp_manager_load_agents_fiber (gpointer user_data)
{
  FoundryAcpManager *self = user_data;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_ACP_MANAGER (self));
  g_assert (PEAS_IS_EXTENSION_SET (self->addins));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryAcpProvider) provider = NULL;

      provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures, foundry_acp_provider_load (provider));
    }

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static DexFuture *
foundry_acp_manager_ensure_agents_loaded (FoundryAcpManager *self)
{
  g_assert (FOUNDRY_IS_ACP_MANAGER (self));

  self->agents_requested = TRUE;

  if (self->load_agents == NULL)
    self->load_agents =
      dex_scheduler_spawn (NULL, 0,
                           foundry_acp_manager_load_agents_fiber,
                           g_object_ref (self),
                           g_object_unref);

  return dex_ref (self->load_agents);
}

static DexFuture *
foundry_acp_manager_stop (FoundryService *service)
{
  FoundryAcpManager *self = (FoundryAcpManager *)service;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_SERVICE (service));

  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_acp_manager_provider_added),
                                        self);
  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_acp_manager_provider_removed),
                                        self);

  dex_clear (&self->load_agents);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryAcpProvider) provider = NULL;

      provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures, foundry_acp_provider_unload (provider));
    }

  g_clear_object (&self->addins);

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static void
foundry_acp_manager_constructed (GObject *object)
{
  FoundryAcpManager *self = (FoundryAcpManager *)object;
  g_autoptr(FoundryContext) context = NULL;

  G_OBJECT_CLASS (foundry_acp_manager_parent_class)->constructed (object);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  self->addins = peas_extension_set_new (NULL,
                                         FOUNDRY_TYPE_ACP_PROVIDER,
                                         "context", context,
                                         NULL);

  g_object_set (self->flatten,
                "model", self->addins,
                NULL);
}

static void
foundry_acp_manager_finalize (GObject *object)
{
  FoundryAcpManager *self = (FoundryAcpManager *)object;

  dex_clear (&self->load_agents);

  g_clear_object (&self->flatten);
  g_clear_object (&self->addins);

  G_OBJECT_CLASS (foundry_acp_manager_parent_class)->finalize (object);
}

static void
foundry_acp_manager_class_init (FoundryAcpManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  object_class->constructed = foundry_acp_manager_constructed;
  object_class->finalize = foundry_acp_manager_finalize;

  service_class->start = foundry_acp_manager_start;
  service_class->stop = foundry_acp_manager_stop;
}

static void
foundry_acp_manager_init (FoundryAcpManager *self)
{
  self->flatten = foundry_flatten_list_model_new (NULL);

  g_signal_connect_object (self->flatten,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static GType
foundry_acp_manager_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_ACP_AGENT;
}

static guint
foundry_acp_manager_get_n_items (GListModel *model)
{
  return g_list_model_get_n_items (G_LIST_MODEL (FOUNDRY_ACP_MANAGER (model)->flatten));
}

static gpointer
foundry_acp_manager_get_item (GListModel *model,
                              guint       position)
{
  return g_list_model_get_item (G_LIST_MODEL (FOUNDRY_ACP_MANAGER (model)->flatten), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_acp_manager_get_item_type;
  iface->get_n_items = foundry_acp_manager_get_n_items;
  iface->get_item = foundry_acp_manager_get_item;
}

/**
 * foundry_acp_manager_list_providers:
 * @self: a [class@Foundry.AcpManager]
 *
 * Lists loaded ACP providers.
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of [class@Foundry.AcpProvider]
 *
 * Since: 1.2
 */
GListModel *
foundry_acp_manager_list_providers (FoundryAcpManager *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_MANAGER (self), NULL);

  return self->addins ? g_object_ref (G_LIST_MODEL (self->addins)) : NULL;
}

/**
 * foundry_acp_manager_list_agents:
 * @self: a [class@Foundry.AcpManager]
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of [class@Foundry.AcpAgent]
 *
 * Since: 1.2
 */
GListModel *
foundry_acp_manager_list_agents (FoundryAcpManager *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_MANAGER (self), NULL);

  dex_future_disown (foundry_acp_manager_ensure_agents_loaded (self));

  return g_object_ref (G_LIST_MODEL (self->flatten));
}

static DexFuture *
foundry_acp_manager_find_agent_fiber (FoundryAcpManager *self,
                                      const char        *id)
{
  g_autoptr(GError) error = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_ACP_MANAGER (self));
  g_assert (id != NULL);

  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (self)), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!dex_await (foundry_acp_manager_ensure_agents_loaded (self), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryAcpAgent) agent = g_list_model_get_item (G_LIST_MODEL (self), i);
      g_autofree char *agent_id = foundry_acp_agent_dup_id (agent);

      if (g_strcmp0 (agent_id, id) == 0)
        return dex_future_new_take_object (g_steal_pointer (&agent));
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_FOUND,
                                "No such ACP agent `%s`",
                                id);
}

/**
 * foundry_acp_manager_find_agent:
 * @self: a [class@Foundry.AcpManager]
 * @id: the agent identifier
 *
 * Finds an available ACP agent by identifier.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.AcpAgent]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_manager_find_agent (FoundryAcpManager *self,
                                const char        *id)
{
  dex_return_error_if_fail (FOUNDRY_IS_ACP_MANAGER (self));
  dex_return_error_if_fail (id != NULL);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (foundry_acp_manager_find_agent_fiber),
                                  2,
                                  FOUNDRY_TYPE_ACP_MANAGER, self,
                                  G_TYPE_STRING, id);
}
