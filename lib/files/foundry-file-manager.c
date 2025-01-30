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

#include <libpeas.h>

#include "foundry-file-manager.h"
#include "foundry-inhibitor.h"
#include "foundry-language-guesser.h"
#include "foundry-service-private.h"

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
  FoundryService    parent_instance;

  PeasExtensionSet *language_guessers;
};

struct _FoundryFileManagerClass
{
  FoundryServiceClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryFileManager, foundry_file_manager, FOUNDRY_TYPE_SERVICE)

static DexFuture *
foundry_file_manager_start (FoundryService *service)
{
  FoundryFileManager *self = (FoundryFileManager *)service;
  g_autoptr(FoundryContext) context = NULL;

  g_assert (FOUNDRY_IS_FILE_MANAGER (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  self->language_guessers = peas_extension_set_new (peas_engine_get_default (),
                                                    FOUNDRY_TYPE_LANGUAGE_GUESSER,
                                                    "context", context,
                                                    NULL);

  return dex_future_new_true ();
}

static DexFuture *
foundry_file_manager_stop (FoundryService *service)
{
  FoundryFileManager *self = (FoundryFileManager *)service;

  g_assert (FOUNDRY_IS_FILE_MANAGER (self));

  g_clear_object (&self->language_guessers);

  return dex_future_new_true ();
}

static void
foundry_file_manager_class_init (FoundryFileManagerClass *klass)
{
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  service_class->start = foundry_file_manager_start;
  service_class->stop = foundry_file_manager_stop;

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
  ADD_ICON (bundled_by_full_filename, ".gitignore", "builder-vcs-git-symbolic");
  ADD_ICON (bundled_by_full_filename, ".gitattributes", "builder-vcs-git-symbolic");
  ADD_ICON (bundled_by_full_filename, ".gitmodules", "builder-vcs-git-symbolic");
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
 * Returns: (transfer full): a [class@Dex.Future] that resovles
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

typedef struct _GuessLanguage
{
  FoundryInhibitor *inhibitor;
  GPtrArray        *guessers;
  GFile            *file;
  char             *content_type;
  GBytes           *contents;
} GuessLanguage;

static void
guess_language_free (GuessLanguage *state)
{
  g_clear_object (&state->file);
  g_clear_object (&state->inhibitor);
  g_clear_pointer (&state->content_type, g_free);
  g_clear_pointer (&state->contents, g_bytes_unref);
  g_clear_pointer (&state->guessers, g_ptr_array_unref);
  g_free (state);
}

static DexFuture *
foundry_file_manager_guess_language_fiber (gpointer data)
{
  GuessLanguage *state = data;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_INHIBITOR (state->inhibitor));
  g_assert (state->file || state->content_type || state->contents);
  g_assert (state->guessers != NULL);

  if (state->file != NULL && state->content_type == NULL)
    {
      g_autoptr(GFileInfo) info = dex_await_object (dex_file_query_info (state->file,
                                                                         G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                                                         G_FILE_QUERY_INFO_NONE,
                                                                         G_PRIORITY_DEFAULT),
                                                    NULL);

      if (info != NULL)
        state->content_type = g_strdup (g_file_info_get_content_type (info));
    }

  for (guint i = 0; i < state->guessers->len; i++)
    {
      FoundryLanguageGuesser *guesser = g_ptr_array_index (state->guessers, i);
      g_autofree char *language = NULL;

      if ((language = dex_await_string (foundry_language_guesser_guess (guesser, state->file, state->content_type, state->contents), NULL)))
        return dex_future_new_take_string (g_steal_pointer (&language));
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_FOUND,
                                "Failed to locate suitable language");
}

/**
 * foundry_file_manager_guess_language:
 * @self: a [class@Foundry.FileManager]
 * @file: (nullable): a [iface@Gio.File] or %NULL
 * @content_type: (nullable): the content-type as a string or %NULL
 * @contents: (nullable): a [struct@GLib.Bytes] of file contents or %NULL
 *
 * Attempts to guess the language of @file, @content_type, or @contents.
 *
 * One of @file, content_type, or @contents must be set.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a string
 *   containing the language identifier, or rejects with error.
 */
DexFuture *
foundry_file_manager_guess_language (FoundryFileManager *self,
                                     GFile              *file,
                                     const char         *content_type,
                                     GBytes             *contents)
{
  g_autoptr(FoundryInhibitor) inhibitor = NULL;
  g_autoptr(GError) error = NULL;
  GuessLanguage *state;

  dex_return_error_if_fail (FOUNDRY_IS_FILE_MANAGER (self));
  dex_return_error_if_fail (!file || G_IS_FILE (file));
  dex_return_error_if_fail (file || content_type || contents);

  if (!(inhibitor = foundry_contextual_inhibit (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  state = g_new0 (GuessLanguage, 1);
  g_set_object (&state->file, file);
  g_set_str (&state->content_type, content_type);
  state->contents = contents ? g_bytes_ref (contents) : NULL;
  state->guessers = g_ptr_array_new_with_free_func (g_object_unref);
  state->inhibitor = g_steal_pointer (&inhibitor);

  if (self->language_guessers != NULL)
    {
      GListModel *model = G_LIST_MODEL (self->language_guessers);
      guint n_items = g_list_model_get_n_items (model);

      for (guint i = 0; i < n_items; i++)
        g_ptr_array_add (state->guessers, g_list_model_get_item (model, i));
    }

  return dex_scheduler_spawn (NULL, 0,
                              foundry_file_manager_guess_language_fiber,
                              state,
                              (GDestroyNotify) guess_language_free);
}
