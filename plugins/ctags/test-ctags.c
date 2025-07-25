/* test-ctags.c
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

#include "plugin-ctags-file.h"

static char **real_argv;
static int real_argc;

static DexFuture *
load_fiber (gpointer data)
{
  GMainLoop *main_loop = data;

  for (int i = 1; i < real_argc; i++)
    {
      g_autoptr(GFile) file = g_file_new_for_path (real_argv[i]);
      g_autoptr(GError) error = NULL;
      g_autoptr(PluginCtagsFile) ctags = dex_await_object (plugin_ctags_file_new (file), &error);
      gsize size = plugin_ctags_file_get_size (ctags);

      g_assert_no_error (error);

      for (gsize j = 0; j < size; j++)
        {
          g_autofree char *name = plugin_ctags_file_dup_name (ctags, j);
          g_autofree char *path = plugin_ctags_file_dup_path (ctags, j);
          g_autofree char *pattern = plugin_ctags_file_dup_pattern (ctags, j);
          g_autofree char *keyval = plugin_ctags_file_dup_keyval (ctags, j);

          g_print ("`%s` `%s` `%s` `%s`\n", name, path, pattern, keyval);
        }
    }

  g_main_loop_quit (main_loop);

  return dex_future_new_true ();
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);

  real_argv = argv;
  real_argc = argc;

  dex_init ();

  dex_future_disown (dex_scheduler_spawn (NULL, 0, load_fiber, main_loop, NULL));
  g_main_loop_run (main_loop);

  return 0;
}
