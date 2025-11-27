/* print-project-diff.c
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

#define DEV_NULL "/dev/null"

#define ANSI_RESET   "\033[0m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_CYAN    "\033[36m"

static GMainLoop *main_loop;
static const char *project_directory;

static void
print_diff_line (FoundryVcsDiffLine *line)
{
  FoundryVcsDiffLineOrigin origin;
  g_autofree char *text = NULL;
  gboolean has_newline;
  gboolean use_color = FALSE;
  const char *color = NULL;

  origin = foundry_vcs_diff_line_get_origin (line);
  text = foundry_vcs_diff_line_dup_text (line);
  has_newline = foundry_vcs_diff_line_get_has_newline (line);

  /* Set color based on line origin */
  switch ((int)origin)
    {
    case FOUNDRY_VCS_DIFF_LINE_ORIGIN_ADDED:
      color = ANSI_GREEN;
      use_color = TRUE;
      break;
    case FOUNDRY_VCS_DIFF_LINE_ORIGIN_DELETED:
      color = ANSI_RED;
      use_color = TRUE;
      break;
    default:
      break;
    }

  if (use_color)
    g_print ("%s", color);
  g_print ("%c%s", (char)origin, text ? text : "");
  if (use_color)
    g_print ("%s", ANSI_RESET);
  /* The text already includes the newline if has_newline is true, so don't add another */
  if (!has_newline)
    g_print ("\n");
}

static void
print_delta (FoundryVcsDelta *delta)
{
  g_autoptr(GListStore) hunks = NULL;
  g_autofree char *old_path = NULL;
  g_autofree char *new_path = NULL;
  FoundryVcsDeltaStatus status;
  GError *error = NULL;

  old_path = foundry_vcs_delta_dup_old_path (delta);
  new_path = foundry_vcs_delta_dup_new_path (delta);
  status = foundry_vcs_delta_get_status (delta);

  /* Print diff header */
  if (status == FOUNDRY_VCS_DELTA_STATUS_RENAMED)
    g_print ("diff --git a/%s b/%s\n", old_path ? old_path : DEV_NULL, new_path ? new_path : DEV_NULL);
  else if (status == FOUNDRY_VCS_DELTA_STATUS_DELETED)
    g_print ("diff --git a/%s b/%s\n", old_path ? old_path : DEV_NULL, DEV_NULL);
  else if (status == FOUNDRY_VCS_DELTA_STATUS_ADDED)
    g_print ("diff --git a/%s b/%s\n", DEV_NULL, new_path ? new_path : DEV_NULL);
  else
    g_print ("diff --git a/%s b/%s\n", old_path ? old_path : DEV_NULL, new_path ? new_path : DEV_NULL);

  if (old_path && new_path && g_strcmp0 (old_path, new_path) != 0)
    g_print ("rename from %s\nrename to %s\n", old_path, new_path);

  /* Get hunks */
  hunks = dex_await_object (foundry_vcs_delta_list_hunks (delta), &error);
  if (error != NULL)
    {
      g_printerr ("Error listing hunks: %s\n", error->message);
      g_clear_error (&error);
      return;
    }

  /* Print each hunk */
  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (hunks)); i++)
    {
      FoundryVcsDiffHunk *hunk;
      g_autoptr(GListStore) lines = NULL;
      g_autofree char *header = NULL;

      hunk = g_list_model_get_item (G_LIST_MODEL (hunks), i);
      header = foundry_vcs_diff_hunk_dup_header (hunk);
      if (header)
        {
          /* Color hunk headers (lines starting with @@) with cyan */
          g_print ("%s%s%s", ANSI_CYAN, header, ANSI_RESET);
        }

      lines = dex_await_object (foundry_vcs_diff_hunk_list_lines (hunk), &error);
      if (error != NULL)
        {
          g_printerr ("Error listing lines: %s\n", error->message);
          g_clear_error (&error);
          g_object_unref (hunk);
          continue;
        }

      for (guint j = 0; j < g_list_model_get_n_items (G_LIST_MODEL (lines)); j++)
        {
          FoundryVcsDiffLine *line;

          line = g_list_model_get_item (G_LIST_MODEL (lines), j);
          print_diff_line (line);
          g_object_unref (line);
        }

      g_object_unref (hunk);
    }
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
