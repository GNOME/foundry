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
#include "plugin-devhelp-repository.h"

struct _PluginDevhelpDocumentationProvider
{
  FoundryDocumentationProvider  parent_instance;
  PluginDevhelpRepository      *repository;
};

G_DEFINE_FINAL_TYPE (PluginDevhelpDocumentationProvider, plugin_devhelp_documentation_provider, FOUNDRY_TYPE_DOCUMENTATION_PROVIDER)

static DexFuture *
plugin_devhelp_documentation_provider_load_fiber (gpointer user_data)
{
  PluginDevhelpDocumentationProvider *self = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *dir = NULL;
  g_autofree char *path = NULL;

  g_assert (PLUGIN_IS_DEVHELP_DOCUMENTATION_PROVIDER (self));

  dir = g_build_filename (g_get_user_data_dir (), "libfoundry", "doc", NULL);
  path = g_build_filename (dir, "devhelp.sqlite", NULL);

  if (!dex_await (foundry_mkdir_with_parents (dir, 0750), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(self->repository = dex_await_object (plugin_devhelp_repository_open (path), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
plugin_devhelp_documentation_provider_load (FoundryDocumentationProvider *provider)
{
  g_assert (PLUGIN_IS_DEVHELP_DOCUMENTATION_PROVIDER (provider));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_devhelp_documentation_provider_load_fiber,
                              g_object_ref (provider),
                              g_object_unref);
}

static DexFuture *
plugin_devhelp_documentation_provider_unload (FoundryDocumentationProvider *provider)
{
  PluginDevhelpDocumentationProvider *self = (PluginDevhelpDocumentationProvider *)provider;

  g_assert (PLUGIN_IS_DEVHELP_DOCUMENTATION_PROVIDER (self));

  g_clear_object (&self->repository);

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
