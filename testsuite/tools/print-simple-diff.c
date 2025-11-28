/* print-simple-diff.c
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

static GMainLoop *main_loop;
static const char *project_directory;

static void
print_delta (FoundryVcsDelta *delta)
{
  g_autofree char *diff_text = NULL;
  GError *error = NULL;

  diff_text = dex_await_string (foundry_vcs_delta_serialize (delta, 3), &error);
  if (error != NULL)
    {
      g_printerr ("Error serializing delta: %s\n", error->message);
      g_clear_error (&error);
      return;
    }

  if (diff_text != NULL)
    g_print ("%s", diff_text);
}

static DexFuture *
main_fiber (gpointer data)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryVcsManager) vcs_manager = NULL;
  g_autoptr(FoundryGitVcs) git_vcs = NULL;
  g_autoptr(FoundryGitCommitBuilder) builder = NULL;
  g_autoptr(GListModel) unstaged_files = NULL;
  g_autoptr(GFile) project_dir = NULL;
  g_autofree char *foundry_dir = NULL;
  GError *error = NULL;

  dex_await (foundry_init (), NULL);

  project_dir = g_file_new_for_path (project_directory);
  foundry_dir = dex_await_string (foundry_context_discover (project_directory, NULL), &error);
  if (error != NULL)
    {
      g_printerr ("Error discovering foundry directory: %s\n", error->message);
      g_main_loop_quit (main_loop);
      return NULL;
    }

  context = dex_await_object (foundry_context_new (foundry_dir, project_directory, 0, NULL), &error);
  if (error != NULL)
    {
      g_printerr ("Error creating context: %s\n", error->message);
      g_main_loop_quit (main_loop);
      return NULL;
    }

  vcs_manager = foundry_context_dup_vcs_manager (context);
  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (vcs_manager)), &error))
    {
      g_printerr ("Error waiting for VCS manager: %s\n", error->message);
      g_main_loop_quit (main_loop);
      return NULL;
    }

  git_vcs = FOUNDRY_GIT_VCS (foundry_vcs_manager_dup_vcs (vcs_manager));
  if (git_vcs == NULL || !FOUNDRY_IS_GIT_VCS (git_vcs))
    {
      g_printerr ("No Git VCS found for project\n");
      g_main_loop_quit (main_loop);
      return NULL;
    }

  builder = dex_await_object (foundry_git_commit_builder_new (git_vcs, NULL, 3), &error);
  if (error != NULL)
    {
      g_printerr ("Error creating commit builder: %s\n", error->message);
      g_main_loop_quit (main_loop);
      return NULL;
    }

  unstaged_files = foundry_git_commit_builder_list_unstaged (builder);

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (unstaged_files)); i++)
    {
      g_autoptr(FoundryGitStatusEntry) entry = NULL;
      g_autoptr(GFile) file = NULL;
      g_autofree char *path = NULL;
      g_autoptr(FoundryVcsDelta) delta = NULL;

      if (!(entry = g_list_model_get_item (G_LIST_MODEL (unstaged_files), i)))
        continue;

      if (!(path = foundry_git_status_entry_dup_path (entry)))
        continue;

      if (!(file = g_file_resolve_relative_path (project_dir, path)))
        continue;

      delta = dex_await_object (foundry_git_commit_builder_load_unstaged_delta (builder, file), &error);
      if (error != NULL)
        {
          g_printerr ("Error loading delta for file: %s\n", error->message);
          g_clear_error (&error);
          continue;
        }

      if (delta != NULL)
        print_delta (delta);
    }

  g_main_loop_quit (main_loop);

  return NULL;
}

int
main (int   argc,
      char *argv[])
{
  if (argc != 2)
    {
      g_printerr ("usage: %s <project_directory>\n", argv[0]);
      return 1;
    }

  project_directory = argv[1];

  main_loop = g_main_loop_new (NULL, FALSE);
  dex_future_disown (dex_scheduler_spawn (NULL, 0, main_fiber, NULL, NULL));
  g_main_loop_run (main_loop);

  return 0;
}
