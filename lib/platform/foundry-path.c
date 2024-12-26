/* foundry-path.c
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

#include <errno.h>
#include <wordexp.h>

#include "foundry-path.h"

/**
 * foundry_path_expand:
 *
 * This function will expand various "shell-like" features of the provided
 * path using the POSIX wordexp(3) function. Command substitution will
 * not be enabled, but path features such as ~user will be expanded.
 *
 * Returns: (transfer full): A newly allocated string containing the
 *   expansion. A copy of the input string upon failure to expand.
 */
char *
foundry_path_expand (const char *path)
{
  wordexp_t state = { 0 };
  char *replace_home = NULL;
  char *ret = NULL;
  char *escaped;
  int r;

  if (path == NULL)
    return NULL;

  /* Special case some path prefixes */
  if (path[0] == '~')
    {
      if (path[1] == 0)
        path = g_get_home_dir ();
      else if (path[1] == G_DIR_SEPARATOR)
        path = replace_home = g_strdup_printf ("%s%s", g_get_home_dir (), &path[1]);
    }
  else if (strncmp (path, "$HOME", 5) == 0)
    {
      if (path[5] == 0)
        path = g_get_home_dir ();
      else if (path[5] == G_DIR_SEPARATOR)
        path = replace_home = g_strdup_printf ("%s%s", g_get_home_dir (), &path[5]);
    }

  escaped = g_shell_quote (path);
  r = wordexp (escaped, &state, WRDE_NOCMD);
  if (r == 0 && state.we_wordc > 0)
    ret = g_strdup (state.we_wordv [0]);
  wordfree (&state);

  if (!g_path_is_absolute (ret))
    {
      g_autofree char *freeme = ret;

      ret = g_build_filename (g_get_home_dir (), freeme, NULL);
    }

  g_free (replace_home);
  g_free (escaped);

  return ret;
}

/**
 * foundry_path_collapse:
 *
 * This function will collapse a path that starts with the users home
 * directory into a shorthand notation using ~/ for the home directory.
 *
 * If the path does not have the home directory as a prefix, it will
 * simply return a copy of @path.
 *
 * Returns: (transfer full): A new path, possibly collapsed.
 */
char *
foundry_path_collapse (const char *path)
{
  g_autofree char *expanded = NULL;

  if (path == NULL)
    return NULL;

  expanded = foundry_path_expand (path);

  if (g_str_has_prefix (expanded, g_get_home_dir ()))
    return g_build_filename ("~",
                             expanded + strlen (g_get_home_dir ()),
                             NULL);

  return g_steal_pointer (&expanded);
}

typedef struct _MkdirWithParents
{
  DexPromise *promise;
  char *path;
  int mode;
} MkdirWithParents;

static void
foundry_mkdir_with_parents_func (gpointer data)
{
  MkdirWithParents *state = data;

  if (g_mkdir_with_parents (state->path, state->mode) == -1)
    {
      int errsv = errno;

      dex_promise_reject (state->promise,
                          g_error_new_literal (G_FILE_ERROR,
                                               g_file_error_from_errno (errsv),
                                               g_strerror (errsv)));
    }
  else
    {
      dex_promise_resolve_int (state->promise, 0);
    }

  g_clear_pointer (&state->path, g_free);
  dex_clear (&state->promise);
  g_free (state);
}

/**
 * foundry_mkdir_with_parents:
 * @path: a path to a directory to create
 * @mode: the mode for the directory such as `0750`
 *
 * Similar to g_mkdir_with_parents() but runs on a thread pool thread.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to 0
 *   if successful, otherwise rejects with error.
 */
DexFuture *
foundry_mkdir_with_parents (const char *path,
                            int         mode)
{
  MkdirWithParents *state;
  DexPromise *promise;

  dex_return_error_if_fail (path != NULL);

  promise = dex_promise_new ();

  state = g_new0 (MkdirWithParents, 1);
  state->promise = dex_ref (promise);
  state->path = g_strdup (path);
  state->mode = mode;

  dex_scheduler_push (dex_thread_pool_scheduler_get_default (),
                      foundry_mkdir_with_parents_func,
                      state);

  return DEX_FUTURE (promise);
}
