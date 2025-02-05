/* foundry-file.c
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

#include "foundry-file.h"
#include "foundry-util.h"

typedef struct _FindInAncestors
{
  GFile *file;
  char  *name;
} FindInAncestors;

static void
find_in_ancestors_free (gpointer data)
{
  FindInAncestors *state = data;

  g_clear_object (&state->file);
  g_clear_pointer (&state->name, g_free);
  g_free (state);
}

static DexFuture *
foundry_file_find_in_ancestors_fiber (gpointer data)
{
  FindInAncestors *state = data;
  GFile *parent;

  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->file));
  g_assert (state->name != NULL);

  parent = g_file_get_parent (state->file);

  while (parent != NULL)
    {
      g_autoptr(GFile) child = g_file_get_child (parent, state->name);
      g_autoptr(GFile) old_parent = NULL;

      if (dex_await_boolean (dex_file_query_exists (child), NULL))
        return dex_future_new_true ();

      old_parent = g_steal_pointer (&parent);
      parent = g_file_get_parent (old_parent);
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_FOUND,
                                "Failed to locate \"%s\" within ancestors",
                                state->name);
}

/**
 * foundry_file_find_in_ancestors:
 * @file: a [iface@Gio.File]
 * @name: the name of the file to find in the ancestors such as ".gitignore"
 *
 * Locates @name within any of the ancestors of @file up to the root of
 * the filesystem.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [iface@Gio.File] or rejects with error,
 */
DexFuture *
foundry_file_find_in_ancestors (GFile      *file,
                                const char *name)
{
  FindInAncestors *state;

  dex_return_error_if_fail (G_IS_FILE (file));
  dex_return_error_if_fail (name != NULL);

  state = g_new0 (FindInAncestors, 1);
  state->file = g_object_ref (file);
  state->name = g_strdup (name);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_file_find_in_ancestors_fiber,
                              state,
                              (GDestroyNotify) find_in_ancestors_free);
}

static gboolean
is_internally_ignored (const char *name)
{
  if (name == NULL)
    return TRUE;

  if (g_str_has_prefix (name, ".goutputstream-"))
    return TRUE;

  if (g_str_has_suffix (name, "~"))
    return TRUE;

  if (g_str_has_suffix (name, ".min.js") || strstr (name, ".min.js.") != NULL)
    return TRUE;

  return FALSE;
}

static void
populate_descendants_matching (GFile        *file,
                               GCancellable *cancellable,
                               GPtrArray    *results,
                               GPatternSpec *spec,
                               guint         depth)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GPtrArray) children = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (results != NULL);
  g_assert (spec != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (depth == 0)
    return;

  enumerator = g_file_enumerate_children (file,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NONE,
                                          cancellable,
                                          NULL);

  if (enumerator == NULL)
    return;

  for (;;)
    {
      g_autoptr(GFileInfo) info = g_file_enumerator_next_file (enumerator, cancellable, NULL);
      const gchar *name;
      GFileType file_type;

      if (info == NULL)
        break;

      name = g_file_info_get_name (info);
      file_type = g_file_info_get_file_type (info);

      if (is_internally_ignored (name))
        continue;

      if (g_pattern_spec_match_string (spec, name))
        g_ptr_array_add (results, g_file_enumerator_get_child (enumerator, info));

      if (!g_file_info_get_is_symlink (info) && file_type == G_FILE_TYPE_DIRECTORY)
        {
          if (children == NULL)
            children = g_ptr_array_new_with_free_func (g_object_unref);
          g_ptr_array_add (children, g_file_enumerator_get_child (enumerator, info));
        }
    }

  g_file_enumerator_close (enumerator, cancellable, NULL);

  if (children != NULL)
    {
      for (guint i = 0; i < children->len; i++)
        {
          GFile *child = g_ptr_array_index (children, i);

          populate_descendants_matching (child, cancellable, results, spec, depth - 1);
        }
    }
}

typedef struct _Find
{
  GFile        *file;
  GPatternSpec *spec;
  guint         depth;
} Find;

static DexFuture *
find_matching_fiber (gpointer user_data)
{
  Find *find = user_data;
  g_autoptr(GPtrArray) ar = NULL;

  g_assert (find != NULL);
  g_assert (G_IS_FILE (find->file));
  g_assert (find->spec != NULL);
  g_assert (find->depth > 0);

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  populate_descendants_matching (find->file,
                                 NULL,
                                 ar,
                                 find->spec,
                                 find->depth);

  return dex_future_new_take_boxed (G_TYPE_PTR_ARRAY, g_steal_pointer (&ar));
}

static void
find_free (Find *find)
{
  g_clear_object (&find->file);
  g_clear_pointer (&find->spec, g_pattern_spec_free);
  g_free (find);
}

static DexFuture *
find_matching (GFile        *file,
               GPatternSpec *spec,
               guint         depth)
{
  Find *state;

  g_assert (G_IS_FILE (file));
  g_assert (spec != NULL);
  g_assert (depth > 0);

  state = g_new0 (Find, 1);
  state->file = g_object_ref (file);
  state->spec = g_pattern_spec_copy (spec);
  state->depth = depth;

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              find_matching_fiber,
                              state,
                              (GDestroyNotify) find_free);

}

/**
 * foundry_file_find_with_depth:
 * @file: an [iface@Gio.File]
 * @pattern: the pattern to find
 * @max_depth: the max depth to recurse
 *
 * Locates files starting from @file matching @pattern.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 */
DexFuture *
foundry_file_find_with_depth (GFile       *file,
                              const gchar *pattern,
                              guint        max_depth)
{
  g_autoptr(GPatternSpec) spec = NULL;

  dex_return_error_if_fail (G_IS_FILE (file));
  dex_return_error_if_fail (pattern != NULL);

  if (!(spec = g_pattern_spec_new (pattern)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVAL,
                                  "Invalid pattern");

  if (max_depth == 0)
    max_depth = G_MAXUINT;

  return find_matching (file, spec, max_depth);
}

/**
 * foundry_file_query_exists_nofollow:
 * @self: a [class@Foundry.File]
 *
 * Resolves to true if @file exists.
 *
 * Does not follow symlink.
 *
 * Returns: (transfer full): a future that resolves to a boolean
 */
DexFuture *
foundry_file_query_exists_nofollow (GFile *file)
{
  dex_return_error_if_fail (G_IS_FILE (file));

  return dex_future_then (dex_file_query_info (file,
                                               G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                               G_PRIORITY_DEFAULT),
                          foundry_future_return_true,
                          NULL, NULL);
}

/* NOTE: This requires that file exists */
GFile *
foundry_file_canonicalize (GFile   *file,
                           GError **error)
{
  char *canonical_path = NULL;

  if ((canonical_path = realpath (g_file_peek_path (file), NULL)))
    {
      GFile *ret = g_file_new_for_path (canonical_path);
      free (canonical_path);
      return ret;
    }

  return (gpointer)foundry_set_error_from_errno (error);
}

/* NOTE: This requires both files to exist */
gboolean
foundry_file_is_in (GFile *file,
                    GFile *toplevel)
{
  g_autoptr(GFile) canonical_file = NULL;
  g_autoptr(GFile) canonical_toplevel = NULL;

  if (!(canonical_toplevel = foundry_file_canonicalize (toplevel, NULL)))
    return FALSE;

  if (!(canonical_file = foundry_file_canonicalize (file, NULL)))
    return FALSE;

  return g_file_equal (canonical_file, canonical_toplevel) ||
         g_file_has_prefix (canonical_file, canonical_toplevel);
}
