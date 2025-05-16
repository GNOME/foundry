/* foundry-completion-manager.c
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

#include "foundry-completion-manager-private.h"
#include "foundry-completion-provider-private.h"
#include "foundry-util.h"

struct _FoundryCompletionManager
{
  FoundryContextual  parent_instance;
  GWeakRef           document_wr;
  PeasExtensionSet  *providers;
};

static GType
foundry_completion_manager_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_COMPLETION_PROVIDER;
}

static guint
foundry_completion_manager_get_n_items (GListModel *model)
{
  FoundryCompletionManager *self = FOUNDRY_COMPLETION_MANAGER (model);

  if (self->providers != NULL)
    return g_list_model_get_n_items (G_LIST_MODEL (self->providers));

  return 0;
}

static gpointer
foundry_completion_manager_get_item (GListModel *model,
                                     guint       position)
{
  FoundryCompletionManager *self = FOUNDRY_COMPLETION_MANAGER (model);

  if (self->providers != NULL)
    return g_list_model_get_item (G_LIST_MODEL (self->providers), position);

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_completion_manager_get_item_type;
  iface->get_n_items = foundry_completion_manager_get_n_items;
  iface->get_item = foundry_completion_manager_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryCompletionManager, foundry_completion_manager, FOUNDRY_TYPE_CONTEXTUAL,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_DOCUMENT,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_completion_manager_addin_added_cb (PeasExtensionSet *set,
                                           PeasPluginInfo   *plugin_info,
                                           GObject          *extension,
                                           gpointer          user_data)
{
  FoundryCompletionProvider *provider = (FoundryCompletionProvider *)extension;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_COMPLETION_MANAGER (user_data));
  g_assert (FOUNDRY_IS_COMPLETION_PROVIDER (provider));

  dex_future_disown (_foundry_completion_provider_load (provider));
}

static void
foundry_completion_manager_addin_removed_cb (PeasExtensionSet *set,
                                             PeasPluginInfo   *plugin_info,
                                             GObject          *extension,
                                             gpointer          user_data)
{
  FoundryCompletionProvider *provider = (FoundryCompletionProvider *)extension;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_COMPLETION_MANAGER (user_data));
  g_assert (FOUNDRY_IS_COMPLETION_PROVIDER (provider));

  dex_future_disown (_foundry_completion_provider_unload (provider));
}

static void
foundry_completion_manager_dispose (GObject *object)
{
  FoundryCompletionManager *self = (FoundryCompletionManager *)object;

  g_clear_object (&self->providers);

  G_OBJECT_CLASS (foundry_completion_manager_parent_class)->dispose (object);
}

static void
foundry_completion_manager_finalize (GObject *object)
{
  FoundryCompletionManager *self = (FoundryCompletionManager *)object;

  g_weak_ref_clear (&self->document_wr);

  G_OBJECT_CLASS (foundry_completion_manager_parent_class)->finalize (object);
}

static void
foundry_completion_manager_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  FoundryCompletionManager *self = FOUNDRY_COMPLETION_MANAGER (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      g_value_take_object (value, g_weak_ref_get (&self->document_wr));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_completion_manager_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  FoundryCompletionManager *self = FOUNDRY_COMPLETION_MANAGER (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      g_weak_ref_set (&self->document_wr, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_completion_manager_class_init (FoundryCompletionManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_completion_manager_dispose;
  object_class->finalize = foundry_completion_manager_finalize;
  object_class->get_property = foundry_completion_manager_get_property;
  object_class->set_property = foundry_completion_manager_set_property;

  properties[PROP_DOCUMENT] =
    g_param_spec_object ("document", NULL, NULL,
                         FOUNDRY_TYPE_TEXT_DOCUMENT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_completion_manager_init (FoundryCompletionManager *self)
{
  g_weak_ref_init (&self->document_wr, NULL);
}

static DexFuture *
foundry_completion_manager_new_fiber (gpointer data)
{
  FoundryTextDocument *document = data;
  g_autoptr(FoundryCompletionManager) self = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  dex_return_error_if_fail (FOUNDRY_IS_TEXT_DOCUMENT (document));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  dex_return_error_if_fail (FOUNDRY_IS_CONTEXT (context));

  self = g_object_new (FOUNDRY_TYPE_COMPLETION_MANAGER,
                       "context", context,
                       "document", document,
                       NULL);

  self->providers = peas_extension_set_new (peas_engine_get_default (),
                                            FOUNDRY_TYPE_COMPLETION_PROVIDER,
                                            "context", context,
                                            "document", document,
                                            NULL);

  g_signal_connect (self->providers,
                    "extension-added",
                    G_CALLBACK (foundry_completion_manager_addin_added_cb),
                    self);
  g_signal_connect (self->providers,
                    "extension-removed",
                    G_CALLBACK (foundry_completion_manager_addin_removed_cb),
                    self);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->providers));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryCompletionProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->providers), i);

      g_ptr_array_add (futures, _foundry_completion_provider_load (provider));
    }

  if (futures->len > 0)
    dex_await (foundry_future_all (futures), NULL);

  return dex_future_new_take_object (g_steal_pointer (&self));
}

DexFuture *
foundry_completion_manager_new (FoundryTextDocument *document)
{
  g_return_val_if_fail (FOUNDRY_IS_TEXT_DOCUMENT (document), NULL);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_completion_manager_new_fiber,
                              g_object_ref (document),
                              g_object_unref);
}
