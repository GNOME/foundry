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
  GListStore *servers;
} FoundryLspProviderPrivate;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (FoundryLspProvider, foundry_lsp_provider, FOUNDRY_TYPE_CONTEXTUAL,
                                  G_ADD_PRIVATE (FoundryLspProvider)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

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

  G_OBJECT_CLASS (foundry_lsp_provider_parent_class)->finalize (object);
}

static void
foundry_lsp_provider_class_init (FoundryLspProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_lsp_provider_finalize;

  klass->load = foundry_lsp_provider_real_load;
  klass->unload = foundry_lsp_provider_real_unload;
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
  g_return_if_fail (FOUNDRY_IS_LSP_PROVIDER (self));
  g_return_if_fail (FOUNDRY_IS_LSP_SERVER (server));

}

void
foundry_lsp_provider_remove (FoundryLspProvider *self,
                             FoundryLspServer   *server)
{
  g_return_if_fail (FOUNDRY_IS_LSP_PROVIDER (self));
  g_return_if_fail (FOUNDRY_IS_LSP_SERVER (server));

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
