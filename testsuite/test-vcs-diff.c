/* test-vcs-diff.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <glib/gstdio.h>

#include <foundry.h>

#include "test-util.h"

typedef struct _TestRepo
{
  char *tmpdir;
  FoundryContext *context;
  FoundryVcs *vcs;
} TestRepo;

static void
test_repo_clear (TestRepo *repo)
{
  if (repo->tmpdir != NULL)
    {
      rm_rf (repo->tmpdir);
      g_clear_pointer (&repo->tmpdir, g_free);
    }

  g_clear_object (&repo->vcs);
  g_clear_object (&repo->context);
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (TestRepo, test_repo_clear)

static void
run_gitv (const char  *dir,
          const char **args)
{
  g_autoptr(GPtrArray) argv = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *stderr_buf = NULL;
  int status = 0;

  argv = g_ptr_array_new ();
  g_ptr_array_add (argv, (char *)"git");
  g_ptr_array_add (argv, (char *)"-C");
  g_ptr_array_add (argv, (char *)dir);

  for (guint i = 0; args[i] != NULL; i++)
    g_ptr_array_add (argv, (char *)args[i]);

  g_ptr_array_add (argv, NULL);

  g_spawn_sync (NULL,
                (char **)argv->pdata,
                NULL,
                G_SPAWN_SEARCH_PATH,
                NULL,
                NULL,
                NULL,
                &stderr_buf,
                &status,
                &error);

  g_assert_no_error (error);
  g_assert_cmpint (status, ==, 0);
}

static void
run_git (const char *dir,
         const char *arg1,
         const char *arg2,
         const char *arg3,
         const char *arg4)
{
  const char *args[] = { arg1, arg2, arg3, arg4, NULL };

  run_gitv (dir, args);
}

static char *
write_file (TestRepo   *repo,
            const char *relative_path,
            const char *contents)
{
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  gboolean ret;

  path = g_build_filename (repo->tmpdir, relative_path, NULL);
  ret = g_file_set_contents (path, contents, -1, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  return g_steal_pointer (&path);
}

static void
commit_all (TestRepo   *repo,
            const char *message)
{
  run_git (repo->tmpdir, "add", "-A", NULL, NULL);
  run_git (repo->tmpdir, "commit", "-m", message, NULL);
}

static void
test_repo_init (TestRepo *repo)
{
  g_autoptr(FoundryVcsManager) vcs_manager = NULL;
  g_autoptr(FoundryVcsProvider) git_provider = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *foundry_dir = NULL;

  repo->tmpdir = g_build_filename (g_get_tmp_dir (), "test-foundry-vcs-diff-XXXXXX", NULL);
  g_assert_nonnull (g_mkdtemp (repo->tmpdir));

  foundry_dir = g_build_filename (repo->tmpdir, ".foundry", NULL);
  repo->context = dex_await_object (foundry_context_new (foundry_dir,
                                                         repo->tmpdir,
                                                         FOUNDRY_CONTEXT_FLAGS_CREATE,
                                                         NULL),
                                    &error);
  g_assert_no_error (error);
  g_assert_nonnull (repo->context);

  vcs_manager = foundry_context_dup_vcs_manager (repo->context);
  git_provider = foundry_vcs_manager_find_provider (vcs_manager, "git");
  g_assert_nonnull (git_provider);

  dex_await (foundry_vcs_provider_initialize (git_provider), &error);
  g_assert_no_error (error);

  repo->vcs = foundry_vcs_manager_dup_vcs (vcs_manager);
  g_assert_true (FOUNDRY_IS_GIT_VCS (repo->vcs));

  run_git (repo->tmpdir, "config", "user.name", "Foundry Test", NULL);
  run_git (repo->tmpdir, "config", "user.email", "foundry@example.invalid", NULL);
}

static char *
content_to_string (FoundryVcsContent *content)
{
  g_autoptr(GBytes) bytes = NULL;
  gsize len = 0;
  const char *data;

  g_assert_true (foundry_vcs_content_get_present (content));

  bytes = foundry_vcs_content_dup_bytes (content);
  g_assert_nonnull (bytes);

  data = g_bytes_get_data (bytes, &len);

  return g_strndup (data, len);
}

static GListModel *
await_deltas (DexFuture *future)
{
  g_autoptr(FoundryVcsDiff) diff = NULL;
  g_autoptr(GError) error = NULL;

  diff = dex_await_object (future, &error);
  g_assert_no_error (error);
  g_assert_nonnull (diff);

  return dex_await_object (foundry_vcs_diff_list_deltas (diff), &error);
}

static FoundryVcsRevision *
resolve_revision (FoundryVcs *vcs,
                  const char *revspec)
{
  g_autoptr(GError) error = NULL;
  FoundryVcsRevision *revision;

  revision = dex_await_object (foundry_vcs_resolve_revision (vcs, revspec), &error);
  g_assert_no_error (error);
  g_assert_nonnull (revision);

  return revision;
}

static void
assert_serializes_with (FoundryVcsDelta *delta,
                       const char      *needle)
{
  g_autoptr(GError) error = NULL;
  g_autofree char *serialized = NULL;

  serialized = dex_await_string (foundry_vcs_delta_serialize (delta, 3), &error);
  g_assert_no_error (error);
  g_assert_nonnull (serialized);
  g_assert_nonnull (strstr (serialized, needle));
}

static void
test_commit_to_commit_ignores_local_modification (void)
{
  g_auto(TestRepo) repo = { 0 };
  g_autoptr(FoundryVcsRevision) old_revision = NULL;
  g_autoptr(FoundryVcsRevision) new_revision = NULL;
  g_autoptr(GListModel) deltas = NULL;
  g_autoptr(FoundryVcsDelta) delta = NULL;
  g_autoptr(FoundryVcsContent) content = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *text = NULL;

  test_repo_init (&repo);
  commit_all (&repo, "initial");
  write_file (&repo, "tracked.txt", "committed\n");
  commit_all (&repo, "add tracked");
  write_file (&repo, "tracked.txt", "local\n");

  old_revision = resolve_revision (repo.vcs, "HEAD~1");
  new_revision = resolve_revision (repo.vcs, "HEAD");
  deltas = await_deltas (foundry_vcs_diff_full (repo.vcs, old_revision, new_revision, NULL));
  g_assert_cmpuint (g_list_model_get_n_items (deltas), ==, 1);

  delta = g_list_model_get_item (deltas, 0);
  content = dex_await_object (foundry_vcs_delta_open_content (delta, FOUNDRY_VCS_DELTA_SIDE_NEW), &error);
  g_assert_no_error (error);
  text = content_to_string (content);
  g_assert_cmpstr (text, ==, "committed\n");
  assert_serializes_with (delta, "+committed\n");
}

static void
test_commit_to_commit_deletion_ignores_recreated_file (void)
{
  g_auto(TestRepo) repo = { 0 };
  g_autoptr(FoundryVcsRevision) old_revision = NULL;
  g_autoptr(FoundryVcsRevision) new_revision = NULL;
  g_autoptr(GListModel) deltas = NULL;
  g_autoptr(FoundryVcsDelta) delta = NULL;
  g_autoptr(FoundryVcsContent) content = NULL;
  g_autoptr(GError) error = NULL;

  test_repo_init (&repo);
  write_file (&repo, "gone.txt", "old\n");
  commit_all (&repo, "add gone");
  {
    g_autofree char *path = g_build_filename (repo.tmpdir, "gone.txt", NULL);

    g_assert_cmpint (g_remove (path), ==, 0);
  }
  commit_all (&repo, "delete gone");
  write_file (&repo, "gone.txt", "recreated\n");

  old_revision = resolve_revision (repo.vcs, "HEAD~1");
  new_revision = resolve_revision (repo.vcs, "HEAD");
  deltas = await_deltas (foundry_vcs_diff_full (repo.vcs, old_revision, new_revision, NULL));
  g_assert_cmpuint (g_list_model_get_n_items (deltas), ==, 1);

  delta = g_list_model_get_item (deltas, 0);
  g_assert_cmpint (foundry_vcs_delta_get_status (delta), ==, FOUNDRY_VCS_DELTA_STATUS_DELETED);
  content = dex_await_object (foundry_vcs_delta_open_content (delta, FOUNDRY_VCS_DELTA_SIDE_NEW), &error);
  g_assert_no_error (error);
  g_assert_false (foundry_vcs_content_get_present (content));
  assert_serializes_with (delta, "-old\n");
}

static void
test_index_to_worktree_excludes_untracked (void)
{
  g_auto(TestRepo) repo = { 0 };
  g_autoptr(FoundryVcsRevision) index_revision = NULL;
  g_autoptr(FoundryVcsRevision) worktree_revision = NULL;
  g_autoptr(GListModel) deltas = NULL;
  g_autoptr(FoundryVcsDelta) delta = NULL;

  test_repo_init (&repo);
  write_file (&repo, "tracked.txt", "old\n");
  commit_all (&repo, "add tracked");
  write_file (&repo, "tracked.txt", "new\n");
  write_file (&repo, "untracked.txt", "ignored by default\n");

  index_revision = foundry_vcs_revision_new_for_index ();
  worktree_revision = foundry_vcs_revision_new_for_worktree ();
  deltas = await_deltas (foundry_vcs_diff_full (repo.vcs, index_revision, worktree_revision, NULL));
  g_assert_cmpuint (g_list_model_get_n_items (deltas), ==, 1);

  delta = g_list_model_get_item (deltas, 0);
  assert_serializes_with (delta, "+new\n");
}

static void
test_commit_to_worktree_staged_delete_same_path_untracked (void)
{
  g_auto(TestRepo) repo = { 0 };
  g_autoptr(FoundryVcsRevision) head_revision = NULL;
  g_autoptr(FoundryVcsRevision) worktree_revision = NULL;
  g_autoptr(GListModel) deltas = NULL;
  g_autoptr(FoundryVcsDelta) delta = NULL;
  g_autoptr(FoundryVcsContent) content = NULL;
  g_autoptr(GError) error = NULL;

  test_repo_init (&repo);
  write_file (&repo, "replace.txt", "old\n");
  commit_all (&repo, "add replace");
  run_git (repo.tmpdir, "rm", "replace.txt", NULL, NULL);
  write_file (&repo, "replace.txt", "untracked replacement\n");

  head_revision = resolve_revision (repo.vcs, "HEAD");
  worktree_revision = foundry_vcs_revision_new_for_worktree ();
  deltas = await_deltas (foundry_vcs_diff_full (repo.vcs, head_revision, worktree_revision, NULL));
  g_assert_cmpuint (g_list_model_get_n_items (deltas), ==, 1);

  delta = g_list_model_get_item (deltas, 0);
  g_assert_cmpint (foundry_vcs_delta_get_status (delta), ==, FOUNDRY_VCS_DELTA_STATUS_DELETED);
  content = dex_await_object (foundry_vcs_delta_open_content (delta, FOUNDRY_VCS_DELTA_SIDE_NEW), &error);
  g_assert_no_error (error);
  g_assert_false (foundry_vcs_content_get_present (content));
}

static void
test_commit_to_worktree_cancelled_edits_disappear (void)
{
  g_auto(TestRepo) repo = { 0 };
  g_autoptr(FoundryVcsRevision) head_revision = NULL;
  g_autoptr(FoundryVcsRevision) worktree_revision = NULL;
  g_autoptr(GListModel) deltas = NULL;

  test_repo_init (&repo);
  write_file (&repo, "cancel.txt", "base\n");
  commit_all (&repo, "add cancel");
  write_file (&repo, "cancel.txt", "staged\n");
  run_git (repo.tmpdir, "add", "cancel.txt", NULL, NULL);
  write_file (&repo, "cancel.txt", "base\n");

  head_revision = resolve_revision (repo.vcs, "HEAD");
  worktree_revision = foundry_vcs_revision_new_for_worktree ();
  deltas = await_deltas (foundry_vcs_diff_full (repo.vcs, head_revision, worktree_revision, NULL));
  g_assert_cmpuint (g_list_model_get_n_items (deltas), ==, 0);
}

static void
test_invalid_revisions_fail_fiber (void)
{
  g_auto(TestRepo) repo = { 0 };
  g_autoptr(GError) error = NULL;
  g_autoptr(FoundryVcsRevision) revision = NULL;

  test_repo_init (&repo);
  write_file (&repo, "tracked.txt", "tracked\n");
  commit_all (&repo, "add tracked");

  revision = dex_await_object (foundry_vcs_resolve_revision (repo.vcs, "missing"), &error);
  g_assert_null (revision);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
  g_clear_error (&error);

  revision = dex_await_object (foundry_vcs_resolve_revision (repo.vcs, "HEAD^{tree}"), &error);
  g_assert_null (revision);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
}

static void
test_commit_to_commit_ignores_local_modification_wrapper (void)
{
  test_from_fiber (test_commit_to_commit_ignores_local_modification);
}

static void
test_commit_to_commit_deletion_ignores_recreated_file_wrapper (void)
{
  test_from_fiber (test_commit_to_commit_deletion_ignores_recreated_file);
}

static void
test_index_to_worktree_excludes_untracked_wrapper (void)
{
  test_from_fiber (test_index_to_worktree_excludes_untracked);
}

static void
test_commit_to_worktree_staged_delete_same_path_untracked_wrapper (void)
{
  test_from_fiber (test_commit_to_worktree_staged_delete_same_path_untracked);
}

static void
test_commit_to_worktree_cancelled_edits_disappear_wrapper (void)
{
  test_from_fiber (test_commit_to_worktree_cancelled_edits_disappear);
}

static void
test_invalid_revisions_fail_wrapper (void)
{
  test_from_fiber (test_invalid_revisions_fail_fiber);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  dex_init ();

  g_test_add_func ("/Foundry/VcsDiff/commit-to-commit-local-modification",
                   test_commit_to_commit_ignores_local_modification_wrapper);
  g_test_add_func ("/Foundry/VcsDiff/commit-to-commit-recreated-deletion",
                   test_commit_to_commit_deletion_ignores_recreated_file_wrapper);
  g_test_add_func ("/Foundry/VcsDiff/index-to-worktree",
                   test_index_to_worktree_excludes_untracked_wrapper);
  g_test_add_func ("/Foundry/VcsDiff/commit-to-worktree-staged-delete",
                   test_commit_to_worktree_staged_delete_same_path_untracked_wrapper);
  g_test_add_func ("/Foundry/VcsDiff/commit-to-worktree-cancelled-edits",
                   test_commit_to_worktree_cancelled_edits_disappear_wrapper);
  g_test_add_func ("/Foundry/VcsDiff/invalid-revisions",
                   test_invalid_revisions_fail_wrapper);

  return g_test_run ();
}
