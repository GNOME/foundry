/* plugin-devhelp-documentation-provider.c
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

#include "plugin-devhelp-documentation-provider.h"

struct _PluginDevhelpDocumentationProvider
{
  FoundryDocumentationProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginDevhelpDocumentationProvider, plugin_devhelp_documentation_provider, FOUNDRY_TYPE_DOCUMENTATION_PROVIDER)

static DexFuture *
plugin_devhelp_documentation_provider_load (FoundryDocumentationProvider *provider)
{
  g_assert (PLUGIN_IS_DEVHELP_DOCUMENTATION_PROVIDER (provider));

  return dex_future_new_true ();
}

static DexFuture *
plugin_devhelp_documentation_provider_unload (FoundryDocumentationProvider *provider)
{
  g_assert (PLUGIN_IS_DEVHELP_DOCUMENTATION_PROVIDER (provider));

  return dex_future_new_true ();
}

static DexFuture *
plugin_devhelp_documentation_provider_index (FoundryDocumentationProvider *provider,
                                             GListModel                   *roots)
{
  g_assert (PLUGIN_IS_DEVHELP_DOCUMENTATION_PROVIDER (provider));
  g_assert (G_IS_LIST_MODEL (roots));

  return dex_future_new_true ();
}

static void
plugin_devhelp_documentation_provider_class_init (PluginDevhelpDocumentationProviderClass *klass)
{
  FoundryDocumentationProviderClass *documentation_provider_class = FOUNDRY_DOCUMENTATION_PROVIDER_CLASS (klass);

  documentation_provider_class->load = plugin_devhelp_documentation_provider_load;
  documentation_provider_class->unload = plugin_devhelp_documentation_provider_unload;
  documentation_provider_class->index = plugin_devhelp_documentation_provider_index;
}

static void
plugin_devhelp_documentation_provider_init (PluginDevhelpDocumentationProvider *self)
{
}
