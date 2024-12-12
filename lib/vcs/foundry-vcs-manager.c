/* foundry-vcs-manager.c
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

#include "foundry-vcs-manager.h"
#include "foundry-search-provider-private.h"
#include "foundry-contextual-private.h"
#include "foundry-debug.h"
#include "foundry-service-private.h"
#include "foundry-settings.h"
#include "foundry-vcs.h"
#include "foundry-util-private.h"

struct _FoundryVcsManager
{
  FoundryService    parent_instance;
  PeasExtensionSet *addins;
  FoundryVcs       *vcs;
};

struct _FoundryVcsManagerClass
{
  FoundryServiceClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryVcsManager, foundry_vcs_manager, FOUNDRY_TYPE_SERVICE)

enum {
  PROP_0,
  PROP_VCS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_vcs_manager_provider_added (PeasExtensionSet *set,
                                    PeasPluginInfo   *plugin_info,
                                    GObject          *addin,
                                    gpointer          user_data)
{
  FoundryVcsManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_SEARCH_PROVIDER (addin));
  g_assert (FOUNDRY_IS_VCS_MANAGER (self));

  g_debug ("Adding FoundrySearchProvider of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_search_provider_load (FOUNDRY_SEARCH_PROVIDER (addin)));
}

static void
foundry_vcs_manager_provider_removed (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      GObject          *addin,
                                      gpointer          user_data)
{
  FoundryVcsManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_SEARCH_PROVIDER (addin));
  g_assert (FOUNDRY_IS_VCS_MANAGER (self));

  g_debug ("Removing FoundrySearchProvider of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_search_provider_unload (FOUNDRY_SEARCH_PROVIDER (addin)));
}

static DexFuture *
foundry_vcs_manager_start (FoundryService *service)
{
  FoundryVcsManager *self = (FoundryVcsManager *)service;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_SERVICE (service));
  g_assert (PEAS_IS_EXTENSION_SET (self->addins));

  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (foundry_vcs_manager_provider_added),
                           self,
                           0);
  g_signal_connect_object (self->addins,
                           "extension-removed",
                           G_CALLBACK (foundry_vcs_manager_provider_removed),
                           self,
                           0);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundrySearchProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures,
                       foundry_search_provider_load (provider));
    }

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static DexFuture *
foundry_vcs_manager_stop (FoundryService *service)
{
  FoundryVcsManager *self = (FoundryVcsManager *)service;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_SERVICE (service));

  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_vcs_manager_provider_added),
                                        self);
  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_vcs_manager_provider_removed),
                                        self);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundrySearchProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures,
                       foundry_search_provider_unload (provider));
    }

  g_clear_object (&self->addins);

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static void
foundry_vcs_manager_constructed (GObject *object)
{
  FoundryVcsManager *self = (FoundryVcsManager *)object;
  g_autoptr(FoundryContext) context = NULL;

  G_OBJECT_CLASS (foundry_vcs_manager_parent_class)->constructed (object);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  self->addins = peas_extension_set_new (NULL,
                                         FOUNDRY_TYPE_SEARCH_PROVIDER,
                                         "context", context,
                                         NULL);
}

static void
foundry_vcs_manager_dispose (GObject *object)
{
  FoundryVcsManager *self = (FoundryVcsManager *)object;

  g_clear_object (&self->vcs);
  g_clear_object (&self->addins);

  G_OBJECT_CLASS (foundry_vcs_manager_parent_class)->dispose (object);
}

static void
foundry_vcs_manager_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundryVcsManager *self = FOUNDRY_VCS_MANAGER (object);

  switch (prop_id)
    {
    case PROP_VCS:
      g_value_take_object (value, foundry_vcs_manager_dup_vcs (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_manager_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  FoundryVcsManager *self = FOUNDRY_VCS_MANAGER (object);

  switch (prop_id)
    {
    case PROP_VCS:
      foundry_vcs_manager_set_vcs (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_manager_class_init (FoundryVcsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  object_class->constructed = foundry_vcs_manager_constructed;
  object_class->dispose = foundry_vcs_manager_dispose;
  object_class->get_property = foundry_vcs_manager_get_property;
  object_class->set_property = foundry_vcs_manager_set_property;

  service_class->start = foundry_vcs_manager_start;
  service_class->stop = foundry_vcs_manager_stop;

  properties[PROP_VCS] =
    g_param_spec_object ("vcs", NULL, NULL,
                         FOUNDRY_TYPE_VCS,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_vcs_manager_init (FoundryVcsManager *self)
{
}

/**
 * foundry_vcs_manager_dup_vcs:
 * @self: a #FoundryVcsManager
 *
 * Get the active [class@Foundry.Vcs].
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.Vcs]
 */
FoundryVcs *
foundry_vcs_manager_dup_vcs (FoundryVcsManager *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_MANAGER (self), NULL);

  return self->vcs ? g_object_ref (self->vcs) : NULL;
}

void
foundry_vcs_manager_set_vcs (FoundryVcsManager *self,
                             FoundryVcs        *vcs)
{
  g_autoptr(FoundrySettings) settings = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryVcs) old = NULL;
  g_autofree char *vcs_id = NULL;

  g_return_if_fail (FOUNDRY_IS_VCS_MANAGER (self));
  g_return_if_fail (!vcs || FOUNDRY_IS_VCS (vcs));

  if (self->vcs == vcs)
    return;

  if (vcs != NULL)
    {
      vcs_id = foundry_vcs_dup_id (vcs);
      g_object_ref (vcs);
    }

  old = g_steal_pointer (&self->vcs);
  self->vcs = vcs;

  if (old != NULL)
    g_object_notify (G_OBJECT (old), "active");

  if (vcs != NULL)
    g_object_notify (G_OBJECT (vcs), "active");

  _foundry_contextual_invalidate_pipeline (FOUNDRY_CONTEXTUAL (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  settings = foundry_context_load_project_settings (context);
  foundry_settings_set_string (settings, "vcs-id", vcs_id ? vcs_id : "");

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VCS]);
}
