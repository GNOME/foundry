/* plugin-internal-tweak-provider.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include <libpeas.h>

#include "plugin-internal-tweak-provider.h"

struct _PluginInternalTweakProvider
{
  FoundryTweakProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginInternalTweakProvider, plugin_internal_tweak_provider, FOUNDRY_TYPE_TWEAK_PROVIDER)

static const FoundryTweakInfo top_page_info[] = {
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/project/",
    .title = N_("Projects"),
    .icon_name = "folder-symbolic",
    .display_hint = "page",
    .section = "-projects",
    .sort_key = "030-010",
  },
};

static DexFuture *
plugin_internal_tweak_provider_load (FoundryTweakProvider *provider)
{
  static const char * const prefixes[] = FOUNDRY_STRV_INIT ("/app", "/project", "/user");

  dex_return_error_if_fail (PLUGIN_IS_INTERNAL_TWEAK_PROVIDER (provider));

  for (guint i = 0; prefixes[i]; i++)
    {
      foundry_tweak_provider_register (provider,
                                       GETTEXT_PACKAGE,
                                       prefixes[i],
                                       top_page_info,
                                       G_N_ELEMENTS (top_page_info),
                                       NULL);
    }

  return dex_future_new_true ();
}

static void
plugin_internal_tweak_provider_class_init (PluginInternalTweakProviderClass *klass)
{
  FoundryTweakProviderClass *provider_class = FOUNDRY_TWEAK_PROVIDER_CLASS (klass);

  provider_class->load = plugin_internal_tweak_provider_load;
}

static void
plugin_internal_tweak_provider_init (PluginInternalTweakProvider *self)
{
}
