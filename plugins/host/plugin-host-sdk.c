/* plugin-host-sdk.c
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

#include "plugin-host-sdk.h"

struct _PluginHostSdk
{
  FoundrySdk parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginHostSdk, plugin_host_sdk, FOUNDRY_TYPE_SDK)

static void
plugin_host_sdk_finalize (GObject *object)
{
  G_OBJECT_CLASS (plugin_host_sdk_parent_class)->finalize (object);
}

static void
plugin_host_sdk_class_init (PluginHostSdkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_host_sdk_finalize;
}

static void
plugin_host_sdk_init (PluginHostSdk *self)
{
}

FoundrySdk *
plugin_host_sdk_new (FoundryContext *context)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);

  return g_object_new (PLUGIN_TYPE_HOST_SDK,
                       "context", context,
                       "id", "host",
                       "arch", foundry_get_default_arch (),
                       "name", _("My Computer"),
                       "installed", TRUE,
                       NULL);
}
