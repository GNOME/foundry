/* plugin-flatpak-documentation-provider.c
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

#include "plugin-flatpak.h"
#include "plugin-flatpak-documentation-provider.h"

struct _PluginFlatpakDocumentationProvider
{
  FoundryDocumentationProvider  parent_instance;
  GListStore                   *roots;
  GHashTable                   *monitors;
};

G_DEFINE_FINAL_TYPE (PluginFlatpakDocumentationProvider, plugin_flatpak_documentation_provider, FOUNDRY_TYPE_DOCUMENTATION_PROVIDER)

static void
plugin_flatpak_documentation_provider_update_installation (PluginFlatpakDocumentationProvider *self,
                                                           FlatpakInstallation                *installation)
{
  g_assert (PLUGIN_IS_FLATPAK_DOCUMENTATION_PROVIDER (self));
  g_assert (FLATPAK_IS_INSTALLATION (installation));

  /* TODO: Update roots for installation */
}

static void
plugin_flatpak_documentation_provider_update (PluginFlatpakDocumentationProvider *self,
                                              GFile                              *file,
                                              GFile                              *other_file,
                                              GFileMonitorEvent                   event,
                                              GFileMonitor                       *source)
{
  GHashTableIter iter;
  gpointer key, value;

  g_assert (PLUGIN_IS_FLATPAK_DOCUMENTATION_PROVIDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!file || G_IS_FILE (other_file));
  g_assert (G_IS_FILE_MONITOR (source));

  if (self->monitors == NULL || self->roots == NULL)
    return;

  g_hash_table_iter_init (&iter, self->monitors);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      FlatpakInstallation *installation = key;
      GFileMonitor *monitor = value;

      if (source == monitor)
        {
          plugin_flatpak_documentation_provider_update_installation (self, installation);
          break;
        }
    }
}

static DexFuture *
plugin_flatpak_documentation_provider_load_fiber (gpointer user_data)
{
  PluginFlatpakDocumentationProvider *self = user_data;
  g_autoptr(GPtrArray) installations = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (PLUGIN_IS_FLATPAK_DOCUMENTATION_PROVIDER (self));

  if (!(installations = dex_await_boxed (plugin_flatpak_load_installations (), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (guint i = 0; i < installations->len; i++)
    {
      FlatpakInstallation *installation = g_ptr_array_index (installations, i);
      g_autoptr(GFileMonitor) monitor = flatpak_installation_create_monitor (installation, NULL, NULL);

      if (monitor == NULL)
        continue;

      g_signal_connect_object (monitor,
                               "changed",
                               G_CALLBACK (plugin_flatpak_documentation_provider_update),
                               self,
                               G_CONNECT_SWAPPED);

      g_hash_table_insert (self->monitors,
                           g_object_ref (installation),
                           g_object_ref (monitor));

      plugin_flatpak_documentation_provider_update_installation (self, installation);
    }

  return dex_future_new_true ();
}

static DexFuture *
plugin_flatpak_documentation_provider_load (FoundryDocumentationProvider *provider)
{
  PluginFlatpakDocumentationProvider *self = (PluginFlatpakDocumentationProvider *)provider;

  g_assert (PLUGIN_IS_FLATPAK_DOCUMENTATION_PROVIDER (self));

  self->monitors = g_hash_table_new_full (NULL, NULL, g_object_unref, g_object_unref);
  self->roots = g_list_store_new (FOUNDRY_TYPE_DOCUMENTATION_ROOT);

  return dex_scheduler_spawn (NULL, 0,
                              plugin_flatpak_documentation_provider_load_fiber,
                              g_object_ref (provider),
                              g_object_unref);
}

static DexFuture *
plugin_flatpak_documentation_provider_unload (FoundryDocumentationProvider *provider)
{
  PluginFlatpakDocumentationProvider *self = (PluginFlatpakDocumentationProvider *)provider;

  g_assert (PLUGIN_IS_FLATPAK_DOCUMENTATION_PROVIDER (self));

  g_clear_pointer (&self->monitors, g_hash_table_unref);
  g_clear_object (&self->roots);

  return dex_future_new_true ();
}

static GListModel *
plugin_flatpak_documentation_provider_list_roots (FoundryDocumentationProvider *provider)
{
  return g_object_ref (G_LIST_MODEL (PLUGIN_FLATPAK_DOCUMENTATION_PROVIDER (provider)->roots));
}

static void
plugin_flatpak_documentation_provider_class_init (PluginFlatpakDocumentationProviderClass *klass)
{
  FoundryDocumentationProviderClass *documentation_provider_class = FOUNDRY_DOCUMENTATION_PROVIDER_CLASS (klass);

  documentation_provider_class->load = plugin_flatpak_documentation_provider_load;
  documentation_provider_class->unload = plugin_flatpak_documentation_provider_unload;
  documentation_provider_class->list_roots = plugin_flatpak_documentation_provider_list_roots;
}

static void
plugin_flatpak_documentation_provider_init (PluginFlatpakDocumentationProvider *self)
{
}
