/* plugin-flatpak-config-provider.c
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

#include "plugin-flatpak-config-provider.h"
#include "plugin-flatpak-config.h"
#include "plugin-flatpak-json-manifest.h"

#define DISCOVERY_MAX_DEPTH 3

struct _PluginFlatpakConfigProvider
{
  FoundryConfigProvider parent_instnace;
};

G_DEFINE_FINAL_TYPE (PluginFlatpakConfigProvider, plugin_flatpak_config_provider, FOUNDRY_TYPE_CONFIG_PROVIDER)

static DexFuture *
plugin_flatpak_config_provider_load_fiber (gpointer user_data)
{
  PluginFlatpakConfigProvider *self = user_data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GPtrArray) matching = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_dir = NULL;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (PLUGIN_IS_FLATPAK_CONFIG_PROVIDER (self));

  if (!(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))) ||
      !(project_dir = foundry_context_dup_project_directory (context)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_CANCELLED,
                                  "Operation cancelled");

  /* First find all the files that match potential flatpak manifests */
  if (!(matching = dex_await_boxed (foundry_file_find_with_depth (project_dir,
                                                                  "*.*.json",
                                                                  DISCOVERY_MAX_DEPTH),
                                    &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (guint i = 0; i < matching->len; i++)
    {
      g_autoptr(PluginFlatpakConfig) manifest = NULL;
      g_autoptr(GError) manifest_error = NULL;
      GFile *match = g_ptr_array_index (matching, i);

      if (!(manifest = dex_await_object (plugin_flatpak_json_manifest_new (context, match),
                                         &manifest_error)))
        {
          g_debug ("Ignoring file %s because error: %s",
                   g_file_peek_path (match),
                   manifest_error->message);
          continue;
        }

      foundry_config_provider_config_added (FOUNDRY_CONFIG_PROVIDER (self),
                                            FOUNDRY_CONFIG (manifest));
    }

  return dex_future_new_true ();
}


static DexFuture *
plugin_flatpak_config_provider_load (FoundryConfigProvider *provider)
{
  PluginFlatpakConfigProvider *self = (PluginFlatpakConfigProvider *)provider;
  DexFuture *future;

  FOUNDRY_ENTRY;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (PLUGIN_IS_FLATPAK_CONFIG_PROVIDER (provider));

  future = dex_scheduler_spawn (NULL, 0,
                                plugin_flatpak_config_provider_load_fiber,
                                g_object_ref (self),
                                g_object_unref);

  FOUNDRY_RETURN (future);
}

static void
plugin_flatpak_config_provider_finalize (GObject *object)
{
  G_OBJECT_CLASS (plugin_flatpak_config_provider_parent_class)->finalize (object);
}

static void
plugin_flatpak_config_provider_class_init (PluginFlatpakConfigProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryConfigProviderClass *config_provider_class = FOUNDRY_CONFIG_PROVIDER_CLASS (klass);

  object_class->finalize = plugin_flatpak_config_provider_finalize;

  config_provider_class->load = plugin_flatpak_config_provider_load;
}

static void
plugin_flatpak_config_provider_init (PluginFlatpakConfigProvider *self)
{
}
