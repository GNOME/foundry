/*
 * test-acp-project-client.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#ifdef FOUNDRY_FEATURE_GIT
static FoundryGitStatusEntry *
find_status_entry (GListModel *status,
                   const char *path)
{
  guint n_items;

  g_assert (G_IS_LIST_MODEL (status));
  g_assert (path != NULL);

  n_items = g_list_model_get_n_items (status);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryGitStatusEntry) entry = NULL;
      g_autofree char *entry_path = NULL;

      entry = g_list_model_get_item (status, i);
      entry_path = foundry_git_status_entry_dup_path (entry);

      if (g_strcmp0 (entry_path, path) == 0)
        return g_object_ref (entry);
    }

  return NULL;
}
#endif

static FoundryAcpChangedFile *
find_changed_file (GListModel *changed_files,
                   const char *path)
{
  guint n_items;

  g_assert (G_IS_LIST_MODEL (changed_files));
  g_assert (path != NULL);

  n_items = g_list_model_get_n_items (changed_files);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryAcpChangedFile) changed_file = NULL;
      g_autofree char *changed_path = NULL;

      changed_file = g_list_model_get_item (changed_files, i);
      changed_path = foundry_acp_changed_file_dup_path (changed_file);

      if (g_strcmp0 (changed_path, path) == 0)
        return g_object_ref (changed_file);
    }

  return NULL;
}

static void
write_child_contents (GFile      *directory,
                      const char *name,
                      const char *contents)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;

  g_assert (G_IS_FILE (directory));
  g_assert (name != NULL);
  g_assert (contents != NULL);

  file = g_file_get_child (directory, name);
  g_file_replace_contents (file,
                           contents,
                           strlen (contents),
                           NULL,
                           FALSE,
                           0,
                           NULL,
                           NULL,
                           &error);
  g_assert_no_error (error);
}

#ifdef FOUNDRY_FEATURE_GIT
static void
stage_path (FoundryGitVcs *git_vcs,
            const char    *path)
{
  g_autoptr(FoundryGitStatusEntry) entry = NULL;
  g_autoptr(GListModel) status = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_GIT_VCS (git_vcs));
  g_assert (path != NULL);

  status = dex_await_object (foundry_git_vcs_list_status (git_vcs), &error);
  g_assert_no_error (error);
  g_assert_nonnull (status);

  entry = find_status_entry (status, path);
  g_assert_nonnull (entry);

  dex_await (foundry_git_vcs_stage_entry (git_vcs, entry, NULL), &error);
  g_assert_no_error (error);
}
#endif

static void
assert_changed_file (GListModel                 *changed_files,
                     const char                 *path,
                     FoundryAcpChangedFileKind   kind,
                     FoundryAcpChangedFileFlags  flags)
{
  g_autoptr(FoundryAcpChangedFile) changed_file = NULL;

  g_assert (G_IS_LIST_MODEL (changed_files));
  g_assert (path != NULL);

  changed_file = find_changed_file (changed_files, path);
  g_assert_nonnull (changed_file);
  g_assert_cmpint (foundry_acp_changed_file_get_kind (changed_file), ==, kind);
  g_assert_cmpint (foundry_acp_changed_file_get_flags (changed_file), ==, flags);
}

static void
test_read_text_file_fiber (void)
{
  g_autoptr(FoundryAcpProjectClient) client = NULL;
  g_autoptr(FoundryAcpSession) session = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_directory = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree char *contents = NULL;
  g_autofree char *project_path = NULL;

  project_path = g_dir_make_tmp ("foundry-acp-project-client-XXXXXX", &error);
  g_assert_no_error (error);
  g_assert_nonnull (project_path);

  project_directory = g_file_new_for_path (project_path);
  file = g_file_get_child (project_directory, "sample.txt");
  g_file_replace_contents (file,
                           "one\ntwo\nthree\n",
                           strlen ("one\ntwo\nthree\n"),
                           NULL,
                           FALSE,
                           0,
                           NULL,
                           NULL,
                           &error);
  g_assert_no_error (error);

  client = foundry_acp_project_client_new_for_project_directory (project_directory);
  session = g_object_new (FOUNDRY_TYPE_ACP_SESSION, NULL);
  contents = dex_await_string (foundry_acp_client_read_text_file (FOUNDRY_ACP_CLIENT (client),
                                                                  session,
                                                                  "sample.txt",
                                                                  2,
                                                                  1),
                               &error);
  g_assert_no_error (error);
  g_assert_cmpstr (contents, ==, "two\n");

  g_file_delete (file, NULL, NULL);
  g_file_delete (project_directory, NULL, NULL);
}

static void
test_read_text_file (void)
{
  test_from_fiber (test_read_text_file_fiber);
}

static void
test_write_text_file_fiber (void)
{
  g_autoptr(FoundryAcpProjectClient) client = NULL;
  g_autoptr(FoundryAcpSession) session = NULL;
  g_autoptr(GListModel) changed_files = NULL;
  g_autoptr(FoundryAcpChangedFile) changed_file = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_directory = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree char *changed_path = NULL;
  g_autofree char *contents = NULL;
  g_autofree char *project_path = NULL;

  project_path = g_dir_make_tmp ("foundry-acp-project-client-XXXXXX", &error);
  g_assert_no_error (error);
  g_assert_nonnull (project_path);

  project_directory = g_file_new_for_path (project_path);
  file = g_file_get_child (project_directory, "written.txt");
  client = foundry_acp_project_client_new_for_project_directory (project_directory);
  session = g_object_new (FOUNDRY_TYPE_ACP_SESSION, NULL);

  dex_await (foundry_acp_client_write_text_file (FOUNDRY_ACP_CLIENT (client),
                                                 session,
                                                 "written.txt",
                                                 "alpha\nbeta\n"),
             &error);
  g_assert_no_error (error);

  g_file_load_contents (file, NULL, &contents, NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (contents, ==, "alpha\nbeta\n");

  changed_files = foundry_acp_session_list_changed_files (session);
  g_assert_cmpuint (g_list_model_get_n_items (changed_files), ==, 1);

  changed_file = g_list_model_get_item (changed_files, 0);
  changed_path = foundry_acp_changed_file_dup_path (changed_file);
  g_assert_cmpstr (changed_path, ==, "written.txt");
  g_assert_cmpint (foundry_acp_changed_file_get_kind (changed_file), ==,
                   FOUNDRY_ACP_CHANGED_FILE_CREATED);
  g_assert_cmpint (foundry_acp_changed_file_get_flags (changed_file), ==,
                   FOUNDRY_ACP_CHANGED_FILE_UNSTAGED);

  g_file_delete (file, NULL, NULL);
  g_file_delete (project_directory, NULL, NULL);
}

static void
test_write_text_file (void)
{
  test_from_fiber (test_write_text_file_fiber);
}

static void
test_write_outside_project_fiber (void)
{
  g_autoptr(FoundryAcpProjectClient) client = NULL;
  g_autoptr(FoundryAcpSession) session = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_directory = NULL;
  g_autofree char *project_path = NULL;

  project_path = g_dir_make_tmp ("foundry-acp-project-client-XXXXXX", &error);
  g_assert_no_error (error);
  g_assert_nonnull (project_path);

  project_directory = g_file_new_for_path (project_path);
  client = foundry_acp_project_client_new_for_project_directory (project_directory);
  session = g_object_new (FOUNDRY_TYPE_ACP_SESSION, NULL);

  dex_await (foundry_acp_client_write_text_file (FOUNDRY_ACP_CLIENT (client),
                                                 session,
                                                 "/tmp/foundry-acp-outside.txt",
                                                 "outside\n"),
             &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);

  g_file_delete (project_directory, NULL, NULL);
}

static void
test_write_outside_project (void)
{
  test_from_fiber (test_write_outside_project_fiber);
}

static void
test_terminal_lifecycle_fiber (void)
{
  static const char *echo_argv[] = { "-c", "printf 'hello\\n'", NULL };
  static const char *sleep_argv[] = { "-c", "sleep 30", NULL };
  g_autoptr(FoundryAcpProjectClient) client = NULL;
  g_autoptr(FoundryAcpSession) session = NULL;
  g_autoptr(FoundryAcpTerminal) terminal = NULL;
  g_autoptr(FoundryAcpTerminal) killed_terminal = NULL;
  g_autoptr(FoundryAcpTerminalOutput) output = NULL;
  g_autoptr(GListModel) active_terminals = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_directory = NULL;
  char **terminal_argv = NULL;
  g_autofree char *terminal_id = NULL;
  g_autofree char *killed_terminal_id = NULL;
  g_autofree char *project_path = NULL;
  g_autofree char *scrollback = NULL;
  g_autofree char *text = NULL;

  project_path = g_dir_make_tmp ("foundry-acp-project-client-XXXXXX", &error);
  g_assert_no_error (error);
  g_assert_nonnull (project_path);

  project_directory = g_file_new_for_path (project_path);
  client = foundry_acp_project_client_new_for_project_directory (project_directory);
  session = g_object_new (FOUNDRY_TYPE_ACP_SESSION, NULL);

  terminal = dex_await_object (foundry_acp_client_create_terminal (FOUNDRY_ACP_CLIENT (client),
                                                                   session,
                                                                   "/bin/sh",
                                                                   echo_argv,
                                                                   NULL,
                                                                   NULL,
                                                                   -1),
                               &error);
  g_assert_no_error (error);
  g_assert_nonnull (terminal);

  terminal_id = foundry_acp_terminal_dup_id (terminal);
  g_assert_nonnull (terminal_id);
  terminal_argv = foundry_acp_terminal_dup_argv (terminal);
  g_assert_nonnull (terminal_argv);
  g_assert_cmpstr (terminal_argv[0], ==, "-c");
  g_assert_cmpstr (terminal_argv[1], ==, "printf 'hello\\n'");
  g_assert_null (terminal_argv[2]);
  g_clear_pointer (&terminal_argv, g_strfreev);
  g_assert_cmpint (foundry_acp_terminal_get_output_byte_limit (terminal), ==, -1);
  g_assert_cmpint (foundry_acp_terminal_get_created_at (terminal), >, 0);
  g_assert_cmpint (foundry_acp_terminal_get_started_at (terminal), >, 0);
  g_assert_cmpint (foundry_acp_terminal_get_exited_at (terminal), ==, 0);
  g_assert_false (foundry_acp_terminal_get_truncated (terminal));

  active_terminals = foundry_acp_session_list_active_terminals (session);
  g_assert_cmpuint (g_list_model_get_n_items (active_terminals), ==, 1);

  dex_await (foundry_acp_client_terminal_wait_for_exit (FOUNDRY_ACP_CLIENT (client),
                                                        session,
                                                        terminal_id),
             &error);
  g_assert_no_error (error);

  output = dex_await_object (foundry_acp_client_terminal_output (FOUNDRY_ACP_CLIENT (client),
                                                                 session,
                                                                 terminal_id),
                             &error);
  g_assert_no_error (error);
  g_assert_nonnull (output);
  g_assert_true (foundry_acp_terminal_output_has_exit_status (output));
  g_assert_cmpint (foundry_acp_terminal_output_get_exit_code (output), ==, 0);
  g_assert_cmpint (foundry_acp_terminal_get_exited_at (terminal), >, 0);

  text = foundry_acp_terminal_output_dup_text (output);
  g_assert_cmpstr (text, ==, "hello\n");
  scrollback = foundry_acp_terminal_dup_scrollback (terminal);
  g_assert_cmpstr (scrollback, ==, "hello\n");

  dex_await (foundry_acp_client_terminal_release (FOUNDRY_ACP_CLIENT (client),
                                                  session,
                                                  terminal_id),
             &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_list_model_get_n_items (active_terminals), ==, 0);

  dex_await (foundry_acp_client_terminal_output (FOUNDRY_ACP_CLIENT (client),
                                                 session,
                                                 terminal_id),
             &error);
  g_assert_error (error, FOUNDRY_ACP_ERROR, FOUNDRY_ACP_ERROR_RESOURCE_NOT_FOUND);
  g_clear_error (&error);

  killed_terminal =
    dex_await_object (foundry_acp_client_create_terminal (FOUNDRY_ACP_CLIENT (client),
                                                          session,
                                                          "/bin/sh",
                                                          sleep_argv,
                                                          NULL,
                                                          NULL,
                                                          -1),
                      &error);
  g_assert_no_error (error);
  g_assert_nonnull (killed_terminal);

  killed_terminal_id = foundry_acp_terminal_dup_id (killed_terminal);
  g_assert_nonnull (killed_terminal_id);

  dex_await (foundry_acp_client_terminal_kill (FOUNDRY_ACP_CLIENT (client),
                                               session,
                                               killed_terminal_id),
             &error);
  g_assert_no_error (error);
  g_assert_cmpint (foundry_acp_terminal_get_state (killed_terminal), ==,
                   FOUNDRY_ACP_TERMINAL_CANCELLED);

  dex_await (foundry_acp_client_terminal_release (FOUNDRY_ACP_CLIENT (client),
                                                  session,
                                                  killed_terminal_id),
             &error);
  g_assert_no_error (error);

  rm_rf (project_path);
}

static void
test_terminal_lifecycle (void)
{
  test_from_fiber (test_terminal_lifecycle_fiber);
}

static void
test_snapshot_changed_files_fiber (void)
{
  g_autoptr(FoundryAcpProjectClient) client = NULL;
  g_autoptr(FoundryAcpSession) session = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_directory = NULL;
  g_autoptr(GFile) deleted_file = NULL;
  g_autoptr(GListModel) changed_files = NULL;
  g_autofree char *project_path = NULL;

  project_path = g_dir_make_tmp ("foundry-acp-project-client-XXXXXX", &error);
  g_assert_no_error (error);
  g_assert_nonnull (project_path);

  project_directory = g_file_new_for_path (project_path);
  write_child_contents (project_directory, "modified.txt", "before\n");
  write_child_contents (project_directory, "deleted.txt", "delete me\n");
  client = foundry_acp_project_client_new_for_project_directory (project_directory);
  session = g_object_new (FOUNDRY_TYPE_ACP_SESSION, NULL);

  dex_await (foundry_acp_project_client_refresh_changed_files (client, session), &error);
  g_assert_no_error (error);

  changed_files = foundry_acp_session_list_changed_files (session);
  g_assert_cmpuint (g_list_model_get_n_items (changed_files), ==, 0);

  write_child_contents (project_directory, "modified.txt", "after\n");
  write_child_contents (project_directory, "created.txt", "new\n");
  deleted_file = g_file_get_child (project_directory, "deleted.txt");
  g_file_delete (deleted_file, NULL, &error);
  g_assert_no_error (error);

  dex_await (foundry_acp_project_client_refresh_changed_files (client, session), &error);
  g_assert_no_error (error);

  assert_changed_file (changed_files,
                       "modified.txt",
                       FOUNDRY_ACP_CHANGED_FILE_MODIFIED,
                       FOUNDRY_ACP_CHANGED_FILE_UNSTAGED);
  assert_changed_file (changed_files,
                       "created.txt",
                       FOUNDRY_ACP_CHANGED_FILE_CREATED,
                       FOUNDRY_ACP_CHANGED_FILE_UNSTAGED);
  assert_changed_file (changed_files,
                       "deleted.txt",
                       FOUNDRY_ACP_CHANGED_FILE_DELETED,
                       FOUNDRY_ACP_CHANGED_FILE_UNSTAGED);

  rm_rf (project_path);
}

static void
test_snapshot_changed_files (void)
{
  test_from_fiber (test_snapshot_changed_files_fiber);
}

#ifdef FOUNDRY_FEATURE_GIT
static void
test_refresh_changed_files_fiber (void)
{
  g_autoptr(FoundryAcpProjectClient) client = NULL;
  g_autoptr(FoundryAcpSession) session = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryGitVcs) git_vcs = NULL;
  g_autoptr(FoundryVcsManager) vcs_manager = NULL;
  g_autoptr(FoundryVcsProvider) git_provider = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_directory = NULL;
  g_autoptr(GFile) deleted_file = NULL;
  g_autoptr(GListModel) changed_files = NULL;
  g_autofree char *foundry_dir = NULL;
  g_autofree char *project_path = NULL;

  project_path = g_dir_make_tmp ("foundry-acp-project-client-XXXXXX", &error);
  g_assert_no_error (error);
  g_assert_nonnull (project_path);

  foundry_dir = g_build_filename (project_path, ".foundry", NULL);
  context = dex_await_object (foundry_context_new (foundry_dir,
                                                   project_path,
                                                   FOUNDRY_CONTEXT_FLAGS_CREATE,
                                                   NULL),
                              &error);
  g_assert_no_error (error);
  g_assert_nonnull (context);

  vcs_manager = foundry_context_dup_vcs_manager (context);
  g_assert_nonnull (vcs_manager);

  git_provider = foundry_vcs_manager_find_provider (vcs_manager, "git");
  g_assert_nonnull (git_provider);

  dex_await (foundry_vcs_provider_initialize (git_provider), &error);
  g_assert_no_error (error);

  git_vcs = FOUNDRY_GIT_VCS (foundry_vcs_manager_dup_vcs (vcs_manager));
  g_assert_nonnull (git_vcs);
  g_assert_true (FOUNDRY_IS_GIT_VCS (git_vcs));

  project_directory = foundry_context_dup_project_directory (context);
  write_child_contents (project_directory, "modified.txt", "before\n");
  write_child_contents (project_directory, "deleted.txt", "delete me\n");
  stage_path (git_vcs, "modified.txt");
  stage_path (git_vcs, "deleted.txt");
  dex_await (foundry_git_vcs_commit (git_vcs,
                                     "Initial commit",
                                     "Foundry Test",
                                     "foundry@example.com"),
             &error);
  g_assert_no_error (error);

  write_child_contents (project_directory, "modified.txt", "after\n");
  write_child_contents (project_directory, "created.txt", "new\n");
  write_child_contents (project_directory, "staged.txt", "staged\n");
  stage_path (git_vcs, "staged.txt");

  deleted_file = g_file_get_child (project_directory, "deleted.txt");
  g_file_delete (deleted_file, NULL, &error);
  g_assert_no_error (error);

  client = foundry_acp_project_client_new (context);
  session = g_object_new (FOUNDRY_TYPE_ACP_SESSION, NULL);

  dex_await (foundry_acp_project_client_refresh_changed_files (client, session), &error);
  g_assert_no_error (error);

  changed_files = foundry_acp_session_list_changed_files (session);
  assert_changed_file (changed_files,
                       "modified.txt",
                       FOUNDRY_ACP_CHANGED_FILE_MODIFIED,
                       FOUNDRY_ACP_CHANGED_FILE_UNSTAGED);
  assert_changed_file (changed_files,
                       "created.txt",
                       FOUNDRY_ACP_CHANGED_FILE_CREATED,
                       (FOUNDRY_ACP_CHANGED_FILE_UNSTAGED |
                        FOUNDRY_ACP_CHANGED_FILE_UNTRACKED));
  assert_changed_file (changed_files,
                       "deleted.txt",
                       FOUNDRY_ACP_CHANGED_FILE_DELETED,
                       FOUNDRY_ACP_CHANGED_FILE_UNSTAGED);
  assert_changed_file (changed_files,
                       "staged.txt",
                       FOUNDRY_ACP_CHANGED_FILE_CREATED,
                       (FOUNDRY_ACP_CHANGED_FILE_STAGED |
                        FOUNDRY_ACP_CHANGED_FILE_UNTRACKED));

  rm_rf (project_path);
}

static void
test_refresh_changed_files (void)
{
  test_from_fiber (test_refresh_changed_files_fiber);
}
#endif

int
main (int   argc,
      char *argv[])
{
  dex_init ();

  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Foundry/AcpProjectClient/read-text-file", test_read_text_file);
  g_test_add_func ("/Foundry/AcpProjectClient/write-text-file", test_write_text_file);
  g_test_add_func ("/Foundry/AcpProjectClient/write-outside-project",
                   test_write_outside_project);
  g_test_add_func ("/Foundry/AcpProjectClient/terminal-lifecycle",
                   test_terminal_lifecycle);
  g_test_add_func ("/Foundry/AcpProjectClient/snapshot-changed-files",
                   test_snapshot_changed_files);
#ifdef FOUNDRY_FEATURE_GIT
  g_test_add_func ("/Foundry/AcpProjectClient/refresh-changed-files",
                   test_refresh_changed_files);
#endif

  return g_test_run ();
}
