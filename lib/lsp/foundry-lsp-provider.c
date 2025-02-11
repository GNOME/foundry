/* foundry-lsp-provider.c
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

#include <glib/gi18n.h>

#include "foundry-config-private.h"
#include "foundry-lsp-provider-private.h"
#include "foundry-lsp-server.h"

typedef struct
{
  GListStore     *servers;
  PeasPluginInfo *plugin_info;
} FoundryLspProviderPrivate;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (FoundryLspProvider, foundry_lsp_provider, FOUNDRY_TYPE_CONTEXTUAL,
                                  G_ADD_PRIVATE (FoundryLspProvider)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_lsp_provider_real_load (FoundryLspProvider *self)
{
  return dex_future_new_true ();
}

static DexFuture *
foundry_lsp_provider_real_unload (FoundryLspProvider *self)
{
  return dex_future_new_true ();
}

static void
foundry_lsp_provider_finalize (GObject *object)
{
  FoundryLspProvider *self = (FoundryLspProvider *)object;
  FoundryLspProviderPrivate *priv = foundry_lsp_provider_get_instance_private (self);

  g_clear_object (&priv->servers);
  g_clear_object (&priv->plugin_info);

  G_OBJECT_CLASS (foundry_lsp_provider_parent_class)->finalize (object);
}

static void
foundry_lsp_provider_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  FoundryLspProvider *self = FOUNDRY_LSP_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_take_object (value, foundry_lsp_provider_dup_plugin_info (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_lsp_provider_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  FoundryLspProvider *self = FOUNDRY_LSP_PROVIDER (object);
  FoundryLspProviderPrivate *priv = foundry_lsp_provider_get_instance_private (self);

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
foundry_lsp_provider_class_init (FoundryLspProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_lsp_provider_finalize;
  object_class->get_property = foundry_lsp_provider_get_property;
  object_class->set_property = foundry_lsp_provider_set_property;

  klass->load = foundry_lsp_provider_real_load;
  klass->unload = foundry_lsp_provider_real_unload;

  properties[PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_lsp_provider_init (FoundryLspProvider *self)
{
  FoundryLspProviderPrivate *priv = foundry_lsp_provider_get_instance_private (self);

  priv->servers = g_list_store_new (FOUNDRY_TYPE_LSP_SERVER);

  g_signal_connect_object (priv->servers,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

/**
 * foundry_lsp_provider_load:
 * @self: a #FoundryLspProvider
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_lsp_provider_load (FoundryLspProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LSP_PROVIDER (self), NULL);

  return FOUNDRY_LSP_PROVIDER_GET_CLASS (self)->load (self);
}

/**
 * foundry_lsp_provider_unload:
 * @self: a #FoundryLspProvider
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_lsp_provider_unload (FoundryLspProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LSP_PROVIDER (self), NULL);

  return FOUNDRY_LSP_PROVIDER_GET_CLASS (self)->unload (self);
}

void
foundry_lsp_provider_add (FoundryLspProvider *self,
                          FoundryLspServer   *server)
{
  FoundryLspProviderPrivate *priv = foundry_lsp_provider_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_LSP_PROVIDER (self));
  g_return_if_fail (FOUNDRY_IS_LSP_SERVER (server));

  g_list_store_append (priv->servers, server);
}

void
foundry_lsp_provider_remove (FoundryLspProvider *self,
                             FoundryLspServer   *server)
{
  FoundryLspProviderPrivate *priv = foundry_lsp_provider_get_instance_private (self);
  guint n_items;

  g_return_if_fail (FOUNDRY_IS_LSP_PROVIDER (self));
  g_return_if_fail (FOUNDRY_IS_LSP_SERVER (server));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (priv->servers));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryLspServer) element = g_list_model_get_item (G_LIST_MODEL (priv->servers), i);

      if (element == server)
        {
          g_list_store_remove (priv->servers, i);
          break;
        }
    }
}

static GType
foundry_lsp_provider_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_LSP_SERVER;
}

static guint
foundry_lsp_provider_get_n_items (GListModel *model)
{
  FoundryLspProvider *self = FOUNDRY_LSP_PROVIDER (model);
  FoundryLspProviderPrivate *priv = foundry_lsp_provider_get_instance_private (self);

  return g_list_model_get_n_items (G_LIST_MODEL (priv->servers));
}

static gpointer
foundry_lsp_provider_get_item (GListModel *model,
                               guint       position)
{
  FoundryLspProvider *self = FOUNDRY_LSP_PROVIDER (model);
  FoundryLspProviderPrivate *priv = foundry_lsp_provider_get_instance_private (self);

  return g_list_model_get_item (G_LIST_MODEL (priv->servers), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_lsp_provider_get_item_type;
  iface->get_item = foundry_lsp_provider_get_item;
  iface->get_n_items = foundry_lsp_provider_get_n_items;
}

/**
 * foundry_lsp_provider_dup_plugin_info:
 * @self: a [class@Foundry.LspProvider]
 *
 * Returns: (transfer full): a [class@Peas.PluginInfo]
 */
PeasPluginInfo *
foundry_lsp_provider_dup_plugin_info (FoundryLspProvider *self)
{
  FoundryLspProviderPrivate *priv = foundry_lsp_provider_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_LSP_PROVIDER (self), NULL);

  return priv->plugin_info ? g_object_ref (priv->plugin_info) : NULL;
}
