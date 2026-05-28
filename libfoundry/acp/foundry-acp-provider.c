/* foundry-acp-provider.c
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

#include "foundry-acp-agent.h"
#include "foundry-acp-client.h"
#include "foundry-acp-connection.h"
#include "foundry-acp-provider-private.h"
#include "foundry-build-pipeline.h"
#include "foundry-context.h"
#include "foundry-util.h"

typedef struct
{
  PeasPluginInfo *plugin_info;
  GPtrArray *agents;
} FoundryAcpProviderPrivate;

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPS
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (FoundryAcpProvider, foundry_acp_provider, FOUNDRY_TYPE_CONTEXTUAL,
                                  G_ADD_PRIVATE (FoundryAcpProvider)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_acp_provider_real_load (FoundryAcpProvider *self)
{
  return dex_future_new_true ();
}

static DexFuture *
foundry_acp_provider_real_unload (FoundryAcpProvider *self)
{
  FoundryAcpProviderPrivate *priv = foundry_acp_provider_get_instance_private (self);
  guint n_items;

  g_assert (FOUNDRY_IS_ACP_PROVIDER (self));

  n_items = priv->agents->len;

  if (n_items > 0)
    {
      g_ptr_array_remove_range (priv->agents, 0, n_items);
      g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, 0);
    }

  return dex_future_new_true ();
}

static void
foundry_acp_provider_dispose (GObject *object)
{
  FoundryAcpProvider *self = (FoundryAcpProvider *)object;
  FoundryAcpProviderPrivate *priv = foundry_acp_provider_get_instance_private (self);
  guint n_items;

  n_items = priv->agents->len;

  if (n_items > 0)
    {
      g_ptr_array_remove_range (priv->agents, 0, n_items);
      g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, 0);
    }

  g_clear_object (&priv->plugin_info);

  G_OBJECT_CLASS (foundry_acp_provider_parent_class)->dispose (object);
}

static void
foundry_acp_provider_finalize (GObject *object)
{
  FoundryAcpProvider *self = (FoundryAcpProvider *)object;
  FoundryAcpProviderPrivate *priv = foundry_acp_provider_get_instance_private (self);

  g_clear_pointer (&priv->agents, g_ptr_array_unref);

  G_OBJECT_CLASS (foundry_acp_provider_parent_class)->finalize (object);
}

static void
foundry_acp_provider_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  FoundryAcpProvider *self = FOUNDRY_ACP_PROVIDER (object);
  FoundryAcpProviderPrivate *priv = foundry_acp_provider_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_set_object (value, priv->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_acp_provider_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  FoundryAcpProvider *self = FOUNDRY_ACP_PROVIDER (object);
  FoundryAcpProviderPrivate *priv = foundry_acp_provider_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_acp_provider_class_init (FoundryAcpProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_acp_provider_dispose;
  object_class->finalize = foundry_acp_provider_finalize;
  object_class->get_property = foundry_acp_provider_get_property;
  object_class->set_property = foundry_acp_provider_set_property;

  klass->load = foundry_acp_provider_real_load;
  klass->unload = foundry_acp_provider_real_unload;

  properties[PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_acp_provider_init (FoundryAcpProvider *self)
{
  FoundryAcpProviderPrivate *priv = foundry_acp_provider_get_instance_private (self);

  priv->agents = g_ptr_array_new_with_free_func (g_object_unref);
}

static GType
foundry_acp_provider_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_ACP_AGENT;
}

static guint
foundry_acp_provider_get_n_items (GListModel *model)
{
  FoundryAcpProvider *self = FOUNDRY_ACP_PROVIDER (model);
  FoundryAcpProviderPrivate *priv = foundry_acp_provider_get_instance_private (self);

  return priv->agents->len;
}

static gpointer
foundry_acp_provider_get_item (GListModel *model,
                               guint       position)
{
  FoundryAcpProvider *self = FOUNDRY_ACP_PROVIDER (model);
  FoundryAcpProviderPrivate *priv = foundry_acp_provider_get_instance_private (self);

  if (position < priv->agents->len)
    return g_object_ref (g_ptr_array_index (priv->agents, position));

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_acp_provider_get_item_type;
  iface->get_n_items = foundry_acp_provider_get_n_items;
  iface->get_item = foundry_acp_provider_get_item;
}

/**
 * foundry_acp_provider_dup_plugin_info:
 * @self: a [class@Foundry.AcpProvider]
 *
 * Gets the plugin information for the provider.
 *
 * Returns: (transfer full) (nullable): a [class@Peas.PluginInfo]
 *
 * Since: 1.2
 */
PeasPluginInfo *
foundry_acp_provider_dup_plugin_info (FoundryAcpProvider *self)
{
  FoundryAcpProviderPrivate *priv = foundry_acp_provider_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_ACP_PROVIDER (self), NULL);

  return priv->plugin_info ? g_object_ref (priv->plugin_info) : NULL;
}

DexFuture *
foundry_acp_provider_load (FoundryAcpProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PROVIDER (self), NULL);

  return FOUNDRY_ACP_PROVIDER_GET_CLASS (self)->load (self);
}

DexFuture *
foundry_acp_provider_unload (FoundryAcpProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PROVIDER (self), NULL);

  return FOUNDRY_ACP_PROVIDER_GET_CLASS (self)->unload (self);
}

/**
 * foundry_acp_provider_dup_name:
 * @self: a [class@Foundry.AcpProvider]
 *
 * Gets a user-visible name for the provider.
 *
 * Returns: (transfer full): the provider name
 *
 * Since: 1.2
 */
char *
foundry_acp_provider_dup_name (FoundryAcpProvider *self)
{
  char *ret = NULL;

  g_return_val_if_fail (FOUNDRY_IS_ACP_PROVIDER (self), NULL);

  if (FOUNDRY_ACP_PROVIDER_GET_CLASS (self)->dup_name)
    ret = FOUNDRY_ACP_PROVIDER_GET_CLASS (self)->dup_name (self);

  if (ret == NULL)
    ret = g_strdup (G_OBJECT_TYPE_NAME (self));

  return g_steal_pointer (&ret);
}

/**
 * foundry_acp_provider_agent_added:
 * @self: a [class@Foundry.AcpProvider]
 * @agent: a [class@Foundry.AcpAgent]
 *
 * Adds @agent to the provider.
 *
 * Since: 1.2
 */
void
foundry_acp_provider_agent_added (FoundryAcpProvider *self,
                                  FoundryAcpAgent    *agent)
{
  FoundryAcpProviderPrivate *priv = foundry_acp_provider_get_instance_private (self);
  guint position;

  g_return_if_fail (FOUNDRY_IS_ACP_PROVIDER (self));
  g_return_if_fail (FOUNDRY_IS_ACP_AGENT (agent));

  position = priv->agents->len;

  g_ptr_array_add (priv->agents, g_object_ref (agent));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

/**
 * foundry_acp_provider_agent_removed:
 * @self: a [class@Foundry.AcpProvider]
 * @agent: a [class@Foundry.AcpAgent]
 *
 * Removes @agent from the provider.
 *
 * Since: 1.2
 */
void
foundry_acp_provider_agent_removed (FoundryAcpProvider *self,
                                    FoundryAcpAgent    *agent)
{
  FoundryAcpProviderPrivate *priv = foundry_acp_provider_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_ACP_PROVIDER (self));
  g_return_if_fail (FOUNDRY_IS_ACP_AGENT (agent));

  for (guint i = 0; i < priv->agents->len; i++)
    {
      FoundryAcpAgent *element = g_ptr_array_index (priv->agents, i);

      if (element == agent)
        {
          g_ptr_array_remove_index (priv->agents, i);
          g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
          return;
        }
    }

  g_critical ("%s did not contain agent %s at %p",
              G_OBJECT_TYPE_NAME (self),
              G_OBJECT_TYPE_NAME (agent),
              agent);
}
