/* plugin-flatpak-dependency-provider.c
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "config.h"

#include "plugin-flatpak-config.h"
#include "plugin-flatpak-dependency.h"
#include "plugin-flatpak-dependency-provider.h"

#include "builder/plugin-flatpak-manifest.h"
#include "builder/plugin-flatpak-modules.h"

struct _PluginFlatpakDependencyProvider
{
  FoundryDependencyProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginFlatpakDependencyProvider, plugin_flatpak_dependency_provider, FOUNDRY_TYPE_DEPENDENCY_PROVIDER)

static DexFuture *
plugin_flatpak_dependency_provider_list_dependencies (FoundryDependencyProvider *dependency_provider,
                                                      FoundryConfig             *config,
                                                      FoundryDependency         *parent)
{
  g_autoptr(GListStore) store = NULL;

  g_assert (PLUGIN_IS_FLATPAK_DEPENDENCY_PROVIDER (dependency_provider));
  g_assert (FOUNDRY_IS_CONFIG (config));
  g_assert (!parent || FOUNDRY_IS_DEPENDENCY (parent));

  dex_return_error_if_fail (!parent || PLUGIN_IS_FLATPAK_DEPENDENCY (parent));

  store = g_list_store_new (FOUNDRY_TYPE_DEPENDENCY);

  /* TODO: Probably want to check the SDK and include that as a dependency
   * of the config (so that we can provide update API for that later on).
   */

  if (PLUGIN_IS_FLATPAK_CONFIG (config))
    {
      g_autoptr(PluginFlatpakManifest) manifest = plugin_flatpak_config_dup_manifest (PLUGIN_FLATPAK_CONFIG (config));
      g_autoptr(PluginFlatpakModules) modules = plugin_flatpak_manifest_dup_modules (manifest);
      g_autoptr(PluginFlatpakModule) primary_module = plugin_flatpak_config_dup_primary_module (PLUGIN_FLATPAK_CONFIG (config));
      guint n_items = 0;

      if (modules != NULL)
        n_items = g_list_model_get_n_items (G_LIST_MODEL (modules));

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(PluginFlatpakModule) module = g_list_model_get_item (G_LIST_MODEL (modules), i);
          g_autoptr(PluginFlatpakDependency) dependency = plugin_flatpak_dependency_new (module);

          if (primary_module != module)
            g_list_store_append (store, dependency);
        }
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static void
plugin_flatpak_dependency_provider_class_init (PluginFlatpakDependencyProviderClass *klass)
{
  FoundryDependencyProviderClass *dependency_provider_class = FOUNDRY_DEPENDENCY_PROVIDER_CLASS (klass);

  dependency_provider_class->list_dependencies = plugin_flatpak_dependency_provider_list_dependencies;
}

static void
plugin_flatpak_dependency_provider_init (PluginFlatpakDependencyProvider *self)
{
}
