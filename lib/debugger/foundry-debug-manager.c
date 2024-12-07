/* foundry-debug-manager.c
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

#include "foundry-contextual-private.h"
#include "foundry-debug.h"
#include "foundry-debugger-private.h"
#include "foundry-debug-manager.h"
#include "foundry-service-private.h"
#include "foundry-util-private.h"

struct _FoundryDebugManager
{
  FoundryService    parent_instance;
  PeasExtensionSet *addins;
};

struct _FoundryDebugManagerClass
{
  FoundryServiceClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryDebugManager, foundry_debug_manager, FOUNDRY_TYPE_SERVICE)

static void
foundry_debug_manager_provider_added (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      GObject          *addin,
                                      gpointer          user_data)
{
  FoundryDebugManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_DEBUGGER (addin));
  g_assert (FOUNDRY_IS_DEBUG_MANAGER (self));

  g_debug ("Adding FoundryDebugger of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_debugger_load (FOUNDRY_DEBUGGER (addin)));
}

static void
foundry_debug_manager_provider_removed (PeasExtensionSet *set,
                                        PeasPluginInfo   *plugin_info,
                                        GObject          *addin,
                                        gpointer          user_data)
{
  FoundryDebugManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_DEBUGGER (addin));
  g_assert (FOUNDRY_IS_DEBUG_MANAGER (self));

  g_debug ("Removing FoundryDebugger of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_debugger_unload (FOUNDRY_DEBUGGER (addin)));
}

static DexFuture *
foundry_debug_manager_start (FoundryService *service)
{
  FoundryDebugManager *self = (FoundryDebugManager *)service;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_SERVICE (service));
  g_assert (PEAS_IS_EXTENSION_SET (self->addins));

  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (foundry_debug_manager_provider_added),
                           self,
                           0);
  g_signal_connect_object (self->addins,
                           "extension-removed",
                           G_CALLBACK (foundry_debug_manager_provider_removed),
                           self,
                           0);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryDebugger) debugger = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures, foundry_debugger_load (debugger));
    }

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static DexFuture *
foundry_debug_manager_stop (FoundryService *service)
{
  FoundryDebugManager *self = (FoundryDebugManager *)service;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_SERVICE (service));

  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_debug_manager_provider_added),
                                        self);
  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_debug_manager_provider_removed),
                                        self);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryDebugger) debugger = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures, foundry_debugger_unload (debugger));
    }

  g_clear_object (&self->addins);

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static void
foundry_debug_manager_constructed (GObject *object)
{
  FoundryDebugManager *self = (FoundryDebugManager *)object;
  g_autoptr(FoundryContext) context = NULL;

  G_OBJECT_CLASS (foundry_debug_manager_parent_class)->constructed (object);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  self->addins = peas_extension_set_new (NULL,
                                         FOUNDRY_TYPE_DEBUGGER,
                                         "context", context,
                                         NULL);
}

static void
foundry_debug_manager_finalize (GObject *object)
{
  FoundryDebugManager *self = (FoundryDebugManager *)object;

  g_clear_object (&self->addins);

  G_OBJECT_CLASS (foundry_debug_manager_parent_class)->finalize (object);
}

static void
foundry_debug_manager_class_init (FoundryDebugManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  object_class->constructed = foundry_debug_manager_constructed;
  object_class->finalize = foundry_debug_manager_finalize;

  service_class->start = foundry_debug_manager_start;
  service_class->stop = foundry_debug_manager_stop;
}

static void
foundry_debug_manager_init (FoundryDebugManager *self)
{
}
