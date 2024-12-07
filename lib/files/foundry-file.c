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
