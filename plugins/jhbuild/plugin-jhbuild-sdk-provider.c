/* plugin-jhbuild-sdk-provider.c
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

#include "plugin-jhbuild-sdk-provider.h"
#include "plugin-jhbuild-sdk.h"

struct _PluginJhbuildSdkProvider
{
  FoundrySdkProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginJhbuildSdkProvider, plugin_jhbuild_sdk_provider, FOUNDRY_TYPE_SDK_PROVIDER)

static DexFuture *
jhbuild_found_cb (DexFuture *completed,
                  gpointer   user_data)
{
  PluginJhbuildSdkProvider *self = user_data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundrySdk) sdk = NULL;

  g_assert (PLUGIN_IS_JHBUILD_SDK_PROVIDER (self));
  g_assert (dex_future_is_resolved (completed));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  sdk = g_object_new (PLUGIN_TYPE_JHBUILD_SDK,
                      "context", context,
                      NULL);

  foundry_sdk_provider_sdk_added (FOUNDRY_SDK_PROVIDER (self), sdk);

  return dex_future_new_true ();
}

static DexFuture *
plugin_jhbuild_sdk_provider_load (FoundrySdkProvider *sdk_provider)
{
  PluginJhbuildSdkProvider *self = (PluginJhbuildSdkProvider *)sdk_provider;
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  DexFuture *future;

  g_assert (PLUGIN_IS_JHBUILD_SDK_PROVIDER (self));

  launcher = foundry_process_launcher_new ();
  foundry_process_launcher_push_host (launcher);
  foundry_process_launcher_append_argv (launcher, "which");
  foundry_process_launcher_append_argv (launcher, "jhbuild");
  foundry_process_launcher_take_fd (launcher, -1, STDIN_FILENO);
  foundry_process_launcher_take_fd (launcher, -1, STDOUT_FILENO);
  foundry_process_launcher_take_fd (launcher, -1, STDERR_FILENO);

  if (!(subprocess = foundry_process_launcher_spawn (launcher, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  future = dex_subprocess_wait_check (subprocess);
  future = dex_future_then (future,
                            jhbuild_found_cb,
                            g_object_ref (self),
                            g_object_unref);

  return future;
}

static void
plugin_jhbuild_sdk_provider_class_init (PluginJhbuildSdkProviderClass *klass)
{
  FoundrySdkProviderClass *sdk_provider_class = FOUNDRY_SDK_PROVIDER_CLASS (klass);

  sdk_provider_class->load = plugin_jhbuild_sdk_provider_load;
}

static void
plugin_jhbuild_sdk_provider_init (PluginJhbuildSdkProvider *self)
{
}
