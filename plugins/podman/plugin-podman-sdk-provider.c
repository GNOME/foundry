/* plugin-podman-sdk-provider.c
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

#include <libdex.h>

#include "plugin-podman-sdk-provider.h"
#include "plugin-podman-sdk.h"

struct _PluginPodmanSdkProvider
{
  FoundrySdkProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginPodmanSdkProvider, plugin_podman_sdk_provider, FOUNDRY_TYPE_SDK_PROVIDER)

static DexFuture *
plugin_podman_sdk_provider_load (FoundrySdkProvider *sdk_provider)
{
  PluginPodmanSdkProvider *self = (PluginPodmanSdkProvider *)sdk_provider;

  g_assert (PLUGIN_IS_PODMAN_SDK_PROVIDER (self));

  return dex_future_new_true ();
}

static void
plugin_podman_sdk_provider_class_init (PluginPodmanSdkProviderClass *klass)
{
  FoundrySdkProviderClass *sdk_provider_class = FOUNDRY_SDK_PROVIDER_CLASS (klass);

  sdk_provider_class->load = plugin_podman_sdk_provider_load;
}

static void
plugin_podman_sdk_provider_init (PluginPodmanSdkProvider *self)
{
}
