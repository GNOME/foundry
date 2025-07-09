/* foundry-llm-provider.c
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

#include <glib/gi18n-lib.h>

#include "foundry-llm-model.h"
#include "foundry-llm-provider-private.h"

typedef struct
{
  GListStore *store;
} FoundryLlmProviderPrivate;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (FoundryLlmProvider, foundry_llm_provider, FOUNDRY_TYPE_CONTEXTUAL,
                                  G_ADD_PRIVATE (FoundryLlmProvider)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static DexFuture *
foundry_llm_provider_real_load (FoundryLlmProvider *self)
{
  return dex_future_new_true ();
}

static DexFuture *
foundry_llm_provider_real_unload (FoundryLlmProvider *self)
{
  FoundryLlmProviderPrivate *priv = foundry_llm_provider_get_instance_private (self);

  g_assert (FOUNDRY_IS_LLM_PROVIDER (self));

  g_list_store_remove_all (priv->store);

  return dex_future_new_true ();
}

static void
foundry_llm_provider_finalize (GObject *object)
{
  FoundryLlmProvider *self = (FoundryLlmProvider *)object;
  FoundryLlmProviderPrivate *priv = foundry_llm_provider_get_instance_private (self);

  g_clear_object (&priv->store);

  G_OBJECT_CLASS (foundry_llm_provider_parent_class)->finalize (object);
}

static void
foundry_llm_provider_class_init (FoundryLlmProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_llm_provider_finalize;

  klass->load = foundry_llm_provider_real_load;
  klass->unload = foundry_llm_provider_real_unload;
}

static void
foundry_llm_provider_init (FoundryLlmProvider *self)
{
  FoundryLlmProviderPrivate *priv = foundry_llm_provider_get_instance_private (self);

  priv->store = g_list_store_new (G_TYPE_OBJECT);
}

/**
 * foundry_llm_provider_load:
 * @self: a #FoundryLlmProvider
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_llm_provider_load (FoundryLlmProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LLM_PROVIDER (self), NULL);

  return FOUNDRY_LLM_PROVIDER_GET_CLASS (self)->load (self);
}

/**
 * foundry_llm_provider_unload:
 * @self: a #FoundryLlmProvider
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_llm_provider_unload (FoundryLlmProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LLM_PROVIDER (self), NULL);

  return FOUNDRY_LLM_PROVIDER_GET_CLASS (self)->unload (self);
}

/**
 * foundry_llm_provider_dup_name:
 * @self: a #FoundryLlmProvider
 *
 * Gets a name for the provider that is expected to be displayed to
 * users such as "Ollama".
 *
 * Returns: (transfer full): the name of the provider
 */
char *
foundry_llm_provider_dup_name (FoundryLlmProvider *self)
{
  char *ret = NULL;

  g_return_val_if_fail (FOUNDRY_IS_LLM_PROVIDER (self), NULL);

  if (FOUNDRY_LLM_PROVIDER_GET_CLASS (self)->dup_name)
    ret = FOUNDRY_LLM_PROVIDER_GET_CLASS (self)->dup_name (self);

  if (ret == NULL)
    ret = g_strdup (G_OBJECT_TYPE_NAME (self));

  return g_steal_pointer (&ret);
}

static GType
foundry_llm_provider_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_LLM_MODEL;
}

static guint
foundry_llm_provider_get_n_items (GListModel *model)
{
  FoundryLlmProvider *self = FOUNDRY_LLM_PROVIDER (model);
  FoundryLlmProviderPrivate *priv = foundry_llm_provider_get_instance_private (self);

  return g_list_model_get_n_items (G_LIST_MODEL (priv->store));
}

static gpointer
foundry_llm_provider_get_item (GListModel *model,
                               guint       position)
{
  FoundryLlmProvider *self = FOUNDRY_LLM_PROVIDER (model);
  FoundryLlmProviderPrivate *priv = foundry_llm_provider_get_instance_private (self);

  return g_list_model_get_item (G_LIST_MODEL (priv->store), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_llm_provider_get_item_type;
  iface->get_n_items = foundry_llm_provider_get_n_items;
  iface->get_item = foundry_llm_provider_get_item;
}
