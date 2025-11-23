/* test-git-commit-builder-gtk.c
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

#include <foundry.h>
#include <foundry-git-commit-builder.h>
#include <gtk/gtk.h>

static GMainLoop *main_loop;
static const char *project_dir;
static GFile *project_dir_file = NULL;

static void
setup_row (GtkSignalListItemFactory *factory,
          GtkListItem              *item,
          gpointer                  user_data)
{
  gtk_list_item_set_child (item,
                           g_object_new (GTK_TYPE_LABEL,
                                        "xalign", 0.f,
                                        NULL));
}

static void
bind_row (GtkSignalListItemFactory *factory,
         GtkListItem              *item,
         gpointer                  user_data)
{
  GFile *file = gtk_list_item_get_item (item);
  GtkLabel *label = GTK_LABEL (gtk_list_item_get_child (item));
  g_autofree char *relative_path = g_file_get_relative_path (project_dir_file, file);

  if (relative_path == NULL)
    relative_path = g_file_get_path (file);

  gtk_label_set_label (label, relative_path);
}

static DexFuture *
main_fiber (gpointer data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryVcsManager) vcs_manager = NULL;
  g_autoptr(FoundryVcs) vcs = NULL;
  g_autoptr(FoundryGitVcs) git_vcs = NULL;
  g_autoptr(FoundryGitCommitBuilder) commit_builder = NULL;
  g_autoptr(GtkListItemFactory) factory = NULL;
  g_autoptr(GtkSelectionModel) untracked_model = NULL;
  g_autoptr(GtkSelectionModel) unstaged_model = NULL;
  g_autoptr(GtkSelectionModel) staged_model = NULL;
  g_autoptr(GListModel) untracked_list = NULL;
  g_autoptr(GListModel) unstaged_list = NULL;
  g_autoptr(GListModel) staged_list = NULL;
  g_autofree char *foundry_dir = NULL;
  GtkScrolledWindow *untracked_scroller;
  GtkScrolledWindow *unstaged_scroller;
  GtkScrolledWindow *staged_scroller;
  GtkListView *untracked_listview;
  GtkListView *unstaged_listview;
  GtkListView *staged_listview;
  GtkBox *box;
  GtkWindow *window;

  dex_await (foundry_init (), NULL);

  if (!(foundry_dir = dex_await_string (foundry_context_discover (project_dir, NULL), &error)))
    g_error ("%s", error->message);

  if (!(context = dex_await_object (foundry_context_new (foundry_dir, project_dir, FOUNDRY_CONTEXT_FLAGS_NONE, NULL), &error)))
    g_error ("%s", error->message);

  vcs_manager = foundry_context_dup_vcs_manager (context);
  vcs = foundry_vcs_manager_dup_vcs (vcs_manager);

  if (vcs == NULL || !FOUNDRY_IS_GIT_VCS (vcs))
    g_error ("No Git VCS found for project");

  git_vcs = FOUNDRY_GIT_VCS (g_object_ref (vcs));

  if (!(commit_builder = dex_await_object (foundry_git_commit_builder_new (git_vcs, NULL, 0), &error)))
    g_error ("%s", error->message);

  project_dir_file = g_file_new_for_path (project_dir);

  window = g_object_new (GTK_TYPE_WINDOW,
                         "default-width", 400,
                         "default-height", 600,
                         NULL);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      NULL);
  gtk_window_set_child (window, GTK_WIDGET (box));

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_row), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_row), NULL);

  gtk_box_append (box, g_object_new (GTK_TYPE_LABEL,
                                     "label", "Untracked",
                                     "xalign", 0.f,
                                     "margin-start", 6,
                                     "margin-top", 6,
                                     NULL));

  untracked_list = foundry_git_commit_builder_list_untracked (commit_builder);
  untracked_model = GTK_SELECTION_MODEL (gtk_no_selection_new (g_object_ref (untracked_list)));
  untracked_listview = g_object_new (GTK_TYPE_LIST_VIEW,
                                     "factory", factory,
                                     "model", untracked_model,
                                     NULL);
  untracked_scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                     "vexpand", TRUE,
                                     NULL);
  gtk_scrolled_window_set_child (untracked_scroller, GTK_WIDGET (untracked_listview));
  gtk_box_append (box, GTK_WIDGET (untracked_scroller));

  gtk_box_append (box, g_object_new (GTK_TYPE_LABEL,
                                     "label", "Unstaged",
                                     "xalign", 0.f,
                                     "margin-start", 6,
                                     "margin-top", 6,
                                     NULL));

  unstaged_list = foundry_git_commit_builder_list_unstaged (commit_builder);
  unstaged_model = GTK_SELECTION_MODEL (gtk_no_selection_new (g_object_ref (unstaged_list)));
  unstaged_listview = g_object_new (GTK_TYPE_LIST_VIEW,
                                    "factory", factory,
                                    "model", unstaged_model,
                                    NULL);
  unstaged_scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                    "vexpand", TRUE,
                                    NULL);
  gtk_scrolled_window_set_child (unstaged_scroller, GTK_WIDGET (unstaged_listview));
  gtk_box_append (box, GTK_WIDGET (unstaged_scroller));

  gtk_box_append (box, g_object_new (GTK_TYPE_LABEL,
                                     "label", "Staged",
                                     "xalign", 0.f,
                                     "margin-start", 6,
                                     "margin-top", 6,
                                     NULL));

  staged_list = foundry_git_commit_builder_list_staged (commit_builder);
  staged_model = GTK_SELECTION_MODEL (gtk_no_selection_new (g_object_ref (staged_list)));
  staged_listview = g_object_new (GTK_TYPE_LIST_VIEW,
                                  "factory", factory,
                                  "model", staged_model,
                                  NULL);
  staged_scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                   "vexpand", TRUE,
                                   NULL);
  gtk_scrolled_window_set_child (staged_scroller, GTK_WIDGET (staged_listview));
  gtk_box_append (box, GTK_WIDGET (staged_scroller));

  g_signal_connect_swapped (window,
                            "close-request",
                            G_CALLBACK (g_main_loop_quit),
                            main_loop);
  gtk_window_present (window);

  return NULL;
}

int
main (int   argc,
      char *argv[])
{
  if (argc != 2)
    {
      g_printerr ("usage: %s PROJECT_DIR\n", argv[0]);
      return 1;
    }

  project_dir = argv[1];

  gtk_init ();

  main_loop = g_main_loop_new (NULL, FALSE);
  dex_future_disown (dex_scheduler_spawn (NULL, 0, main_fiber, NULL, NULL));
  g_main_loop_run (main_loop);

  return 0;
}
