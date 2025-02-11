/* foundry-lsp-manager.c
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

#include "eggflattenlistmodel.h"

#include <libpeas.h>

#include "foundry-lsp-manager.h"
#include "foundry-lsp-provider-private.h"
#include "foundry-lsp-server.h"
#include "foundry-contextual-private.h"
#include "foundry-debug.h"
#include "foundry-service-private.h"
#include "foundry-util-private.h"

struct _FoundryLspManager
{
  FoundryService       parent_instance;
  PeasExtensionSet    *addins;
  EggFlattenListModel *flatten;
};

struct _FoundryLspManagerClass
{
  FoundryServiceClass parent_class;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryLspManager, foundry_lsp_manager, FOUNDRY_TYPE_SERVICE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
foundry_lsp_manager_provider_added (PeasExtensionSet *set,
                                    PeasPluginInfo   *plugin_info,
                                    GObject          *addin,
                                    gpointer          user_data)
{
  FoundryLspManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_LSP_PROVIDER (addin));
  g_assert (FOUNDRY_IS_LSP_MANAGER (self));

  g_debug ("Adding FoundryLspProvider of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_lsp_provider_load (FOUNDRY_LSP_PROVIDER (addin)));
}

static void
foundry_lsp_manager_provider_removed (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      GObject          *addin,
                                      gpointer          user_data)
{
  FoundryLspManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_LSP_PROVIDER (addin));
  g_assert (FOUNDRY_IS_LSP_MANAGER (self));

  g_debug ("Removing FoundryLspProvider of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_lsp_provider_unload (FOUNDRY_LSP_PROVIDER (addin)));
}

static DexFuture *
foundry_lsp_manager_start (FoundryService *service)
{
  FoundryLspManager *self = (FoundryLspManager *)service;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_SERVICE (service));
  g_assert (PEAS_IS_EXTENSION_SET (self->addins));

  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (foundry_lsp_manager_provider_added),
                           self,
                           0);
  g_signal_connect_object (self->addins,
                           "extension-removed",
                           G_CALLBACK (foundry_lsp_manager_provider_removed),
                           self,
                           0);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryLspProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures,
                       foundry_lsp_provider_load (provider));
    }

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static DexFuture *
foundry_lsp_manager_stop (FoundryService *service)
{
  FoundryLspManager *self = (FoundryLspManager *)service;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_SERVICE (service));

  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_lsp_manager_provider_added),
                                        self);
  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_lsp_manager_provider_removed),
                                        self);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryLspProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures,
                       foundry_lsp_provider_unload (provider));
    }

  g_clear_object (&self->addins);

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static void
foundry_lsp_manager_constructed (GObject *object)
{
  FoundryLspManager *self = (FoundryLspManager *)object;
  g_autoptr(FoundryContext) context = NULL;

  G_OBJECT_CLASS (foundry_lsp_manager_parent_class)->constructed (object);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  self->addins = peas_extension_set_new (NULL,
                                         FOUNDRY_TYPE_LSP_PROVIDER,
                                         "context", context,
                                         NULL);

  egg_flatten_list_model_set_model (self->flatten,
                                    G_LIST_MODEL (self->addins));
}

static void
foundry_lsp_manager_finalize (GObject *object)
{
  FoundryLspManager *self = (FoundryLspManager *)object;

  g_clear_object (&self->flatten);
  g_clear_object (&self->addins);

  G_OBJECT_CLASS (foundry_lsp_manager_parent_class)->finalize (object);
}

static void
foundry_lsp_manager_class_init (FoundryLspManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  object_class->constructed = foundry_lsp_manager_constructed;
  object_class->finalize = foundry_lsp_manager_finalize;

  service_class->start = foundry_lsp_manager_start;
  service_class->stop = foundry_lsp_manager_stop;
}

static void
foundry_lsp_manager_init (FoundryLspManager *self)
{
  self->flatten = egg_flatten_list_model_new (NULL);

  g_signal_connect_object (self->flatten,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

/**
 * foundry_lsp_manager_load_client:
 * @self: a #FoundryLspManager
 *
 * Loads a [class@Foundry.LspClient] for the @language_id.
 *
 * If an existing client is already created for this language,
 * that client will be returned.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.LspClient].
 */
DexFuture *
foundry_lsp_manager_load_client (FoundryLspManager *self,
                                 const char        *language_id)
{
  dex_return_error_if_fail (FOUNDRY_IS_LSP_MANAGER (self));

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "not supported");
}

static GType
foundry_lsp_manager_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_LSP_SERVER;
}

static guint
foundry_lsp_manager_get_n_items (GListModel *model)
{
  FoundryLspManager *self = FOUNDRY_LSP_MANAGER (model);

  return g_list_model_get_n_items (G_LIST_MODEL (self->flatten));
}

static gpointer
foundry_lsp_manager_get_item (GListModel *model,
                              guint       position)
{
  FoundryLspManager *self = FOUNDRY_LSP_MANAGER (model);

  return g_list_model_get_item (G_LIST_MODEL (self->flatten), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_lsp_manager_get_item_type;
  iface->get_n_items = foundry_lsp_manager_get_n_items;
  iface->get_item = foundry_lsp_manager_get_item;
}
