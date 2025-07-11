/* foundry-compile-commands.c
 *
 * Copyright 2017-2025 Christian Hergert <chergert@redhat.com>
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

#include <json-glib/json-glib.h>

#include <string.h>

#include "foundry-compile-commands.h"
#include "foundry-debug.h"
#include "foundry-util.h"

/**
 * SECTION:foundry-compile-commands
 * @title: FoundryCompileCommands
 * @short_description: Integration with compile_commands.json
 *
 * The #FoundryCompileCommands object provides a simplified interface to
 * interact with compile_commands.json files which are generated by a
 * number of build systems, including Clang tooling, Meson and CMake.
 */

struct _FoundryCompileCommands
{
  GObject parent_instance;

  /*
   * The info_by_file field contains a hashtable whose keys are #GFile
   * matching the file that is to be compiled. It contains as a value
   * the CompileInfo struct describing how to compile that file.
   */
  GHashTable *info_by_file;

  /*
   * The vala_info field contains an array of every vala like file we've
   * discovered while parsing the database. This is used so because some
   * compile_commands.json only have a single valac command which won't
   * match the file we want to lookup (Notably Meson-based).
   */
  GPtrArray *vala_info;
};

typedef struct
{
  GFile *directory;
  GFile *file;
  char  *command;
} CompileInfo;

G_DEFINE_FINAL_TYPE (FoundryCompileCommands, foundry_compile_commands, G_TYPE_OBJECT)

static gboolean
path_is_c_like (const char *path)
{
  const char *dot;

  if (path == NULL)
    return FALSE;

  if ((dot = strrchr (path, '.')))
    return foundry_str_equal0 (dot, ".c") || foundry_str_equal0 (dot, ".h");

  return FALSE;
}

static gboolean
path_is_cpp_like (const char *path)
{
  static const char *cpplike[] = {
    ".cc", ".cpp", ".c++", ".cxx",
    ".hh", ".hpp", ".h++", ".hxx",
  };
  const char *dot;

  if (path == NULL)
    return FALSE;

  if ((dot = strrchr (path, '.')))
    {
      for (guint i = 0; i < G_N_ELEMENTS (cpplike); i++)
        {
          if (foundry_str_equal0 (dot, cpplike[i]))
            return TRUE;
        }
    }

  return FALSE;
}

static void
compile_info_free (gpointer data)
{
  CompileInfo *info = data;

  if (info != NULL)
    {
      g_clear_object (&info->directory);
      g_clear_object (&info->file);
      g_clear_pointer (&info->command, g_free);
      g_free (info);
    }
}

static void
foundry_compile_commands_finalize (GObject *object)
{
  FoundryCompileCommands *self = (FoundryCompileCommands *)object;

  g_clear_pointer (&self->info_by_file, g_hash_table_unref);
  g_clear_pointer (&self->vala_info, g_ptr_array_unref);

  G_OBJECT_CLASS (foundry_compile_commands_parent_class)->finalize (object);
}

static void
foundry_compile_commands_class_init (FoundryCompileCommandsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_compile_commands_finalize;
}

static void
foundry_compile_commands_init (FoundryCompileCommands *self)
{
}

static DexFuture *
foundry_compile_commands_new_fiber (gpointer data)
{
  GFile *gfile = data;
  g_autoptr(FoundryCompileCommands) self = NULL;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GHashTable) info_by_file = NULL;
  g_autoptr(GHashTable) directories_by_path = NULL;
  g_autoptr(GPtrArray) vala_info = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) bytes = NULL;
  const char *contents;
  JsonNode *root;
  JsonArray *ar;
  gsize len = 0;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (G_IS_FILE (gfile));

  self = g_object_new (FOUNDRY_TYPE_COMPILE_COMMANDS, NULL);
  parser = json_parser_new ();

  if (!(bytes = dex_await_boxed (dex_file_load_contents_bytes (gfile), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  contents = g_bytes_get_data (bytes, &len);

  if (!json_parser_load_from_data (parser, contents, len, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(root = json_parser_get_root (parser)) ||
      !JSON_NODE_HOLDS_ARRAY (root) ||
      !(ar = json_node_get_array (root)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "Failed to extract commands, invalid json");

  info_by_file = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, NULL, compile_info_free);
  directories_by_path = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
  vala_info = g_ptr_array_new_with_free_func (compile_info_free);

  n_items = json_array_get_length (ar);

  for (guint i = 0; i < n_items; i++)
    {
      CompileInfo *info;
      JsonNode *item;
      JsonNode *value;
      JsonObject *obj;
      GFile *dir;
      const char *directory = NULL;
      const char *file = NULL;
      const char *command = NULL;

      item = json_array_get_element (ar, i);

      /* Skip past this node if its invalid for some reason, so we
       * can try to be tolerante of errors created by broken tooling.
       */
      if (item == NULL ||
          !JSON_NODE_HOLDS_OBJECT (item) ||
          !(obj = json_node_get_object (item)))
        continue;

      if (json_object_has_member (obj, "file") &&
          (value = json_object_get_member (obj, "file")) &&
          JSON_NODE_HOLDS_VALUE (value))
        file = json_node_get_string (value);

      if (json_object_has_member (obj, "directory") &&
          (value = json_object_get_member (obj, "directory")) &&
          JSON_NODE_HOLDS_VALUE (value))
        directory = json_node_get_string (value);

      if (json_object_has_member (obj, "command") &&
          (value = json_object_get_member (obj, "command")) &&
          JSON_NODE_HOLDS_VALUE (value))
        command = json_node_get_string (value);

      /* Ignore items that are missing something or other */
      if (file == NULL || command == NULL || directory == NULL)
        continue;

      /* Try to reduce the number of GFile we have for directories */
      if (!(dir = g_hash_table_lookup (directories_by_path, directory)))
        {
          dir = g_file_new_for_path (directory);
          g_hash_table_insert (directories_by_path, (char *)directory, dir);
        }

      info = g_new0 (CompileInfo, 1);
      info->file = g_file_resolve_relative_path (dir, file);
      info->directory = g_object_ref (dir);
      info->command = g_strdup (command);
      g_hash_table_replace (info_by_file, info->file, info);

      /*
       * We might need to keep a special copy of this for resolving .vala
       * builds which won't be able to be matched based on the filename. We
       * keep all of them around right now in case we want to later on find
       * the closest match based on directory.
       */
      if (g_str_has_suffix (file, ".vala"))
        {
          info = g_new0 (CompileInfo, 1);
          info->file = g_file_resolve_relative_path (dir, file);
          info->directory = g_object_ref (dir);
          info->command = g_strdup (command);
          g_ptr_array_add (vala_info, info);
        }

      if (command != NULL && strstr (command, "valac"))
        {
          g_auto(GStrv) argv = NULL;
          gint argc = 0;

          if (!g_shell_parse_argv (command, &argc, &argv, NULL))
            continue;

          for (guint j = 0; j < argc; j++)
            {
              if (strstr (argv[j], ".vala"))
                {
                  info = g_new0 (CompileInfo, 1);
                  info->file = g_file_resolve_relative_path (dir, argv[j]);
                  info->directory = g_object_ref (dir);
                  info->command = g_strdup (command);
                  g_ptr_array_add (vala_info, info);
                }
            }
        }
    }

  self->info_by_file = g_steal_pointer (&info_by_file);
  self->vala_info = g_steal_pointer (&vala_info);

  return dex_future_new_take_object (g_steal_pointer (&self));
}

/**
 * foundry_compile_commands_new:
 * @file: a [iface@Gio.File] to load
 *
 * Creates a new #FoundryCompileCommands object containing the parsed values
 * from a compile_commands.json file.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves
 *   to a [class@Foundry.CompileCommands] or rejects with error.
 */
DexFuture *
foundry_compile_commands_new (GFile *file)
{
  dex_return_error_if_fail (G_IS_FILE (file));

  return dex_scheduler_spawn (NULL, 0,
                              foundry_compile_commands_new_fiber,
                              g_object_ref (file),
                              g_object_unref);
}

static gboolean
suffix_is_vala (const char *suffix)
{
  if (suffix == NULL)
    return FALSE;

  return !!strstr (suffix, ".vala");
}

static char *
foundry_compile_commands_resolve (FoundryCompileCommands *self,
                                  const CompileInfo      *info,
                                  const char             *path)
{
  g_autoptr(GFile) file = NULL;

  g_assert (FOUNDRY_IS_COMPILE_COMMANDS (self));
  g_assert (info != NULL);

  if (path == NULL)
    return NULL;

  if (g_path_is_absolute (path))
    return g_strdup (path);

  file = g_file_resolve_relative_path (info->directory, path);
  if (file != NULL)
    return g_file_get_path (file);

  return NULL;
}

static void
foundry_compile_commands_filter_c (FoundryCompileCommands   *self,
                                   const CompileInfo        *info,
                                   const char * const       *system_includes,
                                   char                   ***argv)
{
  g_autoptr(GPtrArray) ar = NULL;

  g_assert (FOUNDRY_IS_COMPILE_COMMANDS (self));
  g_assert (info != NULL);
  g_assert (argv != NULL);

  if (*argv == NULL)
    return;

  ar = g_ptr_array_new_with_free_func (g_free);

  if (system_includes != NULL)
    {
      for (guint i = 0; system_includes[i]; i++)
        g_ptr_array_add (ar, g_strdup_printf ("-I%s", system_includes[i]));
    }

  for (guint i = 0; (*argv)[i] != NULL; i++)
    {
      const char *param = (*argv)[i];
      const char *next = (*argv)[i+1];
      g_autofree char *resolved = NULL;

      if (param[0] != '-')
        continue;

      switch (param[1])
        {
        case 'I': /* -I/usr/include, -I /usr/include */
          if (param[2] != '\0')
            next = &param[2];
          resolved = foundry_compile_commands_resolve (self, info, next);
          if (resolved != NULL)
            g_ptr_array_add (ar, g_strdup_printf ("-I%s", resolved));
          break;

        case 'f': /* -fPIC */
        case 'W': /* -Werror... */
        case 'm': /* -m64 -mtune=native */
        case 'O': /* -O2 */
          g_ptr_array_add (ar, g_strdup (param));
          break;

        case 'M': /* -MMD -MQ -MT -MF <file> */
          /* ignore the -M class of commands */
          break;

        case 'D': /* -DFOO, -D FOO */
        case 'x': /* -xc++ */
          g_ptr_array_add (ar, g_strdup (param));
          if (param[2] == '\0')
            g_ptr_array_add (ar, g_strdup (next));
          break;

        default:
          if (g_str_has_prefix (param, "-std=") ||
              g_str_has_prefix (param, "--std=") ||
              foundry_str_equal0 (param, "-pthread"))
            {
              g_ptr_array_add (ar, g_strdup (param));
            }
          else if (next != NULL &&
                   (foundry_str_equal0 (param, "-include") ||
                    foundry_str_equal0 (param, "-isystem")))
            {
              g_ptr_array_add (ar, g_strdup (param));
              g_ptr_array_add (ar, foundry_compile_commands_resolve (self, info, next));
            }
          break;
        }
    }

  g_ptr_array_add (ar, NULL);

  g_strfreev (*argv);
  *argv = (char **)g_ptr_array_free (g_steal_pointer (&ar), FALSE);
}

static void
foundry_compile_commands_filter_vala (FoundryCompileCommands   *self,
                                      const CompileInfo        *info,
                                      char                   ***argv)
{
  GPtrArray *ar;

  g_assert (FOUNDRY_IS_COMPILE_COMMANDS (self));
  g_assert (info != NULL);
  g_assert (argv != NULL);

  if (*argv == NULL)
    return;

  ar = g_ptr_array_new ();

  for (guint i = 0; (*argv)[i] != NULL; i++)
    {
      const char *param = (*argv)[i];
      const char *next = (*argv)[i+1];

      if (g_str_has_prefix (param, "--pkg=") ||
          g_str_has_prefix (param, "--target-glib=") ||
          !!strstr (param, ".vapi"))
        {
          g_ptr_array_add (ar, g_strdup (param));
        }
      else if (g_str_has_prefix (param, "--vapidir=") ||
               g_str_has_prefix (param, "--girdir=") ||
               g_str_has_prefix (param, "--metadatadir="))
        {
          g_autofree char *resolved = NULL;
          char *eq = strchr (param, '=');

          next = eq + 1;
          *eq = '\0';

          resolved = foundry_compile_commands_resolve (self, info, next);
          g_ptr_array_add (ar, g_strdup_printf ("%s=%s", param, resolved));
        }
      else if (next != NULL &&
               (g_str_has_prefix (param, "--pkg") ||
                g_str_has_prefix (param, "--target-glib")))
        {
          g_ptr_array_add (ar, g_strdup (param));
          g_ptr_array_add (ar, g_strdup (next));
          i++;
        }
      else if (next != NULL &&
               (g_str_has_prefix (param, "--vapidir") ||
                g_str_has_prefix (param, "--girdir") ||
                g_str_has_prefix (param, "--metadatadir")))
        {
          g_ptr_array_add (ar, g_strdup (param));
          g_ptr_array_add (ar, foundry_compile_commands_resolve (self, info, next));
          i++;
        }
    }

  g_strfreev (*argv);

  g_ptr_array_add (ar, NULL);
  *argv = (char **)g_ptr_array_free (ar, FALSE);
}

static const CompileInfo *
find_with_alternates (FoundryCompileCommands *self,
                      GFile                  *file)
{
  const CompileInfo *info;

  g_assert (FOUNDRY_IS_COMPILE_COMMANDS (self));
  g_assert (G_IS_FILE (file));

  if (self->info_by_file == NULL)
    return NULL;

  if (NULL != (info = g_hash_table_lookup (self->info_by_file, file)))
    return info;

  {
    g_autofree char *path = g_file_get_path (file);
    char *dot = strrchr (path, '.');
    gsize len = strlen (path);

    if (g_str_has_suffix (path, "-private.h"))
      {
        g_autofree char *other_path = NULL;
        g_autoptr(GFile) other = NULL;

        path[len - strlen ("-private.h")] = 0;

        other_path = g_strconcat (path, ".c", NULL);
        other = g_file_new_for_path (other_path);

        if (NULL != (info = g_hash_table_lookup (self->info_by_file, other)))
          return info;
      }
    else if (path_is_c_like (dot) || path_is_cpp_like (dot))
      {
        static const char *tries[] = { ".c", ".cc", ".cpp", ".cxx", ".c++" };

        *dot = 0;

        for (guint i = 0; i < G_N_ELEMENTS (tries); i++)
          {
            g_autofree char *other_path = g_strconcat (path, tries[i], NULL);
            g_autoptr(GFile) other = g_file_new_for_path (other_path);

            if ((info = g_hash_table_lookup (self->info_by_file, other)))
              return info;
          }
      }
  }

  return NULL;
}

/**
 * foundry_compile_commands_lookup:
 * @self: An #FoundryCompileCommands
 * @file: a #GFile representing the file to lookup
 * @system_includes: system include dirs if any
 * @directory: (out) (optional) (transfer full): A location for a #GFile, or %NULL
 * @error: A location for a #GError, or %NULL
 *
 * Locates the commands to compile the @file requested.
 *
 * If @directory is non-NULL, then the directory to run the command from
 * is placed in @directory.
 *
 * Returns: (nullable) (transfer full): A string array or %NULL if
 *   there was a failure to locate or parse the command.
 */
char **
foundry_compile_commands_lookup (FoundryCompileCommands  *self,
                                 GFile                   *file,
                                 const char * const      *system_includes,
                                 GFile                  **directory,
                                 GError                 **error)
{
  g_autofree char *base = NULL;
  const CompileInfo *info;
  const char *dot;

  g_return_val_if_fail (FOUNDRY_IS_COMPILE_COMMANDS (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  base = g_file_get_basename (file);
  dot = strrchr (base, '.');

  if (NULL != (info = find_with_alternates (self, file)))
    {
      g_auto(GStrv) argv = NULL;
      gint argc = 0;

      if (!g_shell_parse_argv (info->command, &argc, &argv, error))
        return NULL;

      if (path_is_c_like (dot) || path_is_cpp_like (dot))
        foundry_compile_commands_filter_c (self, info, system_includes, &argv);
      else if (suffix_is_vala (dot))
        foundry_compile_commands_filter_vala (self, info, &argv);

      if (directory != NULL)
        *directory = g_file_dup (info->directory);

      return g_steal_pointer (&argv);
    }

  /*
   * Some compile-commands databases will give us info about .vala, but there
   * may only be a single valac command to run. While we parsed the JSON
   * document we stored information about each of the Vala files in a special
   * list for exactly this purpose.
   */
  if (foundry_str_equal0 (dot, ".vala") && self->vala_info != NULL)
    {
      for (guint i = 0; i < self->vala_info->len; i++)
        {
          g_auto(GStrv) argv = NULL;
          gint argc = 0;

          info = g_ptr_array_index (self->vala_info, i);

          if (!g_shell_parse_argv (info->command, &argc, &argv, NULL))
            continue;

          foundry_compile_commands_filter_vala (self, info, &argv);

          if (directory != NULL)
            *directory = g_object_ref (info->directory);

          return g_steal_pointer (&argv);
        }
    }

  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_FOUND,
                       "Failed to locate command for requested file");

  return NULL;
}
