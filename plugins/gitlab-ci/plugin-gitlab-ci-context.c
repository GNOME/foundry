/* plugin-gitlab-ci-context.c
 *
 * Copyright 2026 Christian Hergert
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-context.h"
#include "foundry-process-launcher.h"
#include "foundry-subprocess.h"
#include "foundry-vcs-commit.h"
#include "foundry-vcs-file.h"
#include "foundry-vcs-manager.h"
#include "foundry-vcs-remote.h"
#include "foundry-vcs.h"

#ifdef FOUNDRY_FEATURE_GIT
# include "foundry-git-status-entry.h"
# include "foundry-git-vcs.h"
#endif

#include "plugin-gitlab-ci-context-private.h"
#include "plugin-gitlab-ci-error-private.h"

DEX_DEFINE_CLOSURE_TYPE (ContextRequest, context_request,
                         DEX_DEFINE_CLOSURE_OBJECT (FoundryContext, context))

static PluginGitlabCiContext *
plugin_gitlab_ci_context_copy (PluginGitlabCiContext *self)
{
  return plugin_gitlab_ci_context_ref (self);
}

G_DEFINE_BOXED_TYPE (PluginGitlabCiContext,
                     plugin_gitlab_ci_context,
                     plugin_gitlab_ci_context_copy,
                     plugin_gitlab_ci_context_unref)

static void
add_variable (PluginGitlabCiContext *self,
              const char            *name,
              const char            *value)
{
  g_assert (self != NULL);
  g_assert (name != NULL);

  if (value != NULL && value[0] != '\0')
    g_hash_table_replace (self->variables, g_strdup (name), g_strdup (value));
}

static void
add_lines (GHashTable *set,
           const char *lines)
{
  g_auto(GStrv) split = NULL;

  g_assert (set != NULL);

  if (lines == NULL)
    return;

  split = g_strsplit (lines, "\n", -1);

  for (guint i = 0; split[i] != NULL; i++)
    {
      if (split[i][0] != '\0')
        g_hash_table_add (set, g_strdup (split[i]));
    }
}

static char *
run_git (const char         *directory,
         const char * const *arguments)
{
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *stdout_buf = NULL;

  g_assert (directory != NULL);
  g_assert (arguments != NULL);

  launcher = foundry_process_launcher_new ();
  foundry_process_launcher_push_host (launcher);
  foundry_process_launcher_append_argv (launcher, "git");
  foundry_process_launcher_append_argv (launcher, "-C");
  foundry_process_launcher_append_argv (launcher, directory);
  foundry_process_launcher_append_args (launcher, arguments);
  subprocess = foundry_process_launcher_spawn_with_flags (launcher,
                                                          (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE),
                                                          &error);

  if (subprocess == NULL)
    return NULL;

  stdout_buf = dex_await_string (foundry_subprocess_communicate_utf8 (subprocess, NULL), &error);
  if (stdout_buf == NULL || !g_subprocess_get_successful (subprocess))
    return NULL;

  g_strchomp (stdout_buf);

  return g_steal_pointer (&stdout_buf);
}

static void
parse_remote (const char  *remote,
              char       **host,
              char       **project)
{
  g_autofree char *path = NULL;
  const char *colon;

  g_assert (host != NULL);
  g_assert (project != NULL);

  if (remote == NULL || remote[0] == '\0')
    return;

  if (strstr (remote, "://") != NULL)
    {
      g_autoptr(GUri) uri = g_uri_parse (remote, G_URI_FLAGS_NONE, NULL);

      if (uri != NULL)
        {
          const char *uri_path = g_uri_get_path (uri);

          *host = g_strdup (g_uri_get_host (uri));
          if (uri_path != NULL)
            path = g_strdup (uri_path + strspn (uri_path, "/"));
        }
    }
  else if ((colon = strchr (remote, ':')))
    {
      const char *at = strchr (remote, '@');
      const char *host_start = at != NULL && at < colon ? at + 1 : remote;

      *host = g_strndup (host_start, colon - host_start);
      path = g_strdup (colon + 1);
    }

  if (*host != NULL && g_str_has_prefix (*host, "ssh."))
    {
      g_autofree char *tmp = *host;
      *host = g_strdup (tmp + strlen ("ssh."));
    }

  if (path != NULL)
    {
      if (g_str_has_suffix (path, ".git"))
        path[strlen (path) - 4] = '\0';
      *project = g_steal_pointer (&path);
    }
}

static gboolean
populate_files (PluginGitlabCiContext  *self,
                FoundryVcs             *vcs,
                GError                **error)
{
  g_autoptr(GListModel) files = NULL;

  g_assert (self != NULL);
  g_assert (FOUNDRY_IS_VCS (vcs));

  files = dex_await_object (foundry_vcs_list_files (vcs), error);
  if (files == NULL)
    return FALSE;

  for (guint i = 0; i < g_list_model_get_n_items (files); i++)
    {
      g_autoptr(FoundryVcsFile) file = g_list_model_get_item (files, i);
      g_autofree char *relative = foundry_vcs_file_dup_relative_path (file);

      if (relative != NULL)
        g_hash_table_add (self->files, g_steal_pointer (&relative));
    }

#ifdef FOUNDRY_FEATURE_GIT
  if (FOUNDRY_IS_GIT_VCS (vcs))
    {
      g_autoptr(GListModel) statuses = NULL;

      statuses = dex_await_object (foundry_git_vcs_list_status (FOUNDRY_GIT_VCS (vcs)), NULL);

      if (statuses != NULL)
        {
          for (guint i = 0; i < g_list_model_get_n_items (statuses); i++)
            {
              g_autoptr(FoundryGitStatusEntry) entry =
                g_list_model_get_item (statuses, i);
              g_autofree char *path =
                foundry_git_status_entry_dup_path (entry);

              if (path != NULL)
                {
                  g_hash_table_add (self->files, g_strdup (path));
                  g_hash_table_add (self->changed_files, g_steal_pointer (&path));
                }
            }
        }
    }
#endif

  return TRUE;
}

static DexFuture *
context_fiber (gpointer user_data)
{
  const char *diff_args[] = { "diff", "--name-only", "HEAD^", "--", NULL };
  ContextRequest *request = user_data;
  g_autoptr(PluginGitlabCiContext) self = NULL;
  g_autoptr(FoundryVcsManager) vcs_manager = NULL;
  g_autoptr(FoundryVcs) vcs = NULL;
  g_autoptr(FoundryVcsCommit) tip = NULL;
  g_autoptr(FoundryVcsRemote) origin = NULL;
  g_autoptr(GFile) project_directory = NULL;
  g_autoptr(GFile) configuration = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *remote = NULL;
  g_autofree char *remote_host = NULL;
  g_autofree char *project_path = NULL;
  g_autofree char *project_name = NULL;
  g_autofree char *project_namespace = NULL;
  g_autofree char *head = NULL;
  g_autofree char *short_sha = NULL;
  g_autofree char *branch = NULL;
  g_autofree char *pipeline_id = NULL;
  g_autofree char *pipeline_input = NULL;
  g_autofree char *project_dir = NULL;
  g_autofree char *diff = NULL;
  char *slash;

  g_assert (request != NULL);
  g_assert (FOUNDRY_IS_CONTEXT (request->context));

  self = g_new0 (PluginGitlabCiContext, 1);
  g_atomic_ref_count_init (&self->ref_count);
  project_directory = foundry_context_dup_project_directory (request->context);
  vcs_manager = foundry_context_dup_vcs_manager (request->context);

  if (project_directory == NULL ||
      !(self->repository_root = g_file_get_path (project_directory)) ||
      vcs_manager == NULL ||
      !(vcs = foundry_vcs_manager_dup_vcs (vcs_manager)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "GitLab CI requires a local VCS project");

  self->configuration_path = g_build_filename (self->repository_root, ".gitlab-ci.yml", NULL);
  self->server_host = g_strdup ("gitlab.com");
  self->variables = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  self->files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->changed_files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  configuration = g_file_new_for_path (self->configuration_path);
  if (dex_await_enum (dex_file_query_file_type (configuration,
                                                G_FILE_QUERY_INFO_NONE,
                                                G_PRIORITY_DEFAULT),
                      &error) != G_FILE_TYPE_REGULAR)
    {
      if (error == NULL)
        error = g_error_new (G_IO_ERROR,
                             G_IO_ERROR_NOT_FOUND,
                             "%s does not exist",
                             self->configuration_path);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  branch = foundry_vcs_dup_branch_name (vcs);
  tip = dex_await_object (foundry_vcs_load_tip (vcs), &error);
  if (tip == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  head = foundry_vcs_commit_dup_id (tip);
  if (head == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "VCS tip has no commit identifier");

  origin = dex_await_object (foundry_vcs_find_remote (vcs, "origin"), NULL);
  if (origin != NULL)
    remote = foundry_vcs_remote_dup_uri (origin);

  parse_remote (remote, &remote_host, &project_path);
  if (remote_host != NULL)
    g_set_str (&self->server_host, remote_host);

  if (project_path == NULL)
    {
      project_name = g_path_get_basename (self->repository_root);
      project_namespace = g_strdup ("local");
      project_path = g_strdup_printf ("%s/%s",
                                      project_namespace,
                                      project_name);
    }
  else if ((slash = strrchr (project_path, '/')))
    {
      project_name = g_strdup (slash + 1);
      project_namespace = g_strndup (project_path, slash - project_path);
    }
  else
    {
      project_name = g_strdup (project_path);
      project_namespace = g_strdup ("local");
    }

  if (!populate_files (self, vcs, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  diff = run_git (self->repository_root, diff_args);
  add_lines (self->changed_files, diff);

  short_sha = g_strndup (head, MIN ((gsize)8, strlen (head)));
  pipeline_input = g_strdup_printf ("%s\n%s\n%s", project_path, head, self->configuration_path);
  pipeline_id = g_compute_checksum_for_string (G_CHECKSUM_SHA256, pipeline_input, -1);
  pipeline_id[16] = '\0';
  project_dir = g_strdup_printf ("/builds/%s", project_path);

  add_variable (self, "CI", "true");
  add_variable (self, "GITLAB_CI", "true");
  add_variable (self, "CI_SERVER_HOST", self->server_host);
  add_variable (self, "CI_PROJECT_NAME", project_name);
  add_variable (self, "CI_PROJECT_NAMESPACE", project_namespace);
  add_variable (self, "CI_PROJECT_PATH", project_path);
  add_variable (self, "CI_PROJECT_DIR", project_dir);
  add_variable (self, "CI_COMMIT_SHA", head);
  add_variable (self, "CI_COMMIT_SHORT_SHA", short_sha);
  add_variable (self, "CI_COMMIT_REF_NAME", branch);
  add_variable (self, "CI_COMMIT_BRANCH", branch);
  add_variable (self, "CI_DEFAULT_BRANCH", branch ? branch : "main");
  add_variable (self, "CI_PIPELINE_SOURCE", "push");
  add_variable (self, "CI_PIPELINE_ID", pipeline_id);
  add_variable (self, "CI_PIPELINE_IID", pipeline_id);

  return dex_future_new_take_boxed (PLUGIN_TYPE_GITLAB_CI_CONTEXT,
                                    g_steal_pointer (&self));
}

DexFuture *
plugin_gitlab_ci_context_new (FoundryContext *context)
{
  ContextRequest *request;

  dex_return_error_if_fail (FOUNDRY_IS_CONTEXT (context));

  request = context_request_new ();
  request->context = g_object_ref (context);
  return dex_scheduler_spawn (NULL,
                              0,
                              context_fiber,
                              request,
                              (GDestroyNotify)context_request_free);
}

PluginGitlabCiContext *
plugin_gitlab_ci_context_ref (PluginGitlabCiContext *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  g_atomic_ref_count_inc (&self->ref_count);
  return self;
}

void
plugin_gitlab_ci_context_unref (PluginGitlabCiContext *self)
{
  if (self == NULL)
    return;
  if (!g_atomic_ref_count_dec (&self->ref_count))
    return;

  g_clear_pointer (&self->repository_root, g_free);
  g_clear_pointer (&self->configuration_path, g_free);
  g_clear_pointer (&self->server_host, g_free);
  g_clear_pointer (&self->variables, g_hash_table_unref);
  g_clear_pointer (&self->files, g_hash_table_unref);
  g_clear_pointer (&self->changed_files, g_hash_table_unref);
  g_free (self);
}

const char *
plugin_gitlab_ci_context_get_variable (PluginGitlabCiContext *self,
                                       const char            *name)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_hash_table_lookup (self->variables, name);
}

static gboolean
set_matches_pattern (GHashTable *set,
                     const char *pattern)
{
  GHashTableIter iter;
  gpointer key;

  g_assert (set != NULL);
  g_assert (pattern != NULL);

  g_hash_table_iter_init (&iter, set);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      if (g_pattern_match_simple (pattern, key))
        return TRUE;
    }
  return FALSE;
}

gboolean
plugin_gitlab_ci_context_file_exists (PluginGitlabCiContext *self,
                                      const char            *pattern)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (pattern != NULL, FALSE);

  return set_matches_pattern (self->files, pattern);
}

gboolean
plugin_gitlab_ci_context_file_changed (PluginGitlabCiContext *self,
                                       const char            *pattern)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (pattern != NULL, FALSE);

  return set_matches_pattern (self->changed_files, pattern);
}
