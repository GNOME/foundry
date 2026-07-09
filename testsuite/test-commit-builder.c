/* test-commit-builder.c
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

#include <foundry.h>

#include "test-util.h"

static void
wait_for_commit_builder (FoundryGitCommitBuilder *commit_builder)
{
  gint64 deadline;

  g_assert (FOUNDRY_IS_GIT_COMMIT_BUILDER (commit_builder));

  deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND * 5;

  while (foundry_git_commit_builder_get_busy (commit_builder))
    {
      g_assert_cmpint (g_get_monotonic_time (), <, deadline);
      dex_await (dex_timeout_new_msec (10), NULL);
    }
}

static void
test_commit_builder_fiber (void)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryVcsManager) vcs_manager = NULL;
  g_autoptr(FoundryVcsProvider) git_provider = NULL;
  g_autoptr(FoundryGitCommitBuilder) commit_builder = NULL;
  g_autoptr(FoundryGitCommit) head_commit = NULL;
  g_autoptr(FoundryGitVcs) git_vcs = NULL;
  g_autoptr(GFile) project_dir = NULL;
  g_autoptr(GFile) file1 = NULL;
  g_autoptr(GFile) file2 = NULL;
  g_autoptr(GFile) file3 = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *tmpdir = NULL;
  g_autofree char *foundry_dir = NULL;
  g_autofree char *commit_message = NULL;
  g_autofree char *builder_message = NULL;
  g_autofree char *commit_title = NULL;
  g_autofree char *head_id = NULL;
  g_autofree char *amended_head_id = NULL;
  g_autofree char *file1_path = NULL;
  g_autofree char *file2_path = NULL;
  g_autofree char *file3_path = NULL;
  gboolean ret;

  /* Create temporary directory */
  tmpdir = g_build_filename (g_get_tmp_dir (), "test-foundry-git-XXXXXX", NULL);
  g_assert_nonnull (g_mkdtemp (tmpdir));
  g_assert_true (g_file_test (tmpdir, G_FILE_TEST_IS_DIR));

  foundry_dir = g_build_filename (tmpdir, ".foundry", NULL);

  /* Create context with CREATE flag */
  context = dex_await_object (foundry_context_new (foundry_dir, tmpdir, FOUNDRY_CONTEXT_FLAGS_CREATE, NULL), &error);
  g_assert_no_error (error);
  g_assert_nonnull (context);

  /* Get VCS manager */
  vcs_manager = foundry_context_dup_vcs_manager (context);
  g_assert_nonnull (vcs_manager);

  /* Find git provider */
  git_provider = foundry_vcs_manager_find_provider (vcs_manager, "git");
  g_assert_nonnull (git_provider);

  /* Initialize git repository */
  dex_await (foundry_vcs_provider_initialize (git_provider), &error);
  g_assert_no_error (error);

  /* Check that VCS manager now has git vcs */
  /* The initialize function chains callbacks that reload and set the VCS,
   * so it should be available after awaiting */
  git_vcs = FOUNDRY_GIT_VCS (foundry_vcs_manager_dup_vcs (vcs_manager));
  g_assert_nonnull (git_vcs);
  g_assert_true (FOUNDRY_IS_GIT_VCS (git_vcs));

  /* Create test files */
  project_dir = foundry_context_dup_project_directory (context);
  g_assert_nonnull (project_dir);

  g_assert_true (g_file_query_exists (project_dir, NULL));

  file1 = g_file_get_child (project_dir, "empty.txt");
  file2 = g_file_get_child (project_dir, "hello.txt");
  file3 = g_file_get_child (project_dir, "world.txt");

  file1_path = g_file_get_path (file1);
  file2_path = g_file_get_path (file2);
  file3_path = g_file_get_path (file3);

  /* Create empty file */
  ret = g_file_set_contents (file1_path, "", 0, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  /* Create file with content */
  ret = g_file_set_contents (file2_path, "Hello, World!\n", -1, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  /* Create another file with content */
  ret = g_file_set_contents (file3_path, "Test content\n", -1, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  /* Create commit builder */
  commit_builder = dex_await_object (foundry_git_commit_builder_new (git_vcs, NULL, 3), &error);
  g_assert_no_error (error);
  g_assert_nonnull (commit_builder);
  g_assert_false (foundry_git_commit_builder_get_amend (commit_builder));
  g_assert_false (foundry_git_commit_builder_get_busy (commit_builder));
  g_assert_false (foundry_git_commit_builder_get_can_amend (commit_builder));

  /* List untracked files */
  {
    g_autoptr(GListModel) untracked = foundry_git_commit_builder_list_untracked (commit_builder);
    guint n_untracked = g_list_model_get_n_items (untracked);

    g_assert_nonnull (untracked);
    g_assert_cmpint (n_untracked, >=, 3);

    /* Stage each untracked file. Walk backwards since the model will
     * update to remove the item as part of staging.
     */
    for (guint i = n_untracked; i > 0; i--)
      {
        g_autoptr(FoundryGitStatusEntry) entry = g_list_model_get_item (untracked, i - 1);
        g_autoptr(GFile) file = NULL;
        g_autofree char *relative_path = NULL;
        g_autofree char *path = NULL;

        g_assert_nonnull (entry);
        relative_path = foundry_git_status_entry_dup_path (entry);
        g_assert_nonnull (relative_path);

        file = g_file_get_child (project_dir, relative_path);
        g_assert_nonnull (file);

        path = g_file_get_path (file);

        /* Stage the file if it's one of our test files */
        if (g_strcmp0 (path, file1_path) == 0 ||
            g_strcmp0 (path, file2_path) == 0 ||
            g_strcmp0 (path, file3_path) == 0)
          {
            dex_await (foundry_git_commit_builder_stage_file (commit_builder, file), &error);
            g_assert_no_error (error);
          }
      }
  }

  /* Clear signing properties */
  foundry_git_commit_builder_set_signing_key (commit_builder, NULL);
  foundry_git_commit_builder_set_signing_format (commit_builder, NULL);

  /* Set commit message */
  commit_message = g_strdup ("Test commit message");
  foundry_git_commit_builder_set_message (commit_builder, commit_message);

  /* Make commit */
  dex_await (foundry_git_commit_builder_commit (commit_builder), &error);
  g_assert_no_error (error);

  /* Load head commit */
  head_commit = dex_await_object (foundry_vcs_load_tip (FOUNDRY_VCS (git_vcs)), &error);
  g_assert_no_error (error);
  g_assert_nonnull (head_commit);

  /* Verify commit message matches */
  commit_title = foundry_vcs_commit_dup_title (FOUNDRY_VCS_COMMIT (head_commit));
  g_assert_cmpstr (commit_title, ==, commit_message);
  head_id = foundry_vcs_commit_dup_id (FOUNDRY_VCS_COMMIT (head_commit));
  g_assert_nonnull (head_id);
  g_assert_cmpint (foundry_vcs_commit_get_n_parents (FOUNDRY_VCS_COMMIT (head_commit)), ==, 0);

  g_clear_object (&commit_builder);
  g_clear_object (&head_commit);
  g_clear_pointer (&commit_title, g_free);

  ret = g_file_set_contents (file2_path, "Hello, Amended!\n", -1, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  commit_builder = dex_await_object (foundry_git_commit_builder_new (git_vcs, NULL, 3), &error);
  g_assert_no_error (error);
  g_assert_nonnull (commit_builder);
  g_assert_true (foundry_git_commit_builder_get_can_amend (commit_builder));

  {
    g_autoptr(FoundryVcsDelta) staged_delta = NULL;

    staged_delta = dex_await_object (foundry_git_commit_builder_load_staged_delta (commit_builder, file2), &error);
    g_assert_null (staged_delta);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
    g_clear_error (&error);
  }

  dex_await (foundry_git_commit_builder_stage_file (commit_builder, file2), &error);
  g_assert_no_error (error);

  foundry_git_commit_builder_set_message (commit_builder, "Replacement message");
  g_assert_true (foundry_git_commit_builder_get_can_commit (commit_builder));

  {
    g_autoptr(GListModel) staged = foundry_git_commit_builder_list_staged (commit_builder);
    guint normal_staged;
    guint amend_staged;

    normal_staged = g_list_model_get_n_items (staged);
    g_assert_cmpuint (normal_staged, ==, 1);

    foundry_git_commit_builder_set_amend (commit_builder, TRUE);
    g_assert_true (foundry_git_commit_builder_get_amend (commit_builder));
    g_assert_true (foundry_git_commit_builder_get_busy (commit_builder));
    g_assert_false (foundry_git_commit_builder_get_can_commit (commit_builder));

    wait_for_commit_builder (commit_builder);

    builder_message = foundry_git_commit_builder_dup_message (commit_builder);
    g_assert_cmpstr (builder_message, ==, commit_message);

    g_assert_false (foundry_git_commit_builder_get_busy (commit_builder));
    g_assert_true (foundry_git_commit_builder_get_can_commit (commit_builder));

    amend_staged = g_list_model_get_n_items (staged);
    g_assert_cmpuint (amend_staged, >, normal_staged);
  }

  dex_await (foundry_git_commit_builder_commit (commit_builder), &error);
  g_assert_no_error (error);

  head_commit = dex_await_object (foundry_vcs_load_tip (FOUNDRY_VCS (git_vcs)), &error);
  g_assert_no_error (error);
  g_assert_nonnull (head_commit);

  commit_title = foundry_vcs_commit_dup_title (FOUNDRY_VCS_COMMIT (head_commit));
  g_assert_cmpstr (commit_title, ==, commit_message);

  amended_head_id = foundry_vcs_commit_dup_id (FOUNDRY_VCS_COMMIT (head_commit));
  g_assert_nonnull (amended_head_id);
  g_assert_cmpstr (amended_head_id, !=, head_id);
  g_assert_cmpint (foundry_vcs_commit_get_n_parents (FOUNDRY_VCS_COMMIT (head_commit)), ==, 0);

  /* Cleanup */
  rm_rf (tmpdir);
}

static void
test_commit_builder (void)
{
  test_from_fiber (test_commit_builder_fiber);
}

int
main (int argc,
      char *argv[])
{
  dex_init ();

  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Foundry/Git/CommitBuilder", test_commit_builder);

  return g_test_run ();
}
