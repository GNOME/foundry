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

#include "plugin-buildconfig-config-provider.h"

struct _PluginBuildconfigConfigProvider
{
  FoundryConfigProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginBuildconfigConfigProvider, plugin_buildconfig_config_provider, FOUNDRY_TYPE_CONFIG_PROVIDER)

static DexFuture *
plugin_buildconfig_config_provider_load_fiber (gpointer user_data)
{
  PluginBuildconfigConfigProvider *self = user_data;

  g_assert (PLUGIN_IS_BUILDCONFIG_CONFIG_PROVIDER (self));

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
