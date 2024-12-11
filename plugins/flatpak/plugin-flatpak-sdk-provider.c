/* plugin-flatpak-sdk-provider.c
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

#include "plugin-flatpak.h"
#include "plugin-flatpak-sdk.h"
#include "plugin-flatpak-sdk-provider.h"

struct _PluginFlatpakSdkProvider
{
  FoundrySdkProvider  parent_instance;
  GPtrArray          *installations;
};

G_DEFINE_FINAL_TYPE (PluginFlatpakSdkProvider, plugin_flatpak_sdk_provider, FOUNDRY_TYPE_SDK_PROVIDER)

static DexFuture *
plugin_flatpak_sdk_provider_load_fiber (gpointer user_data)
{
  PluginFlatpakSdkProvider *self = user_data;
  FlatpakInstallation *installation;
  g_autoptr(FoundryContext) context = NULL;

  g_assert (PLUGIN_IS_FLATPAK_SDK_PROVIDER (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (context));

  /* Try loading the system installation */
  if ((installation = dex_await_object (plugin_flatpak_installation_new_system (), NULL)))
    g_ptr_array_add (self->installations, g_steal_pointer (&installation));

  /* Try loading the default user installation */
  if ((installation = dex_await_object (plugin_flatpak_installation_new_user (), NULL)))
    g_ptr_array_add (self->installations, g_steal_pointer (&installation));

  /* Try loading the private installation for Foundry */
  if ((installation = dex_await_object (plugin_flatpak_installation_new_private (context), NULL)))
    g_ptr_array_add (self->installations, g_steal_pointer (&installation));

  return dex_future_new_true ();
}

static DexFuture *
plugin_flatpak_sdk_provider_load (FoundrySdkProvider *sdk_provider)
{
  PluginFlatpakSdkProvider *self = (PluginFlatpakSdkProvider *)sdk_provider;

  g_assert (PLUGIN_IS_FLATPAK_SDK_PROVIDER (self));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_flatpak_sdk_provider_load_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static DexFuture *
plugin_flatpak_sdk_provider_unload (FoundrySdkProvider *sdk_provider)
{
  PluginFlatpakSdkProvider *self = (PluginFlatpakSdkProvider *)sdk_provider;

  g_assert (PLUGIN_IS_FLATPAK_SDK_PROVIDER (self));

  return FOUNDRY_SDK_PROVIDER_CLASS (plugin_flatpak_sdk_provider_parent_class)->unload (sdk_provider);
}

static void
plugin_flatpak_sdk_provider_finalize (GObject *object)
{
  PluginFlatpakSdkProvider *self = (PluginFlatpakSdkProvider *)object;

  g_clear_pointer (&self->installations, g_ptr_array_unref);

  G_OBJECT_CLASS (plugin_flatpak_sdk_provider_parent_class)->finalize (object);
}

static void
plugin_flatpak_sdk_provider_class_init (PluginFlatpakSdkProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundrySdkProviderClass *sdk_provider_class = FOUNDRY_SDK_PROVIDER_CLASS (klass);

  object_class->finalize = plugin_flatpak_sdk_provider_finalize;

  sdk_provider_class->load = plugin_flatpak_sdk_provider_load;
  sdk_provider_class->unload = plugin_flatpak_sdk_provider_unload;
}

static void
plugin_flatpak_sdk_provider_init (PluginFlatpakSdkProvider *self)
{
  self->installations = g_ptr_array_new_with_free_func (g_object_unref);
}
