/* plugin-buildconfig-config-provider.c
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

#include "plugin-buildconfig-config.h"
#include "plugin-buildconfig-config-provider.h"

struct _PluginBuildconfigConfigProvider
{
  FoundryConfigProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginBuildconfigConfigProvider, plugin_buildconfig_config_provider, FOUNDRY_TYPE_CONFIG_PROVIDER)

static void
plugin_buildconfig_config_provider_add_default (PluginBuildconfigConfigProvider *self)
{
  g_autoptr(PluginBuildconfigConfig) config = NULL;
  g_autoptr(FoundryContext) context = NULL;

  g_assert (PLUGIN_IS_BUILDCONFIG_CONFIG_PROVIDER (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  config = g_object_new (PLUGIN_TYPE_BUILDCONFIG_CONFIG,
                         "id", "default",
                         "name", _("Default"),
                         "context", context,
                         NULL);

  foundry_config_provider_config_added (FOUNDRY_CONFIG_PROVIDER (self),
                                        FOUNDRY_CONFIG (config));
}

static DexFuture *
plugin_buildconfig_config_provider_load_fiber (gpointer user_data)
{
  PluginBuildconfigConfigProvider *self = user_data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GFile) dot_buildconfig = NULL;
  g_autoptr(GFile) project_dir = NULL;

  g_assert (PLUGIN_IS_BUILDCONFIG_CONFIG_PROVIDER (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  project_dir = foundry_context_dup_project_directory (context);
  dot_buildconfig = g_file_get_child (project_dir, ".buildconfig");

  if (dex_await_boolean (dex_file_query_exists (dot_buildconfig), NULL))
    {
      g_autoptr(GKeyFile) key_file = NULL;
      g_autoptr(GError) error = NULL;

      if ((key_file = dex_await_boxed (foundry_key_file_new_from_file (dot_buildconfig, 0), &error)))
        {
          /* TODO: load existing buildconfig and return*/

          return dex_future_new_true ();
        }
    }

  plugin_buildconfig_config_provider_add_default (self);

  return dex_future_new_true ();
}

static DexFuture *
plugin_buildconfig_config_provider_load (FoundryConfigProvider *provider)
{
  g_assert (PLUGIN_IS_BUILDCONFIG_CONFIG_PROVIDER (provider));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_buildconfig_config_provider_load_fiber,
                              g_object_ref (provider),
                              g_object_unref);
}

static void
plugin_buildconfig_config_provider_finalize (GObject *object)
{
  G_OBJECT_CLASS (plugin_buildconfig_config_provider_parent_class)->finalize (object);
}

static void
plugin_buildconfig_config_provider_class_init (PluginBuildconfigConfigProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryConfigProviderClass *config_provider_class = FOUNDRY_CONFIG_PROVIDER_CLASS (klass);

  object_class->finalize = plugin_buildconfig_config_provider_finalize;

  config_provider_class->load = plugin_buildconfig_config_provider_load;
}

static void
plugin_buildconfig_config_provider_init (PluginBuildconfigConfigProvider *self)
{
}
