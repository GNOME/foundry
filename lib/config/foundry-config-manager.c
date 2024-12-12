/* foundry-config-manager.c
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

#include <libpeas.h>

#include "eggflattenlistmodel.h"

#include "foundry-config.h"
#include "foundry-config-manager.h"
#include "foundry-config-provider-private.h"
#include "foundry-contextual-private.h"
#include "foundry-debug.h"
#include "foundry-service-private.h"
#include "foundry-settings.h"
#include "foundry-util-private.h"

struct _FoundryConfigManager
{
  FoundryService       parent_instance;
  EggFlattenListModel *flatten;
  PeasExtensionSet    *addins;
  FoundryConfig       *config;
};

struct _FoundryConfigManagerClass
{
  FoundryServiceClass parent_class;
};

enum {
  PROP_0,
  PROP_CONFIG,
  N_PROPS
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryConfigManager, foundry_config_manager, FOUNDRY_TYPE_SERVICE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties[N_PROPS];

static void
foundry_config_manager_provider_added (PeasExtensionSet *set,
                                       PeasPluginInfo   *plugin_info,
                                       GObject          *addin,
                                       gpointer          user_data)
{
  FoundryConfigManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_CONFIG_PROVIDER (addin));
  g_assert (FOUNDRY_IS_CONFIG_MANAGER (self));

  g_debug ("Adding FoundryConfigProvider of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_config_provider_load (FOUNDRY_CONFIG_PROVIDER (addin)));
}

static void
foundry_config_manager_provider_removed (PeasExtensionSet *set,
                                         PeasPluginInfo   *plugin_info,
                                         GObject          *addin,
                                         gpointer          user_data)
{
  FoundryConfigManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_CONFIG_PROVIDER (addin));
  g_assert (FOUNDRY_IS_CONFIG_MANAGER (self));

  g_debug ("Removing FoundryConfigProvider of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_config_provider_unload (FOUNDRY_CONFIG_PROVIDER (addin)));
}

static DexFuture *
foundry_config_manager_start_fiber (gpointer user_data)
{
  FoundryConfigManager *self = user_data;
  g_autoptr(FoundrySettings) settings  = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryConfig) config = NULL;
  g_autoptr(GPtrArray) futures = NULL;
  g_autofree char *config_id = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_CONFIG_MANAGER (self));
  g_assert (PEAS_IS_EXTENSION_SET (self->addins));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  settings = foundry_context_load_project_settings (context);

  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (foundry_config_manager_provider_added),
                           self,
                           0);
  g_signal_connect_object (self->addins,
                           "extension-removed",
                           G_CALLBACK (foundry_config_manager_provider_removed),
                           self,
                           0);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryConfigProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures,
                       foundry_config_provider_load (provider));
    }

  if (futures->len > 0)
    dex_await (foundry_future_all (futures), NULL);

  config_id = foundry_settings_get_string (settings, "config-id");

  if ((config = foundry_config_manager_find_config (self, config_id)))
    foundry_config_manager_set_config (self, config);

  return dex_future_new_true ();
}

static DexFuture *
foundry_config_manager_start (FoundryService *service)
{
  FoundryConfigManager *self = (FoundryConfigManager *)service;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_CONFIG_MANAGER (self));
  g_assert (PEAS_IS_EXTENSION_SET (self->addins));

  return dex_scheduler_spawn (NULL, 0,
                              foundry_config_manager_start_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static DexFuture *
foundry_config_manager_stop (FoundryService *service)
{
  FoundryConfigManager *self = (FoundryConfigManager *)service;
  g_autoptr(FoundrySettings) settings = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_SERVICE (service));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  settings = foundry_context_load_project_settings (context);

  if (self->config != NULL)
    {
      g_autofree char *id = foundry_config_dup_id (self->config);

      foundry_settings_set_string (settings, "config-id", id);
    }

  g_clear_object (&self->config);

  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_config_manager_provider_added),
                                        self);
  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_config_manager_provider_removed),
                                        self);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryConfigProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures,
                       foundry_config_provider_unload (provider));
    }

  g_clear_object (&self->addins);

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static void
foundry_config_manager_constructed (GObject *object)
{
  FoundryConfigManager *self = (FoundryConfigManager *)object;
  g_autoptr(FoundryContext) context = NULL;

  G_OBJECT_CLASS (foundry_config_manager_parent_class)->constructed (object);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  self->addins = peas_extension_set_new (NULL,
                                         FOUNDRY_TYPE_CONFIG_PROVIDER,
                                         "context", context,
                                         NULL);

  egg_flatten_list_model_set_model (self->flatten, G_LIST_MODEL (self->addins));
}

static void
foundry_config_manager_finalize (GObject *object)
{
  FoundryConfigManager *self = (FoundryConfigManager *)object;

  g_clear_object (&self->flatten);
  g_clear_object (&self->config);
  g_clear_object (&self->addins);

  G_OBJECT_CLASS (foundry_config_manager_parent_class)->finalize (object);
}

static void
foundry_config_manager_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  FoundryConfigManager *self = FOUNDRY_CONFIG_MANAGER (object);

  switch (prop_id)
    {
    case PROP_CONFIG:
      g_value_take_object (value, foundry_config_manager_dup_config (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_config_manager_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  FoundryConfigManager *self = FOUNDRY_CONFIG_MANAGER (object);

  switch (prop_id)
    {
    case PROP_CONFIG:
      foundry_config_manager_set_config (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_config_manager_class_init (FoundryConfigManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  object_class->constructed = foundry_config_manager_constructed;
  object_class->finalize = foundry_config_manager_finalize;
  object_class->get_property = foundry_config_manager_get_property;
  object_class->set_property = foundry_config_manager_set_property;

  service_class->start = foundry_config_manager_start;
  service_class->stop = foundry_config_manager_stop;

  properties[PROP_CONFIG] =
    g_param_spec_object ("config", NULL, NULL,
                         FOUNDRY_TYPE_CONFIG,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_config_manager_init (FoundryConfigManager *self)
{
  self->flatten = egg_flatten_list_model_new (NULL);

  g_signal_connect_object (self->flatten,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static GType
foundry_config_manager_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_CONFIG;
}

static guint
foundry_config_manager_get_n_items (GListModel *model)
{
  return g_list_model_get_n_items (G_LIST_MODEL (FOUNDRY_CONFIG_MANAGER (model)->flatten));
}

static gpointer
foundry_config_manager_get_item (GListModel *model,
                                 guint       position)
{
  return g_list_model_get_item (G_LIST_MODEL (FOUNDRY_CONFIG_MANAGER (model)->flatten), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_config_manager_get_item_type;
  iface->get_n_items = foundry_config_manager_get_n_items;
  iface->get_item = foundry_config_manager_get_item;
}

/**
 * foundry_config_manager_dup_config:
 * @self: a #FoundryConfigManager
 *
 * Gets the active configuration
 *
 * Returns: (transfer full) (nullable): a #FoundryConfig or %NULL
 */
FoundryConfig *
foundry_config_manager_dup_config (FoundryConfigManager *self)
{
  FoundryConfig *ret = NULL;

  g_return_val_if_fail (FOUNDRY_IS_CONFIG_MANAGER (self), NULL);

  g_set_object (&ret, self->config);

  return ret;
}

/**
 * foundry_config_manager_set_config:
 * @self: a #FoundryConfigManager
 *
 * Sets the active configuration for the config manager.
 *
 * Other services such as the build manager or sdk manager may respond
 * to changes of this property and update accordingly.
 */
void
foundry_config_manager_set_config (FoundryConfigManager *self,
                                   FoundryConfig        *config)
{
  g_autoptr(FoundrySettings) settings = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryConfig) old = NULL;
  g_autofree char *config_id = NULL;

  g_return_if_fail (FOUNDRY_IS_CONFIG_MANAGER (self));
  g_return_if_fail (!config || FOUNDRY_IS_CONFIG (config));

  if (self->config == config)
    return;

  if (config != NULL)
    {
      config_id = foundry_config_dup_id (config);
      g_object_ref (config);
    }

  old = g_steal_pointer (&self->config);
  self->config = config;

  if (old != NULL)
    g_object_notify (G_OBJECT (old), "active");

  if (config != NULL)
    g_object_notify (G_OBJECT (config), "active");

  _foundry_contextual_invalidate_pipeline (FOUNDRY_CONTEXTUAL (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  settings = foundry_context_load_project_settings (context);
  foundry_settings_set_string (settings, "config-id", config_id ? config_id : "");

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONFIG]);
}

/**
 * foundry_config_manager_find_config:
 * @self: a #FoundryConfigManager
 * @config_id: an identifier matching a #FoundryConfig:id
 *
 * Looks through available configs to find one matching @config_id.
 *
 * Returns: (transfer full) (nullable): a #FoundryConfig or %NULL
 */
FoundryConfig *
foundry_config_manager_find_config (FoundryConfigManager *self,
                                    const char           *config_id)
{
  guint n_items;

  g_return_val_if_fail (FOUNDRY_IS_CONFIG_MANAGER (self), NULL);
  g_return_val_if_fail (config_id != NULL, NULL);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryConfig) config = g_list_model_get_item (G_LIST_MODEL (self), i);
      g_autofree char *id = foundry_config_dup_id (config);

      if (g_strcmp0 (config_id, id) == 0)
        return g_steal_pointer (&config);
    }

  return NULL;
}
