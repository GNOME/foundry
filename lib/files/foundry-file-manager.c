/* foundry-file-manager.c
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

#include <gio/gio.h>
#include <gom/gom.h>

#include "foundry-file-manager.h"
#include "foundry-inhibitor.h"
#include "foundry-service-private.h"
#include "foundry-util.h"

static gchar bundled_lookup_table[256];
static GIcon *x_zerosize_icon;
static GHashTable *bundled_by_content_type;
static GHashTable *bundled_by_full_filename;
/* This ensures those files get a proper icon when they end with .md
 * (markdown files).  It can't be fixed in the shared-mime-info db because
 * otherwise they wouldn't get detected as markdown anymore.
 */
static const struct {
  const gchar *searched_prefix;
  const gchar *icon_name;
} bundled_check_by_name_prefix[] = {
  { "README", "text-x-readme-symbolic" },
  { "NEWS", "text-x-changelog-symbolic" },
  { "CHANGELOG", "text-x-changelog-symbolic" },
  { "COPYING", "text-x-copying-symbolic" },
  { "LICENSE", "text-x-copying-symbolic" },
  { "AUTHORS", "text-x-authors-symbolic" },
  { "MAINTAINERS", "text-x-authors-symbolic" },
  { "Dockerfile", "text-makefile-symbolic" },
  { "Containerfile", "text-makefile-symbolic" },
  { "package.json", "text-makefile-symbolic" },
  { "pom.xml", "text-makefile-symbolic" },
  { "build.gradle", "text-makefile-symbolic" },
  { "Cargo.toml", "text-makefile-symbolic" },
  { "pyproject.toml", "text-makefile-symbolic" },
  { "requirements.txt", "text-makefile-symbolic" },
  { "go.mod", "text-makefile-symbolic" },
  { "wscript", "text-makefile-symbolic" },
};

static const struct {
  const char *suffix;
  const char *content_type;
} suffix_content_type_overrides[] = {
  { ".md", "text-markdown-symbolic" },
};

struct _FoundryFileManager
{
  FoundryService parent_instance;
};

struct _FoundryFileManagerClass
{
  FoundryServiceClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryFileManager, foundry_file_manager, FOUNDRY_TYPE_SERVICE)

static void
foundry_file_manager_class_init (FoundryFileManagerClass *klass)
{
  bundled_by_content_type = g_hash_table_new (g_str_hash, g_str_equal);
  bundled_by_full_filename = g_hash_table_new (g_str_hash, g_str_equal);

  /*
   * This needs to be updated when we add icons for specific mime-types
   * because of how icon theme loading works (and it wanting to use
   * Adwaita generic icons before our hicolor specific icons).
   */
#define ADD_ICON(t, n, v) g_hash_table_insert (t, (gpointer)n, v ? (gpointer)v : (gpointer)n)
  /* We don't get GThemedIcon fallbacks in an order that prioritizes some
   * applications over something more generic like text-x-script, so we need
   * to map the higher priority symbolic first.
   */
  ADD_ICON (bundled_by_content_type, "application-x-php-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "application-x-ruby-symbolic", "text-x-ruby-symbolic");
  ADD_ICON (bundled_by_content_type, "application-javascript-symbolic", "text-x-javascript-symbolic");
  ADD_ICON (bundled_by_content_type, "application-json-symbolic", "text-x-javascript-symbolic");
  ADD_ICON (bundled_by_content_type, "application-sql-symbolic", "text-sql-symbolic");

  ADD_ICON (bundled_by_content_type, "text-css-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-html-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-markdown-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-rust-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-sql-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-authors-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-blueprint-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-changelog-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-chdr-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-copying-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-c++src-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-csrc-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-go-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-javascript-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-python-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-python3-symbolic", "text-x-python-symbolic");
  ADD_ICON (bundled_by_content_type, "text-x-readme-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-ruby-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-script-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-vala-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-xml-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-meson", "text-makefile-symbolic");
  ADD_ICON (bundled_by_content_type, "text-x-cmake", "text-makefile-symbolic");
  ADD_ICON (bundled_by_content_type, "text-x-makefile", "text-makefile-symbolic");

  ADD_ICON (bundled_by_full_filename, ".editorconfig", "format-indent-more-symbolic");
  ADD_ICON (bundled_by_full_filename, ".gitignore", "vcs-git-symbolic");
  ADD_ICON (bundled_by_full_filename, ".gitattributes", "vcs-git-symbolic");
  ADD_ICON (bundled_by_full_filename, ".gitmodules", "vcs-git-symbolic");
#undef ADD_ICON

  /* Create faster check than doing full string checks */
  for (guint i = 0; i < G_N_ELEMENTS (bundled_check_by_name_prefix); i++)
    bundled_lookup_table[(guint)bundled_check_by_name_prefix[i].searched_prefix[0]] = 1;

  x_zerosize_icon = g_themed_icon_new ("text-x-generic-symbolic");
}

static void
foundry_file_manager_init (FoundryFileManager *self)
{
}

static DexFuture *
foundry_file_manager_show_fiber (gpointer data)
{
  GFile *file = data;
  g_autoptr(GVariantBuilder) builder = NULL;
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *uri = NULL;

  g_assert (G_IS_FILE (file));

  uri = g_file_get_uri (file);
  builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
  g_variant_builder_add (builder, "s", uri);

  if (!(bus = dex_await_object (dex_bus_get (G_BUS_TYPE_SESSION), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(reply = dex_await_variant (dex_dbus_connection_call (bus,
                                                             "org.freedesktop.FileManager1",
                                                             "/org/freedesktop/FileManager1",
                                                             "org.freedesktop.FileManager1",
                                                             "ShowItems",
                                                             g_variant_new ("(ass)", builder, ""),
                                                             NULL,
                                                             G_DBUS_CALL_FLAGS_NONE,
                                                             -1),
                                   &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

/**
 * foundry_file_manager_show:
 * @self: a #FoundryFileManager
 *
 * Requests that @file is shown in the users default file-manager.
 *
 * For example, on GNOME this would open Nautilus.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves
 *   to `true` if successful
 */
DexFuture *
foundry_file_manager_show (FoundryFileManager *self,
                           GFile              *file)
{
  dex_return_error_if_fail (FOUNDRY_IS_FILE_MANAGER (self));
  dex_return_error_if_fail (G_IS_FILE (file));

  return dex_scheduler_spawn (NULL, 0,
                              foundry_file_manager_show_fiber,
                              g_object_ref (file),
                              g_object_unref);
}

/**
 * foundry_file_manager_find_symbolic_icon:
 * @content_type: the content-type to lookup
 * @filename: (nullable): optional filename
 *
 * This function is similar to g_content_type_get_symbolic_icon() except that
 * it takes our bundled icons into account to ensure that they are taken at a
 * higher priority than the fallbacks from the current icon theme.
 *
 * Returns: (transfer full) (nullable): A #GIcon or %NULL
 */
GIcon *
foundry_file_manager_find_symbolic_icon (FoundryFileManager *self,
                                         const char         *content_type,
                                         const char         *filename)
{
  g_autoptr(GIcon) icon = NULL;
  const char * const *names;
  const char *replacement_by_filename;
  const char *suffix;

  g_return_val_if_fail (!self || FOUNDRY_IS_FILE_MANAGER (self), NULL);
  g_return_val_if_fail (content_type != NULL, NULL);

  /* Special case folders to never even try to use an overridden icon. For
   * example in the case of the LICENSES folder required by the REUSE licensing
   * helpers, the icon would be the copyright icon. Even if in this particular
   * case it might make sense to keep the copyright icon, it's just really
   * confusing to have a folder without a folder icon, especially since it becomes
   * an expanded folder icon when opening it in the project tree.
   */
  if (strcmp (content_type, "inode/directory") == 0)
    return g_content_type_get_symbolic_icon (content_type);
  else if (strcmp (content_type, "application/x-zerosize") == 0)
    return g_object_ref (x_zerosize_icon);

  /* Special case some weird content-types in the wild, particularly when Wine is
   * installed and taking over a content-type we would otherwise not expect.
   */
  if ((suffix = filename ? strrchr (filename, '.') : NULL))
    {
      for (guint i = 0; i < G_N_ELEMENTS (suffix_content_type_overrides); i++)
        {
          if (strcmp (suffix, suffix_content_type_overrides[i].suffix) == 0)
            {
              content_type = suffix_content_type_overrides[i].content_type;
              break;
            }
        }
    }

  icon = g_content_type_get_symbolic_icon (content_type);

  if (filename != NULL && bundled_lookup_table [(guint8)filename[0]])
    {
      for (guint j = 0; j < G_N_ELEMENTS (bundled_check_by_name_prefix); j++)
        {
          const gchar *searched_prefix = bundled_check_by_name_prefix[j].searched_prefix;

          /* Check prefix but ignore case, because there might be some files named e.g. ReadMe.txt */
          if (g_ascii_strncasecmp (filename, searched_prefix, strlen (searched_prefix)) == 0)
            return g_icon_new_for_string (bundled_check_by_name_prefix[j].icon_name, NULL);
        }
    }

  if (filename != NULL)
    {
      if ((replacement_by_filename = g_hash_table_lookup (bundled_by_full_filename, filename)))
        return g_icon_new_for_string (replacement_by_filename, NULL);
    }

  if (G_IS_THEMED_ICON (icon))
    {
      names = g_themed_icon_get_names (G_THEMED_ICON (icon));

      if (names != NULL)
        {
          gboolean fallback = FALSE;

          for (guint i = 0; names[i] != NULL; i++)
            {
              const gchar *replace = g_hash_table_lookup (bundled_by_content_type, names[i]);

              if (replace != NULL)
                return g_icon_new_for_string (replace, NULL);

              fallback |= (g_str_equal (names[i], "text-plain") ||
                           g_str_equal (names[i], "application-octet-stream"));
            }

          if (fallback)
            return g_icon_new_for_string ("text-x-generic-symbolic", NULL);
        }
    }

  return g_steal_pointer (&icon);
}

static DexFuture *
foundry_file_manager_write_metadata_fiber (FoundryFileManager *self,
                                           GFile              *file,
                                           GFileInfo          *file_info)
{
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_FILE_MANAGER (self));
  g_assert (G_IS_FILE (file));
  g_assert (G_IS_FILE_INFO (file));

  /* First try to set the metadata on the file itself. If this is
   * a successful then we are done. Otherwise we'll have to use a
   * fallback mechanism to set metadata.
   */
  if (dex_await (dex_file_set_attributes (file, file_info, G_FILE_QUERY_INFO_NONE, 0), &error))
    return dex_future_new_true ();

  /* TODO: Do fallback with local metadata file */

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Metadata not supported");
}

/**
 * foundry_file_manager_write_metadata:
 * @self: a [class@Foundry.FileManager]
 * @file: a [iface@Gio.File]
 * @file_info: a [class@Gio.FileInfo]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   boolean or rejects with error.
 */
DexFuture *
foundry_file_manager_write_metadata (FoundryFileManager *self,
                                     GFile              *file,
                                     GFileInfo          *file_info)
{
  dex_return_error_if_fail (FOUNDRY_IS_FILE_MANAGER (self));
  dex_return_error_if_fail (G_IS_FILE (file));
  dex_return_error_if_fail (G_IS_FILE_INFO (file));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (foundry_file_manager_write_metadata_fiber),
                                  3,
                                  FOUNDRY_TYPE_FILE_MANAGER, self,
                                  G_TYPE_FILE, file,
                                  G_TYPE_FILE_INFO, file_info);
}
