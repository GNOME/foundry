/* foundry-llm-tool-provider.c
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

#include "foundry-llm-tool.h"
#include "foundry-llm-tool-provider-private.h"

typedef struct
{
  PeasPluginInfo *plugin_info;
  GListStore     *tools;
} FoundryLlmToolProviderPrivate;

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPS
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (FoundryLlmToolProvider, foundry_llm_tool_provider, FOUNDRY_TYPE_CONTEXTUAL,
                                  G_ADD_PRIVATE (FoundryLlmToolProvider)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties[N_PROPS];

static void
foundry_llm_tool_provider_finalize (GObject *object)
{
  FoundryLlmToolProvider *self = (FoundryLlmToolProvider *)object;
  FoundryLlmToolProviderPrivate *priv = foundry_llm_tool_provider_get_instance_private (self);

  g_clear_object (&priv->tools);
  g_clear_object (&priv->plugin_info);

  G_OBJECT_CLASS (foundry_llm_tool_provider_parent_class)->finalize (object);
}

static void
foundry_llm_tool_provider_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  FoundryLlmToolProvider *self = FOUNDRY_LLM_TOOL_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_take_object (value, foundry_llm_tool_provider_dup_plugin_info (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_llm_tool_provider_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  FoundryLlmToolProvider *self = FOUNDRY_LLM_TOOL_PROVIDER (object);
  FoundryLlmToolProviderPrivate *priv = foundry_llm_tool_provider_get_instance_private (self);

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
foundry_llm_tool_provider_class_init (FoundryLlmToolProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_llm_tool_provider_finalize;
  object_class->get_property = foundry_llm_tool_provider_get_property;
  object_class->set_property = foundry_llm_tool_provider_set_property;

  properties[PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_llm_tool_provider_init (FoundryLlmToolProvider *self)
{
  FoundryLlmToolProviderPrivate *priv = foundry_llm_tool_provider_get_instance_private (self);

  priv->tools = g_list_store_new (FOUNDRY_TYPE_LLM_TOOL);

  g_signal_connect_object (priv->tools,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

/**
 * foundry_llm_tool_provider_dup_plugin_info:
 * @self: a [class@Foundry.LlmToolProvider]
 *
 * Returns: (transfer full) (nullable):
 */
PeasPluginInfo *
foundry_llm_tool_provider_dup_plugin_info (FoundryLlmToolProvider *self)
{
  FoundryLlmToolProviderPrivate *priv = foundry_llm_tool_provider_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_LLM_TOOL_PROVIDER (self), NULL);

  return priv->plugin_info ? g_object_ref (priv->plugin_info) : NULL;
}

void
foundry_llm_tool_provider_add_tool (FoundryLlmToolProvider *self,
                                    FoundryLlmTool         *tool)
{
  FoundryLlmToolProviderPrivate *priv = foundry_llm_tool_provider_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_LLM_TOOL_PROVIDER (self));
  g_return_if_fail (FOUNDRY_IS_LLM_TOOL (tool));

  g_list_store_append (priv->tools, tool);
}

void
foundry_llm_tool_provider_remove_tool (FoundryLlmToolProvider *self,
                                       FoundryLlmTool         *tool)
{
  FoundryLlmToolProviderPrivate *priv = foundry_llm_tool_provider_get_instance_private (self);
  guint n_tools;

  g_return_if_fail (FOUNDRY_IS_LLM_TOOL_PROVIDER (self));
  g_return_if_fail (FOUNDRY_IS_LLM_TOOL (tool));

  n_tools = g_list_model_get_n_items (G_LIST_MODEL (priv->tools));

  for (guint i = 0; i < n_tools; i++)
    {
      g_autoptr(FoundryLlmTool) item = g_list_model_get_item (G_LIST_MODEL (priv->tools), i);

      if (item == tool)
        {
          g_list_store_remove (priv->tools, i);
          break;
        }
    }
}

static GType
foundry_llm_tool_provider_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_LLM_TOOL;
}

static guint
foundry_llm_tool_provider_get_n_items (GListModel *model)
{
  FoundryLlmToolProvider *self = FOUNDRY_LLM_TOOL_PROVIDER (model);
  FoundryLlmToolProviderPrivate *priv = foundry_llm_tool_provider_get_instance_private (self);

  return g_list_model_get_n_items (G_LIST_MODEL (priv->tools));
}

static gpointer
foundry_llm_tool_provider_get_item (GListModel *model,
                                    guint       position)
{
  FoundryLlmToolProvider *self = FOUNDRY_LLM_TOOL_PROVIDER (model);
  FoundryLlmToolProviderPrivate *priv = foundry_llm_tool_provider_get_instance_private (self);

  return g_list_model_get_item (G_LIST_MODEL (priv->tools), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_llm_tool_provider_get_item_type;
  iface->get_n_items = foundry_llm_tool_provider_get_n_items;
  iface->get_item = foundry_llm_tool_provider_get_item;
}
