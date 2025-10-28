/* test-vcs-clone-gtk.c
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

#include <glib-unix.h>

#include <foundry.h>
#include <foundry-gtk.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

static GMainLoop *main_loop;
static const char *clone_uri;
static const char *destination_dir;
static const char *branch_name;

static DexFuture *
main_fiber (gpointer data)
{
  g_autoptr(FoundryGitCloner) cloner = NULL;
  g_autoptr(FoundryOperation) operation = NULL;
  g_autoptr(FoundryGitUri) uri = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) final_dir = NULL;
  g_autoptr(VtePty) pty = NULL;
  g_autofd int master_fd = -1;
  g_autofd int producer_fd = -1;
  GtkWidget *window;
  GtkWidget *scroll;
  GtkWidget *view;

  dex_await (foundry_init (), NULL);

  if (!(uri = foundry_git_uri_new (clone_uri)))
    {
      g_printerr ("Invalid URI: `%s`\n", clone_uri);
      g_main_loop_quit (main_loop);
      return dex_future_new_true ();
    }

  cloner = foundry_git_cloner_new ();
  foundry_git_cloner_set_uri (cloner, clone_uri);

  if (destination_dir)
    {
      if (!g_path_is_absolute (destination_dir))
        {
          g_printerr ("Expected absolute directory but got `%s`\n", destination_dir);
          g_main_loop_quit (main_loop);
          return dex_future_new_true ();
        }

      file = g_file_new_for_path (destination_dir);
      final_dir = g_file_get_child (file, foundry_git_uri_get_clone_name (uri));
    }
  else
    {
      g_autofree char *cwd = g_get_current_dir ();
      file = g_file_new_for_path (cwd);
      final_dir = g_file_get_child (file, foundry_git_uri_get_clone_name (uri));
    }

  foundry_git_cloner_set_directory (cloner, final_dir);

  if (branch_name)
    foundry_git_cloner_set_remote_branch_name (cloner, branch_name);

  window = g_object_new (GTK_TYPE_WINDOW,
                         "default-width", 800,
                         "default-height", 600,
                         "title", "VCS Clone Test",
                         NULL);

  scroll = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                         "hscrollbar-policy", GTK_POLICY_NEVER,
                         "propagate-natural-height", TRUE,
                         "propagate-natural-width", TRUE,
                         NULL);

  view = foundry_terminal_new ();
  vte_terminal_set_enable_fallback_scrolling (VTE_TERMINAL (view), FALSE);
  vte_terminal_set_scroll_unit_is_pixels (VTE_TERMINAL (view), TRUE);

  pty = vte_pty_new_sync (VTE_PTY_DEFAULT, NULL, NULL);
  vte_terminal_set_pty (VTE_TERMINAL (view), pty);

  /* Get the master PTY fd */
  master_fd = vte_pty_get_fd (pty);

  /* Create a producer (slave) PTY from the master PTY */
  producer_fd = foundry_pty_create_producer (master_fd, TRUE, &error);
  if (producer_fd == -1)
    {
      g_printerr ("Failed to create slave PTY: %s\n", error->message);
      g_main_loop_quit (main_loop);
      return dex_future_new_true ();
    }

  gtk_window_set_child (GTK_WINDOW (window), scroll);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), view);

  g_signal_connect_swapped (window,
                            "close-request",
                            G_CALLBACK (g_main_loop_quit),
                            main_loop);

  gtk_window_present (GTK_WINDOW (window));

  operation = foundry_operation_new ();

  g_print ("Cloning %s into %s\n", clone_uri, g_file_peek_path (final_dir));
  if (branch_name)
    g_print ("Checking out branch: %s\n", branch_name);

  if (!dex_await (foundry_git_cloner_clone (cloner, producer_fd, operation), &error))
    {
      g_printerr ("Clone failed: %s\n", error->message);
      g_main_loop_quit (main_loop);
      return dex_future_new_true ();
    }

  g_print ("Clone completed successfully to: %s\n", g_file_peek_path (final_dir));

  return dex_future_new_true ();
}

int
main (int   argc,
      char *argv[])
{
  if (argc < 2 || argc > 4)
    {
      g_printerr ("usage: %s URI [DIRECTORY] [BRANCH]\n", argv[0]);
      return 1;
    }

  clone_uri = argv[1];
  destination_dir = argc > 2 ? argv[2] : NULL;
  branch_name = argc > 3 ? argv[3] : NULL;

  dex_init ();
  gtk_init ();

  dex_future_disown (foundry_init ());

  foundry_gtk_init ();

  main_loop = g_main_loop_new (NULL, FALSE);
  dex_future_disown (dex_scheduler_spawn (NULL, 0, main_fiber, NULL, NULL));
  g_main_loop_run (main_loop);

  return 0;
}
