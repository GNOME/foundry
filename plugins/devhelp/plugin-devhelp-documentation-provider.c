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

#include "foundry-gom-private.h"

#include "plugin-devhelp-documentation-provider.h"
#include "plugin-devhelp-importer.h"
#include "plugin-devhelp-purge-missing.h"
#include "plugin-devhelp-repository.h"
#include "plugin-devhelp-sdk.h"

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
plugin_devhelp_documentation_provider_index_fiber (PluginDevhelpDocumentationProvider *self,
                                                   GListModel                         *roots,
                                                   PluginDevhelpRepository            *repository)
{
  g_autoptr(PluginDevhelpPurgeMissing) purge_missing = NULL;
  g_autoptr(GError) error = NULL;
  guint n_items;

  g_assert (PLUGIN_IS_DEVHELP_DOCUMENTATION_PROVIDER (self));
  g_assert (G_IS_LIST_MODEL (roots));
  g_assert (PLUGIN_IS_DEVHELP_REPOSITORY (repository));

  n_items = g_list_model_get_n_items (roots);

  if (n_items > 0)
    {
      g_autoptr(PluginDevhelpImporter) importer = plugin_devhelp_importer_new ();
      g_autoptr(PluginDevhelpProgress) progress = plugin_devhelp_progress_new ();

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(FoundryDocumentationRoot) root = g_list_model_get_item (roots, i);
          g_autoptr(PluginDevhelpSdk) sdk = NULL;
          g_autoptr(GListModel) directories = foundry_documentation_root_list_directories (root);
          g_autofree char *ident = foundry_documentation_root_dup_identifier (root);
          g_autofree char *title = foundry_documentation_root_dup_title (root);
          g_autoptr(GIcon) icon = foundry_documentation_root_dup_icon (root);
          const char *icon_name = NULL;
          guint n_dirs = g_list_model_get_n_items (directories);
          gint64 sdk_id = 0;

          if (G_IS_THEMED_ICON (icon))
            icon_name = g_themed_icon_get_names (G_THEMED_ICON (icon))[0];

          /* Insert the SDK if it is not yet available */
          if (!(sdk = dex_await_object (plugin_devhelp_repository_find_sdk (repository, ident), NULL)))
            {
              sdk = g_object_new (PLUGIN_TYPE_DEVHELP_SDK,
                                  "repository", repository,
                                  "name", title,
                                  "version", NULL,
                                  "ident", ident,
                                  "icon-name", icon_name,
                                  NULL);

              if (!dex_await (gom_resource_save (GOM_RESOURCE (sdk)), &error))
                return dex_future_new_for_error (g_steal_pointer (&error));
            }

          sdk_id = plugin_devhelp_sdk_get_id (sdk);

          for (guint j = 0; j < n_dirs; j++)
            {
              g_autoptr(GFile) dir = g_list_model_get_item (directories, i);
              const char *path = g_file_peek_path (dir);

              plugin_devhelp_importer_add_directory (importer, path, sdk_id);
            }
        }

      if (!dex_await (plugin_devhelp_importer_import (importer, repository, progress), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  /* Now purge any empty SDK entries */
  purge_missing = plugin_devhelp_purge_missing_new ();
  if (!dex_await (plugin_devhelp_purge_missing_run (purge_missing, repository), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
plugin_devhelp_documentation_provider_index (FoundryDocumentationProvider *provider,
                                             GListModel                   *roots)
{
  PluginDevhelpDocumentationProvider *self = (PluginDevhelpDocumentationProvider *)provider;

  dex_return_error_if_fail (PLUGIN_IS_DEVHELP_DOCUMENTATION_PROVIDER (self));
  dex_return_error_if_fail (G_IS_LIST_MODEL (roots));
  dex_return_error_if_fail (PLUGIN_IS_DEVHELP_REPOSITORY (self->repository));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_devhelp_documentation_provider_index_fiber),
                                  3,
                                  PLUGIN_TYPE_DEVHELP_DOCUMENTATION_PROVIDER, provider,
                                  G_TYPE_LIST_MODEL, roots,
                                  PLUGIN_TYPE_DEVHELP_REPOSITORY, self->repository);
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
