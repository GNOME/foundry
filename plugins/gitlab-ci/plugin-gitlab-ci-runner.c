/* plugin-gitlab-ci-runner.c
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

#include <unistd.h>

#include <foundry.h>

#include "plugin-gitlab-ci-error-private.h"
#include "plugin-gitlab-ci-runner-private.h"

typedef enum
{
  JOB_PENDING,
  JOB_RUNNING,
  JOB_PASSED,
  JOB_FAILED,
  JOB_SKIPPED,
} JobStatus;

typedef struct
{
  PluginGitlabCiJob *job;
  DexPromise        *completion;
  char              *workspace;
  char              *artifact_dir;
  guint              image_uid;
  guint              image_gid;
  JobStatus          status;
  int                exit_status;
  guint              image_identity_resolved : 1;
} JobState;

typedef struct
{
  gatomicrefcount           ref_count;
  PluginGitlabCiContext    *context;
  PluginGitlabCiPipeline   *pipeline;
  PluginGitlabCiRunOptions *options;
  DexCancellable           *cancellable;
  DexCancellable           *stop;
  DexLimiter               *limiter;
  GHashTable               *selected;
  GHashTable               *states;
  GPtrArray                *state_array;
  char                     *run_root;
  char                     *workspace_root;
  char                     *control_root;
  char                     *artifact_root;
  guint                     n_finished;
  guint                     n_failed;
} Execution;

typedef struct
{
  Execution *execution;
  JobState  *state;
} JobRequest;

typedef struct
{
  GInputStream *stream;
  GPtrArray    *secrets;
  char         *job_name;
  int           fd;
} PumpRequest;

DEX_DEFINE_CLOSURE_TYPE (RunnerRequest, runner_request,
                         DEX_DEFINE_CLOSURE_POINTER (PluginGitlabCiContext *, context, plugin_gitlab_ci_context_unref),
                         DEX_DEFINE_CLOSURE_OBJECT (PluginGitlabCiPipeline, pipeline),
                         DEX_DEFINE_CLOSURE_VALUE (PluginGitlabCiRunOptions *, options),
                         DEX_DEFINE_CLOSURE_POINTER (DexCancellable *, cancellable, dex_unref))

static Execution *execution_ref   (Execution *self);
static void       execution_unref (Execution *self);

static void
job_state_free (JobState *state)
{
  dex_clear (&state->completion);
  g_clear_pointer (&state->workspace, g_free);
  g_clear_pointer (&state->artifact_dir, g_free);
  g_free (state);
}

static Execution *
execution_ref (Execution *self)
{
  g_assert (self != NULL);

  g_atomic_ref_count_inc (&self->ref_count);
  return self;
}

static void
execution_unref (Execution *self)
{
  if (!g_atomic_ref_count_dec (&self->ref_count))
    return;

  g_clear_pointer (&self->context, plugin_gitlab_ci_context_unref);
  g_clear_object (&self->pipeline);
  dex_clear (&self->cancellable);
  dex_clear (&self->stop);
  dex_clear (&self->limiter);
  g_clear_pointer (&self->selected, g_hash_table_unref);
  g_clear_pointer (&self->states, g_hash_table_unref);
  g_clear_pointer (&self->state_array, g_ptr_array_unref);
  g_clear_pointer (&self->run_root, g_free);
  g_clear_pointer (&self->workspace_root, g_free);
  g_clear_pointer (&self->control_root, g_free);
  g_clear_pointer (&self->artifact_root, g_free);
  g_free (self);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (Execution, execution_unref)

static void
job_request_free (JobRequest *request)
{
  g_clear_pointer (&request->execution, execution_unref);
  g_free (request);
}

static void
pump_request_free (PumpRequest *request)
{
  g_clear_object (&request->stream);
  g_clear_pointer (&request->secrets, g_ptr_array_unref);
  g_clear_pointer (&request->job_name, g_free);
  g_free (request);
}

static char *
safe_name (const char *name)
{
  char *ret;

  g_assert (name != NULL);

  ret = g_strdup (name);

  for (char *iter = ret; *iter != '\0'; iter++)
    {
      if (!g_ascii_isalnum (*iter) && *iter != '-' && *iter != '_' && *iter != '.')
        *iter = '_';
    }

  return ret;
}

static gboolean
safe_relative_path (const char *path)
{
  g_auto(GStrv) parts = NULL;

  g_assert (path != NULL);

  if (path[0] == '\0' || g_path_is_absolute (path))
    return FALSE;

  parts = g_strsplit (path, G_DIR_SEPARATOR_S, -1);
  for (guint i = 0; parts[i] != NULL; i++)
    {
      if (g_str_equal (parts[i], ".."))
        return FALSE;
    }

  return TRUE;
}

static gboolean
ensure_directory (const char  *path,
                  GError     **error)
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(GError) local_error = NULL;

  g_assert (path != NULL);

  file = g_file_new_for_path (path);

  if (dex_await (dex_file_make_directory_with_parents (file), &local_error))
    return TRUE;

  if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    return TRUE;

  g_propagate_error (error, g_steal_pointer (&local_error));

  return FALSE;
}

static gboolean
write_file (const char  *path,
            const char  *contents,
            GError     **error)
{
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GFile) file = NULL;

  g_assert (path != NULL);
  g_assert (contents != NULL);

  file = g_file_new_for_path (path);
  bytes = g_bytes_new (contents, strlen (contents));

  return dex_await (dex_file_replace_contents_bytes (file, bytes, NULL, FALSE, G_FILE_CREATE_PRIVATE), error);
}

static gboolean
run_git (Execution          *execution,
         const char * const *arguments,
         GError            **error)
{
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;

  g_assert (execution != NULL);
  g_assert (arguments != NULL);

  launcher = foundry_process_launcher_new ();
  foundry_process_launcher_push_host (launcher);
  foundry_process_launcher_append_argv (launcher, "git");
  foundry_process_launcher_append_args (launcher, arguments);

  if (execution->options->stderr_fd >= 0)
    foundry_process_launcher_take_fd (launcher,
                                      dup (execution->options->stderr_fd),
                                      STDERR_FILENO);
  else
    foundry_process_launcher_take_fd (launcher,
                                      dup (STDERR_FILENO),
                                      STDERR_FILENO);

  if (!(subprocess = foundry_process_launcher_spawn (launcher, error)))
    return FALSE;

  return dex_await (foundry_subprocess_wait_check (subprocess, execution->cancellable), error);
}

static gboolean
clone_project (Execution   *execution,
               const char  *workspace,
               GError     **error)
{
  const char *arguments[] = {
    "clone",
    "--no-checkout",
    "--quiet",
    "--",
    NULL,
    NULL,
    NULL
  };

  g_assert (execution != NULL);
  g_assert (workspace != NULL);

  /* A no-checkout clone provides the Git metadata available in GitLab jobs
   * without writing the project files twice. Local clones hard-link objects
   * when possible and fall back to copying across filesystems.
   */
  arguments[4] = execution->context->repository_root;
  arguments[5] = workspace;

  return run_git (execution, arguments, error);
}

static gboolean
reset_project_index (Execution   *execution,
                     const char  *workspace,
                     GError     **error)
{
  const char *arguments[] = {
    "-C",
    NULL,
    "reset",
    "--mixed",
    "--quiet",
    "HEAD",
    NULL
  };

  g_assert (execution != NULL);
  g_assert (workspace != NULL);

  arguments[1] = workspace;

  return run_git (execution, arguments, error);
}

static gboolean
copy_tree (GFile   *source,
           GFile   *destination,
           GError **error)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  const char *attributes = (G_FILE_ATTRIBUTE_STANDARD_NAME ","
                            G_FILE_ATTRIBUTE_STANDARD_TYPE);

  g_assert (G_IS_FILE (source));
  g_assert (G_IS_FILE (destination));

  if (!dex_await (dex_file_make_directory_with_parents (destination), error) &&
      !g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    return FALSE;

  g_clear_error (error);

  enumerator = dex_await_object (dex_file_enumerate_children (source,
                                                              attributes,
                                                              G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                              G_PRIORITY_DEFAULT),
                                 error);
  if (enumerator == NULL)
    return FALSE;

  for (;;)
    {
      g_autolist(GFileInfo) infos = NULL;

      infos = dex_await_boxed (dex_file_enumerator_next_files (enumerator, 64, G_PRIORITY_DEFAULT), error);
      if (infos == NULL && error != NULL && *error != NULL)
        return FALSE;

      if (infos == NULL)
        break;

      for (const GList *iter = infos; iter != NULL; iter = iter->next)
        {
          GFileInfo *info = iter->data;
          g_autoptr(GFile) source_child = NULL;
          g_autoptr(GFile) destination_child = NULL;
          const char *name = g_file_info_get_name (info);

          if (name == NULL)
            continue;

          source_child = g_file_get_child (source, name);
          destination_child = g_file_get_child (destination, name);

          if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
            {
              if (!copy_tree (source_child, destination_child, error))
                return FALSE;
            }
          else if (!dex_await (dex_file_copy (source_child,
                                              destination_child,
                                              (G_FILE_COPY_OVERWRITE |
                                               G_FILE_COPY_ALL_METADATA |
                                               G_FILE_COPY_NOFOLLOW_SYMLINKS),
                                              G_PRIORITY_DEFAULT),
                               error))
            {
              return FALSE;
            }
        }
    }

  return TRUE;
}

static gboolean
copy_project (Execution   *execution,
              const char  *workspace,
              GError     **error)
{
  GHashTableIter iter;
  gpointer key;

  g_assert (execution != NULL);
  g_assert (workspace != NULL);

  if (!clone_project (execution, workspace, error))
    return FALSE;

  if (!ensure_directory (workspace, error))
    return FALSE;

  g_hash_table_iter_init (&iter, execution->context->files);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      const char *relative = key;
      g_autofree char *source_path = NULL;
      g_autofree char *destination_path = NULL;
      g_autoptr(GFile) source = NULL;
      g_autoptr(GFile) destination = NULL;
      g_autoptr(GFile) parent = NULL;
      g_autoptr(GFileInfo) info = NULL;
      g_autoptr(GError) local_error = NULL;

      if (!safe_relative_path (relative))
        continue;

      source_path = g_build_filename (execution->context->repository_root, relative, NULL);
      destination_path = g_build_filename (workspace, relative, NULL);
      source = g_file_new_for_path (source_path);
      destination = g_file_new_for_path (destination_path);
      parent = g_file_get_parent (destination);

      if (!dex_await (dex_file_make_directory_with_parents (parent), error) &&
          !g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        return FALSE;

      g_clear_error (error);

      info = dex_await_object (dex_file_query_info (source,
                                                    G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                    G_PRIORITY_DEFAULT),
                               &local_error);

      if (info == NULL)
        {
          if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
            continue;

          g_propagate_error (error, g_steal_pointer (&local_error));
          return FALSE;
        }

      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        {
          if (!copy_tree (source, destination, error))
            return FALSE;
        }
      else if (!dex_await (dex_file_copy (source,
                                          destination,
                                          (G_FILE_COPY_OVERWRITE |
                                           G_FILE_COPY_ALL_METADATA |
                                           G_FILE_COPY_NOFOLLOW_SYMLINKS),
                                          G_PRIORITY_DEFAULT),
                           error))
        {
          return FALSE;
        }
    }

  return reset_project_index (execution, workspace, error);
}

static gboolean
write_all (int          fd,
           const char  *data,
           gsize        length,
           GError     **error)
{
  gsize written = 0;

  g_assert (fd >= 0);
  g_assert (data != NULL || length == 0);

  while (written < length)
    {
      gint64 count;

      count = dex_await_int64 (dex_aio_write (NULL, fd, data + written, length - written, -1), error);
      if (count <= 0)
        return FALSE;

      written += count;
    }

  return TRUE;
}

static void
redact (GString   *string,
        GPtrArray *secrets)
{
  g_assert (string != NULL);
  g_assert (secrets != NULL);

  for (guint i = 0; i < secrets->len; i++)
    {
      const char *secret = g_ptr_array_index (secrets, i);

      if (secret[0] != '\0')
        g_string_replace (string, secret, "[REDACTED]", 0);
    }
}

static DexFuture *
pump_fiber (gpointer user_data)
{
  PumpRequest *request = user_data;
  g_autoptr(GString) pending = NULL;
  g_autoptr(GError) error = NULL;
  gsize tail_length = 0;

  g_assert (request != NULL);

  pending = g_string_new (NULL);

  for (guint i = 0; i < request->secrets->len; i++)
    {
      const char *secret = g_ptr_array_index (request->secrets, i);

      tail_length = MAX (tail_length, strlen (secret));
    }

  if (tail_length > 0)
    tail_length--;

  for (;;)
    {
      g_autoptr(GBytes) bytes = NULL;
      g_autoptr(GString) output = NULL;
      g_autofree char *prefix = NULL;
      const char *data;
      gsize length;
      gsize emit_length;

      if (!(bytes = dex_await_boxed (dex_input_stream_read_bytes (request->stream, 8192, G_PRIORITY_DEFAULT), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      data = g_bytes_get_data (bytes, &length);
      if (length > 0)
        g_string_append_len (pending, data, length);

      redact (pending, request->secrets);
      emit_length = length == 0 || pending->len <= tail_length
                      ? pending->len
                      : pending->len - tail_length;

      if (length > 0 && pending->len <= tail_length)
        continue;

      if (emit_length > 0)
        {
          prefix = g_strdup_printf ("[%s] ", request->job_name);
          output = g_string_new (prefix);
          g_string_append_len (output, pending->str, emit_length);
          if (!write_all (request->fd, output->str, output->len, &error))
            return dex_future_new_for_error (g_steal_pointer (&error));
          g_string_erase (pending, 0, emit_length);
        }

      if (length == 0)
        break;
    }

  return dex_future_new_true ();
}

static guint
parse_timeout (const char *timeout)
{
  g_autofree char *copy = NULL;
  char *end;
  guint64 multiplier = 1;
  guint64 value;

  if (timeout == NULL || timeout[0] == '\0')
    return 3600;

  copy = g_strstrip (g_ascii_strdown (timeout, -1));
  value = g_ascii_strtoull (copy, &end, 10);
  while (g_ascii_isspace (*end))
    end++;

  if (g_str_has_prefix (end, "h"))
    multiplier = 60 * 60;
  else if (g_str_has_prefix (end, "m"))
    multiplier = 60;
  else if (*end != '\0' && !g_str_has_prefix (end, "s"))
    return 3600;

  return CLAMP (value * multiplier, 1, G_MAXINT);
}

static GPtrArray *
collect_secrets (PluginGitlabCiJob *job)
{
  GHashTableIter iter;
  g_autoptr(GPtrArray) secrets = NULL;
  gpointer value;

  g_assert (job != NULL);

  secrets = g_ptr_array_new_with_free_func (g_free);
  g_hash_table_iter_init (&iter, job->variables);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      PluginGitlabCiVariable *variable = value;

      if (variable->secret && variable->value[0] != '\0')
        g_ptr_array_add (secrets, g_strdup (variable->value));
    }

  return g_steal_pointer (&secrets);
}

static gboolean
parse_image_identity (const char  *output,
                      guint       *uid,
                      guint       *gid,
                      GError     **error)
{
  g_autofree char *copy = NULL;
  g_auto(GStrv) parts = NULL;
  guint64 parsed_uid;
  guint64 parsed_gid;

  g_assert (output != NULL);
  g_assert (uid != NULL);
  g_assert (gid != NULL);

  copy = g_strstrip (g_strdup (output));
  parts = g_strsplit (copy, ":", -1);

  if (g_strv_length (parts) != 2 ||
      !g_ascii_string_to_unsigned (parts[0], 10, 0, G_MAXUINT, &parsed_uid, NULL) ||
      !g_ascii_string_to_unsigned (parts[1], 10, 0, G_MAXUINT, &parsed_gid, NULL))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_DATA,
                           "Invalid user identity returned by container image");
      return FALSE;
    }

  *uid = parsed_uid;
  *gid = parsed_gid;

  return TRUE;
}

static gboolean
resolve_image_identity (Execution  *execution,
                        JobState   *state,
                        GError    **error)
{
  static const char identity_script[] =
    "while read key value rest; do "
    "case \"$key\" in Uid:) uid=$value ;; Gid:) gid=$value ;; esac; "
    "done < /proc/self/status; "
    "printf '%s:%s\\n' \"$uid\" \"$gid\"";
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autofree char *output = NULL;

  g_assert (execution != NULL);
  g_assert (state != NULL);

  if (state->image_identity_resolved)
    return TRUE;

  /* Rootless Podman normally maps the host user to container UID 0. Probe the
   * image's effective user so keep-id can instead map the host-owned workspace
   * and private control files to the UID and GID expected by the image.
   */
  launcher = foundry_process_launcher_new ();
  foundry_process_launcher_push_host (launcher);
  foundry_process_launcher_append_argv (launcher, "podman");
  foundry_process_launcher_append_argv (launcher, "run");
  foundry_process_launcher_append_argv (launcher, "--rm");
  foundry_process_launcher_append_argv (launcher,
                                        execution->options->offline
                                          ? "--pull=never"
                                          : "--pull=missing");
  foundry_process_launcher_append_argv (launcher, "--network=none");
  foundry_process_launcher_append_argv (launcher, "--security-opt");
  foundry_process_launcher_append_argv (launcher, "no-new-privileges");
  foundry_process_launcher_append_argv (launcher, "--cap-drop=all");

  if (state->job->image_user != NULL)
    {
      foundry_process_launcher_append_argv (launcher, "--user");
      foundry_process_launcher_append_argv (launcher, state->job->image_user);
    }

  foundry_process_launcher_append_argv (launcher, "--entrypoint");
  foundry_process_launcher_append_argv (launcher, "/bin/sh");
  foundry_process_launcher_append_argv (launcher, state->job->image);
  foundry_process_launcher_append_argv (launcher, "-c");
  foundry_process_launcher_append_argv (launcher, identity_script);

  if (execution->options->stderr_fd >= 0)
    foundry_process_launcher_take_fd (launcher,
                                      dup (execution->options->stderr_fd),
                                      STDERR_FILENO);
  else
    foundry_process_launcher_take_fd (launcher,
                                      dup (STDERR_FILENO),
                                      STDERR_FILENO);

  if (!(subprocess = foundry_process_launcher_spawn_with_flags (launcher, G_SUBPROCESS_FLAGS_STDOUT_PIPE, error)))
    return FALSE;

  if (!(output = dex_await_string (foundry_subprocess_communicate_utf8 (subprocess, NULL), error)))
    return FALSE;

  if (!g_subprocess_get_successful (subprocess))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to determine user identity for image '%s'",
                   state->job->image);
      return FALSE;
    }

  if (!parse_image_identity (output, &state->image_uid, &state->image_gid, error))
    return FALSE;

  state->image_identity_resolved = TRUE;

  return TRUE;
}

static char *
build_script (GPtrArray *commands,
              gboolean   exit_on_error)
{
  g_autoptr(GString) script = NULL;

  g_assert (commands != NULL);

  script = g_string_new ("#!/bin/sh\n");
  if (exit_on_error)
    g_string_append (script, "set -e\n");

  for (guint i = 0; i < commands->len; i++)
    {
      g_string_append (script, g_ptr_array_index (commands, i));
      g_string_append_c (script, '\n');
    }

  return g_string_free (g_steal_pointer (&script), FALSE);
}

static gboolean
prepare_control (Execution  *execution,
                 JobState   *state,
                 char      **control_dir,
                 GError    **error)
{
  g_autofree char *name = NULL;
  g_autofree char *main_path = NULL;
  g_autofree char *after_path = NULL;
  g_autofree char *main_script = NULL;
  g_autofree char *after_script = NULL;
  GHashTableIter iter;
  gpointer value;

  g_assert (execution != NULL);
  g_assert (state != NULL);
  g_assert (control_dir != NULL);

  name = safe_name (state->job->name);
  *control_dir = g_build_filename (execution->control_root, name, NULL);
  if (!ensure_directory (*control_dir, error))
    return FALSE;

  main_path = g_build_filename (*control_dir, "main.sh", NULL);
  after_path = g_build_filename (*control_dir, "after.sh", NULL);

  {
    g_autoptr(GPtrArray) commands = g_ptr_array_new ();

    for (guint i = 0; i < state->job->before_script->len; i++)
      g_ptr_array_add (commands, g_ptr_array_index (state->job->before_script, i));
    for (guint i = 0; i < state->job->script->len; i++)
      g_ptr_array_add (commands, g_ptr_array_index (state->job->script, i));

    main_script = build_script (commands, TRUE);
  }

  after_script = build_script (state->job->after_script, FALSE);
  if (!write_file (main_path, main_script, error) ||
      !write_file (after_path, after_script, error))
    return FALSE;

  g_hash_table_iter_init (&iter, state->job->variables);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      PluginGitlabCiVariable *variable = value;

      if (variable->file)
        {
          g_autofree char *variables_dir = NULL;
          g_autofree char *variable_name = NULL;
          g_autofree char *variable_path = NULL;

          variables_dir = g_build_filename (*control_dir, "variables", NULL);
          variable_name = safe_name (variable->name);
          variable_path = g_build_filename (variables_dir, variable_name, NULL);

          if (!ensure_directory (variables_dir, error) ||
              !write_file (variable_path, variable->value, error))
            return FALSE;
        }
    }

  return TRUE;
}

static void
append_environment (FoundryProcessLauncher *launcher,
                    PluginGitlabCiJob      *job)
{
  GHashTableIter iter;
  gpointer value;

  g_assert (FOUNDRY_IS_PROCESS_LAUNCHER (launcher));
  g_assert (job != NULL);

  g_hash_table_iter_init (&iter, job->variables);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      PluginGitlabCiVariable *variable = value;
      g_autofree char *argument = NULL;
      g_autofree char *variable_name = NULL;

      if (variable->file)
        {
          variable_name = safe_name (variable->name);
          argument = g_strdup_printf ("%s=/gitlab/variables/%s",
                                      variable->name,
                                      variable_name);
        }
      else
        {
          argument = g_strdup_printf ("%s=%s", variable->name, variable->value);
        }

      foundry_process_launcher_append_argv (launcher, "--env");
      foundry_process_launcher_append_argv (launcher, argument);
    }
}

static void
append_podman_arguments (Execution              *execution,
                         JobState               *state,
                         FoundryProcessLauncher *launcher,
                         const char             *control_dir,
                         const char             *cidfile)
{
  g_autofree char *container_name = NULL;
  g_autofree char *workspace_mount = NULL;
  g_autofree char *control_mount = NULL;
  g_autofree char *run_name = NULL;
  g_autofree char *job_name = NULL;
  g_autofree char *user_namespace = NULL;
  const char *project_dir;

  g_assert (execution != NULL);
  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_PROCESS_LAUNCHER (launcher));
  g_assert (control_dir != NULL);
  g_assert (cidfile != NULL);
  g_assert (state->image_identity_resolved);

  run_name = g_path_get_basename (execution->run_root);
  job_name = safe_name (state->job->name);
  container_name = g_strdup_printf ("foundry-%.8s-%s", run_name, job_name);

  if (strlen (container_name) > 63)
    container_name[63] = '\0';

  if (!(project_dir = plugin_gitlab_ci_context_get_variable (execution->context, "CI_PROJECT_DIR")))
    project_dir = "/builds/project";

  workspace_mount = g_strdup_printf ("%s:%s:Z", state->workspace, project_dir);
  control_mount = g_strdup_printf ("%s:/gitlab:ro,Z", control_dir);
  user_namespace = g_strdup_printf ("keep-id:uid=%u,gid=%u",
                                    state->image_uid,
                                    state->image_gid);

  foundry_process_launcher_append_argv (launcher, "podman");
  foundry_process_launcher_append_argv (launcher, "run");
  foundry_process_launcher_append_argv (launcher, "--rm");
  foundry_process_launcher_append_argv (launcher, "--userns");
  foundry_process_launcher_append_argv (launcher, user_namespace);
  foundry_process_launcher_append_argv (launcher, "--name");
  foundry_process_launcher_append_argv (launcher, container_name);
  foundry_process_launcher_append_argv (launcher, "--cidfile");
  foundry_process_launcher_append_argv (launcher, cidfile);
  foundry_process_launcher_append_argv (launcher,
                                        execution->options->offline
                                          ? "--pull=never"
                                          : "--pull=missing");
  /* GitLab's Flatpak jobs expect to run bubblewrap inside the job container.
   * Rootless Podman still confines privileged containers to the caller's user
   * namespace, while allowing nested mount and PID namespaces to be created.
   */
  foundry_process_launcher_append_argv (launcher, "--privileged");

  if (execution->options->offline)
    foundry_process_launcher_append_argv (launcher, "--network=none");

  if (state->job->image_user != NULL)
    {
      foundry_process_launcher_append_argv (launcher, "--user");
      foundry_process_launcher_append_argv (launcher, state->job->image_user);
    }

  foundry_process_launcher_append_argv (launcher, "--volume");
  foundry_process_launcher_append_argv (launcher, workspace_mount);
  foundry_process_launcher_append_argv (launcher, "--volume");
  foundry_process_launcher_append_argv (launcher, control_mount);
  foundry_process_launcher_append_argv (launcher, "--workdir");
  foundry_process_launcher_append_argv (launcher, project_dir);

  append_environment (launcher, state->job);

  foundry_process_launcher_append_argv (launcher, "--entrypoint");
  foundry_process_launcher_append_argv (launcher, "/bin/sh");
  foundry_process_launcher_append_argv (launcher, state->job->image);
}

static void
cleanup_container (const char *cidfile)
{
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *container_id = NULL;
  const char *data;
  gsize length;

  g_assert (cidfile != NULL);

  file = g_file_new_for_path (cidfile);
  if (!(bytes = dex_await_boxed (dex_file_load_contents_bytes (file), NULL)))
    return;

  data = g_bytes_get_data (bytes, &length);
  container_id = g_strndup (data, length);
  g_strstrip (container_id);
  if (container_id[0] == '\0')
    return;

  launcher = foundry_process_launcher_new ();
  foundry_process_launcher_push_host (launcher);
  foundry_process_launcher_append_argv (launcher, "podman");
  foundry_process_launcher_append_argv (launcher, "rm");
  foundry_process_launcher_append_argv (launcher, "--force");
  foundry_process_launcher_append_argv (launcher, container_id);

  subprocess = foundry_process_launcher_spawn_with_flags (launcher,
                                                          (G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE),
                                                          &error);
  if (subprocess != NULL)
    dex_await (dex_subprocess_wait (subprocess), NULL);
}

static int
run_container (Execution  *execution,
               JobState   *state,
               GError    **error)
{
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(DexFuture) combined = NULL;
  g_autoptr(DexFuture) timed = NULL;
  g_autoptr(GPtrArray) secrets = NULL;
  g_autofree char *control_dir = NULL;
  g_autofree char *cidfile = NULL;
  g_autoptr(GError) local_error = NULL;
  PumpRequest *stdout_request;
  PumpRequest *stderr_request;
  int exit_status;

  g_assert (execution != NULL);
  g_assert (state != NULL);

  if (!resolve_image_identity (execution, state, error))
    return -1;

  if (!prepare_control (execution, state, &control_dir, error))
    return -1;

  cidfile = g_build_filename (control_dir, "container.cid", NULL);
  launcher = foundry_process_launcher_new ();
  foundry_process_launcher_push_host (launcher);
  append_podman_arguments (execution, state, launcher, control_dir, cidfile);

  if (execution->options->shell)
    {
      foundry_process_launcher_append_argv (launcher, "-i");

      if (execution->options->stdin_fd >= 0)
        foundry_process_launcher_take_fd (launcher, dup (execution->options->stdin_fd), STDIN_FILENO);

      if (execution->options->stdout_fd >= 0)
        foundry_process_launcher_take_fd (launcher, dup (execution->options->stdout_fd), STDOUT_FILENO);

      if (execution->options->stderr_fd >= 0)
        foundry_process_launcher_take_fd (launcher, dup (execution->options->stderr_fd), STDERR_FILENO);

      subprocess = foundry_process_launcher_spawn_with_flags (launcher,
                                                              execution->options->stdin_fd < 0 ? G_SUBPROCESS_FLAGS_STDIN_INHERIT : 0,
                                                              error);
      if (subprocess == NULL)
        return -1;

      if (!dex_await (dex_future_first (dex_subprocess_wait (subprocess),
                                        dex_ref (DEX_FUTURE (execution->cancellable)),
                                        NULL),
                      error))
        {
          g_subprocess_force_exit (subprocess);
          cleanup_container (cidfile);
          return -1;
        }

      return g_subprocess_get_if_exited (subprocess)
               ? g_subprocess_get_exit_status (subprocess)
               : 128 + g_subprocess_get_term_sig (subprocess);
    }

  foundry_process_launcher_append_argv (launcher, "-c");
  foundry_process_launcher_append_argv (launcher,
                                        "sh /gitlab/main.sh; status=$?; sh /gitlab/after.sh; exit \"$status\"");

  subprocess = foundry_process_launcher_spawn_with_flags (launcher,
                                                          (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE),
                                                          error);
  if (subprocess == NULL)
    return -1;

  secrets = collect_secrets (state->job);
  stdout_request = g_new0 (PumpRequest, 1);
  stdout_request->stream = g_object_ref (g_subprocess_get_stdout_pipe (subprocess));
  stdout_request->secrets = g_ptr_array_ref (secrets);
  stdout_request->job_name = g_strdup (state->job->name);
  stdout_request->fd = execution->options->stdout_fd >= 0
                     ? execution->options->stdout_fd
                     : STDOUT_FILENO;
  stderr_request = g_new0 (PumpRequest, 1);
  stderr_request->stream = g_object_ref (g_subprocess_get_stderr_pipe (subprocess));
  stderr_request->secrets = g_ptr_array_ref (secrets);
  stderr_request->job_name = g_strdup (state->job->name);
  stderr_request->fd = execution->options->stderr_fd >= 0
                     ? execution->options->stderr_fd
                     : STDERR_FILENO;

  combined = dex_future_all ( dex_subprocess_wait (subprocess),
                             dex_scheduler_spawn (NULL,
                                                  0,
                                                  pump_fiber,
                                                  stdout_request,
                                                  (GDestroyNotify)pump_request_free),
                             dex_scheduler_spawn (NULL,
                                                  0,
                                                  pump_fiber,
                                                  stderr_request,
                                                  (GDestroyNotify)pump_request_free),
                             NULL);

  timed = dex_future_with_timeout_seconds (g_steal_pointer (&combined),
                                           parse_timeout (state->job->timeout));

  if (!dex_await (dex_future_first (g_steal_pointer (&timed),
                                    dex_ref (DEX_FUTURE (execution->cancellable)),
                                    dex_ref (DEX_FUTURE (execution->stop)),
                                    NULL),
                  &local_error))
    {
      g_subprocess_force_exit (subprocess);
      cleanup_container (cidfile);

      if (!dex_future_is_pending (DEX_FUTURE (execution->cancellable)))
        {
          g_propagate_error (error,
                             g_error_new_literal (G_IO_ERROR,
                                                  G_IO_ERROR_CANCELLED,
                                                  "CI run was cancelled"));
          return -1;
        }

      if (!dex_future_is_pending (DEX_FUTURE (execution->stop)))
        return 1;

      if (g_error_matches (local_error, DEX_ERROR, DEX_ERROR_TIMED_OUT))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_TIMED_OUT,
                       "job '%s' exceeded its timeout",
                       state->job->name);
          return 124;
        }

      g_propagate_error (error, g_steal_pointer (&local_error));

      return -1;
    }

  exit_status = g_subprocess_get_if_exited (subprocess)
                  ? g_subprocess_get_exit_status (subprocess)
                  : 128 + g_subprocess_get_term_sig (subprocess);
  cleanup_container (cidfile);
  return exit_status;
}

static int
stage_index (PluginGitlabCiPipeline *pipeline,
             const char             *stage)
{
  g_assert (pipeline != NULL);
  g_assert (stage != NULL);

  for (guint i = 0; i < pipeline->stages->len; i++)
    {
      if (g_str_equal (g_ptr_array_index (pipeline->stages, i), stage))
        return i;
    }

  return -1;
}

static gboolean
await_state (Execution  *execution,
             JobState   *dependency,
             GError    **error)
{
  int exit_status;

  g_assert (execution != NULL);
  g_assert (dependency != NULL);
  g_assert (error != NULL);

  exit_status = dex_await_int (dex_future_first (dex_ref (DEX_FUTURE (dependency->completion)),
                                                 dex_ref (DEX_FUTURE (execution->cancellable)),
                                                 dex_ref (DEX_FUTURE (execution->stop)),
                                                 NULL),
                               error);
  if (*error != NULL)
    return FALSE;

  return exit_status == 0 || dependency->job->allow_failure;
}

static gboolean
await_dependencies (Execution  *execution,
                    JobState   *state,
                    GError    **error)
{
  g_assert (execution != NULL);
  g_assert (state != NULL);

  if (execution->options->shell)
    return TRUE;

  if (state->job->has_explicit_needs)
    {
      for (guint i = 0; i < state->job->needs->len; i++)
        {
          PluginGitlabCiNeed *need = g_ptr_array_index (state->job->needs, i);
          JobState *dependency = g_hash_table_lookup (execution->states, need->job);

          if (dependency != NULL && !await_state (execution, dependency, error))
            return FALSE;
        }
    }
  else
    {
      int own_stage = stage_index (execution->pipeline, state->job->stage);

      for (guint i = 0; i < execution->state_array->len; i++)
        {
          JobState *dependency = g_ptr_array_index (execution->state_array, i);

          if (dependency != state &&
              stage_index (execution->pipeline, dependency->job->stage) < own_stage &&
              !await_state (execution, dependency, error))
            return FALSE;
        }
    }

  return TRUE;
}

static gboolean
materialize_artifacts (Execution  *execution,
                       JobState   *state,
                       GError    **error)
{
  g_autoptr(GHashTable) copied = NULL;

  g_assert (execution != NULL);
  g_assert (state != NULL);

  copied = g_hash_table_new (g_str_hash, g_str_equal);

  for (guint i = 0; i < state->job->needs->len; i++)
    {
      PluginGitlabCiNeed *need = g_ptr_array_index (state->job->needs, i);
      JobState *dependency = g_hash_table_lookup (execution->states, need->job);
      g_autoptr(GFile) source = NULL;
      g_autoptr(GFile) destination = NULL;

      if (!need->artifacts || dependency == NULL || dependency->artifact_dir == NULL)
        continue;

      source = g_file_new_for_path (dependency->artifact_dir);
      destination = g_file_new_for_path (state->workspace);

      if (!copy_tree (source, destination, error))
        return FALSE;

      g_hash_table_add (copied, dependency->job->name);
    }

  for (guint i = 0; i < state->job->dependencies->len; i++)
    {
      const char *name = g_ptr_array_index (state->job->dependencies, i);
      JobState *dependency = g_hash_table_lookup (execution->states, name);
      g_autoptr(GFile) source = NULL;
      g_autoptr(GFile) destination = NULL;

      if (dependency == NULL ||
          dependency->artifact_dir == NULL ||
          g_hash_table_contains (copied, name))
        continue;

      source = g_file_new_for_path (dependency->artifact_dir);
      destination = g_file_new_for_path (state->workspace);
      if (!copy_tree (source, destination, error))
        return FALSE;
    }

  return TRUE;
}

static gboolean
artifact_matches (PluginGitlabCiJob *job,
                  const char        *relative)
{
  g_assert (job != NULL);
  g_assert (relative != NULL);

  for (guint i = 0; i < job->artifact_paths->len; i++)
    {
      const char *pattern = g_ptr_array_index (job->artifact_paths, i);
      g_autofree char *prefix = NULL;

      if (!safe_relative_path (pattern))
        continue;

      prefix = g_strconcat (pattern, G_DIR_SEPARATOR_S, NULL);
      if (g_pattern_match_simple (pattern, relative) ||
          g_str_has_prefix (relative, prefix))
        return TRUE;
    }

  return FALSE;
}

static gboolean
collect_artifacts_from (PluginGitlabCiJob  *job,
                        GFile              *root,
                        GFile              *directory,
                        GFile              *destination,
                        GError            **error)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  const char *attributes = (G_FILE_ATTRIBUTE_STANDARD_NAME ","
                            G_FILE_ATTRIBUTE_STANDARD_TYPE);
  GFileCopyFlags copy_flags = (G_FILE_COPY_OVERWRITE |
                               G_FILE_COPY_ALL_METADATA |
                               G_FILE_COPY_NOFOLLOW_SYMLINKS);

  g_assert (job != NULL);
  g_assert (G_IS_FILE (root));
  g_assert (G_IS_FILE (directory));
  g_assert (G_IS_FILE (destination));

  enumerator = dex_await_object (dex_file_enumerate_children (directory,
                                                              attributes,
                                                              G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                              G_PRIORITY_DEFAULT),
                                 error);
  if (enumerator == NULL)
    return FALSE;

  for (;;)
    {
      gpointer infos_ptr;

      infos_ptr = dex_await_boxed (dex_file_enumerator_next_files (enumerator, 64, G_PRIORITY_DEFAULT), error);
      if (infos_ptr == NULL && error != NULL && *error != NULL)
        return FALSE;

      if (infos_ptr == NULL)
        break;

      {
        g_autolist(GFileInfo) infos = infos_ptr;

        for (const GList *iter = infos; iter != NULL; iter = iter->next)
          {
            GFileInfo *info = iter->data;
            g_autoptr(GFile) child = NULL;
            g_autoptr(GFile) destination_child = NULL;
            g_autofree char *relative = NULL;
            const char *name = g_file_info_get_name (info);

            if (name == NULL)
              continue;

            child = g_file_get_child (directory, name);
            relative = g_file_get_relative_path (root, child);
            if (relative == NULL || !safe_relative_path (relative))
              continue;

            if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
              {
                if (!collect_artifacts_from (job, root, child, destination, error))
                  return FALSE;
              }
            else if (artifact_matches (job, relative))
              {
                g_autoptr(GFile) parent = NULL;

                destination_child = g_file_resolve_relative_path (destination, relative);
                parent = g_file_get_parent (destination_child);

                if (!dex_await (dex_file_make_directory_with_parents (parent), error) &&
                    !g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_EXISTS))
                  return FALSE;

                g_clear_error (error);

                if (!dex_await (dex_file_copy (child, destination_child, copy_flags, G_PRIORITY_DEFAULT), error))
                  return FALSE;
              }
          }
      }
    }

  return TRUE;
}

static gboolean
should_collect_artifacts (PluginGitlabCiJob *job,
                          int                exit_status)
{
  g_assert (job != NULL);

  if (job->artifact_paths->len == 0)
    return FALSE;

  if (g_str_equal (job->artifacts_when, "always"))
    return TRUE;

  if (g_str_equal (job->artifacts_when, "on_failure"))
    return exit_status != 0;

  return exit_status == 0;
}

static gboolean
collect_artifacts (Execution  *execution,
                   JobState   *state,
                   GError    **error)
{
  g_autofree char *name = NULL;
  g_autoptr(GFile) workspace = NULL;
  g_autoptr(GFile) destination = NULL;

  g_assert (execution != NULL);
  g_assert (state != NULL);

  if (!should_collect_artifacts (state->job, state->exit_status))
    return TRUE;

  name = safe_name (state->job->name);
  state->artifact_dir = g_build_filename (execution->artifact_root, name, NULL);
  if (!ensure_directory (state->artifact_dir, error))
    return FALSE;

  workspace = g_file_new_for_path (state->workspace);
  destination = g_file_new_for_path (state->artifact_dir);

  return collect_artifacts_from (state->job,
                                 workspace,
                                 workspace,
                                 destination,
                                 error);
}

static DexFuture *
execute_job_fiber (gpointer user_data)
{
  JobRequest *request = user_data;
  Execution *execution = request->execution;
  JobState *state = request->state;
  g_autoptr(GError) error = NULL;
  int exit_status = 1;

  g_assert (execution != NULL);
  g_assert (state != NULL);

  if (!copy_project (execution, state->workspace, &error) ||
      !materialize_artifacts (execution, state, &error))
    goto failure;

  for (int attempt = 0; attempt <= state->job->retry; attempt++)
    {
      g_clear_error (&error);
      exit_status = run_container (execution, state, &error);

      if (exit_status == 0 ||
          !dex_future_is_pending (DEX_FUTURE (execution->cancellable)) ||
          !dex_future_is_pending (DEX_FUTURE (execution->stop)))
        break;
    }

  if (error != NULL &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT) &&
      dex_future_is_pending (DEX_FUTURE (execution->cancellable)) &&
      dex_future_is_pending (DEX_FUTURE (execution->stop)))
    goto failure;

  g_clear_error (&error);
  state->exit_status = exit_status;
  state->status = exit_status == 0 ? JOB_PASSED : JOB_FAILED;
  if (!collect_artifacts (execution, state, &error))
    goto failure;

  return dex_future_new_for_int (exit_status);

failure:
  if (error != NULL)
    g_warning ("GitLab CI job '%s' failed: %s",
               state->job->name,
               error->message);

  state->exit_status = 1;
  state->status = JOB_FAILED;

  return dex_future_new_for_int (1);
}

static DexFuture *
job_fiber (gpointer user_data)
{
  JobRequest *request = user_data;
  Execution *execution = request->execution;
  JobState *state = request->state;
  JobRequest *execute_request;
  g_autoptr(GError) error = NULL;
  int exit_status;

  g_assert (execution != NULL);
  g_assert (state != NULL);

  if (!await_dependencies (execution, state, &error))
    {
      state->status = JOB_SKIPPED;
      state->exit_status = error != NULL ? 130 : 1;
      dex_promise_resolve_int (state->completion, state->exit_status);
      return dex_future_new_for_int (state->exit_status);
    }

  state->status = JOB_RUNNING;
  execute_request = g_new0 (JobRequest, 1);
  execute_request->execution = execution_ref (execution);
  execute_request->state = state;

  exit_status = dex_await_int (dex_limiter_run (execution->limiter,
                                                NULL,
                                                0,
                                                execute_job_fiber,
                                                execute_request,
                                                (GDestroyNotify)job_request_free),
                               &error);

  if (error != NULL)
    {
      state->status = JOB_FAILED;
      state->exit_status = 1;
    }
  else
    {
      state->exit_status = exit_status;
    }

  execution->n_finished++;

  if (state->status == JOB_FAILED && !state->job->allow_failure)
    {
      execution->n_failed++;

      if (execution->options->fail_fast &&
          dex_future_is_pending (DEX_FUTURE (execution->stop)))
        dex_cancellable_cancel (execution->stop);
    }

  if (execution->options->progress_func != NULL)
    execution->options->progress_func ((double)execution->n_finished / execution->state_array->len,
                                       execution->options->progress_data);

  dex_promise_resolve_int (state->completion, state->exit_status);

  return dex_future_new_for_int (state->exit_status);
}

static gboolean
add_selection (Execution          *execution,
               PluginGitlabCiJob  *job,
               GError            **error)
{
  int own_stage;

  g_assert (execution != NULL);
  g_assert (job != NULL);

  if (g_hash_table_contains (execution->selected, job->name))
    return TRUE;

  if (job->status != PLUGIN_GITLAB_CI_JOB_STATUS_SELECTED)
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                   "job '%s' is %s",
                   job->name,
                   plugin_gitlab_ci_job_status_to_string (job->status));
      return FALSE;
    }

  g_hash_table_add (execution->selected, job->name);

  for (guint i = 0; i < job->needs->len; i++)
    {
      PluginGitlabCiNeed *need = g_ptr_array_index (job->needs, i);
      PluginGitlabCiJob *dependency = plugin_gitlab_ci_pipeline_lookup_job (execution->pipeline, need->job);

      if (dependency != NULL &&
          dependency->status == PLUGIN_GITLAB_CI_JOB_STATUS_SELECTED &&
          !add_selection (execution, dependency, error))
        return FALSE;
    }

  for (guint i = 0; i < job->dependencies->len; i++)
    {
      const char *name = g_ptr_array_index (job->dependencies, i);
      PluginGitlabCiJob *dependency = plugin_gitlab_ci_pipeline_lookup_job (execution->pipeline, name);

      if (dependency != NULL &&
          dependency->status == PLUGIN_GITLAB_CI_JOB_STATUS_SELECTED &&
          !add_selection (execution, dependency, error))
        return FALSE;
    }

  if (job->has_explicit_needs || execution->options->shell)
    return TRUE;

  own_stage = stage_index (execution->pipeline, job->stage);

  for (guint i = 0; i < execution->pipeline->jobs->len; i++)
    {
      PluginGitlabCiJob *dependency = g_ptr_array_index (execution->pipeline->jobs, i);

      if (dependency->status == PLUGIN_GITLAB_CI_JOB_STATUS_SELECTED &&
          stage_index (execution->pipeline, dependency->stage) < own_stage &&
          !add_selection (execution, dependency, error))
        return FALSE;
    }

  return TRUE;
}

static gboolean
select_jobs (Execution  *execution,
             GError    **error)
{
  g_assert (execution != NULL);

  if (execution->options->job_names != NULL &&
      execution->options->job_names[0] != NULL)
    {
      for (guint i = 0; execution->options->job_names[i] != NULL; i++)
        {
          PluginGitlabCiJob *job = plugin_gitlab_ci_pipeline_lookup_job (execution->pipeline,
                                                                         execution->options->job_names[i]);

          if (job == NULL)
            {
              g_set_error (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_NOT_FOUND,
                           "unknown job '%s'",
                           execution->options->job_names[i]);
              return FALSE;
            }

          if (!add_selection (execution, job, error))
            return FALSE;
        }
    }
  else
    {
      for (guint i = 0; i < execution->pipeline->jobs->len; i++)
        {
          PluginGitlabCiJob *job = g_ptr_array_index (execution->pipeline->jobs, i);

          if (job->status == PLUGIN_GITLAB_CI_JOB_STATUS_SELECTED &&
              !add_selection (execution, job, error))
            return FALSE;
        }
    }

  if (g_hash_table_size (execution->selected) == 0)
    {
      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_NOT_FOUND,
                           "pipeline has no runnable jobs");
      return FALSE;
    }

  return TRUE;
}

static Execution *
execution_new (RunnerRequest  *request,
               GError        **error)
{
  g_autoptr(Execution) self = NULL;
  g_autofree char *uuid = NULL;
  const char *base;

  g_assert (request != NULL);

  self = g_new0 (Execution, 1);
  g_atomic_ref_count_init (&self->ref_count);
  self->context = plugin_gitlab_ci_context_ref (request->context);
  self->pipeline = g_object_ref (request->pipeline);
  self->options = request->options;
  self->cancellable = dex_ref (request->cancellable);
  self->stop = dex_cancellable_new ();
  self->limiter = dex_limiter_new (MAX (1, request->options->jobs));
  self->selected = g_hash_table_new (g_str_hash, g_str_equal);
  self->states = g_hash_table_new (g_str_hash, g_str_equal);
  self->state_array = g_ptr_array_new_with_free_func ((GDestroyNotify)job_state_free);

  if (!select_jobs (self, error))
    return NULL;

  if (request->options->shell && g_hash_table_size (self->selected) != 1)
    {
      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_INVALID_ARGUMENT,
                           "shell mode requires exactly one job");
      return NULL;
    }

  base = request->options->output_dir ? request->options->output_dir : g_get_tmp_dir ();

  uuid = g_uuid_string_random ();
  self->run_root = g_build_filename (base, uuid, NULL);
  self->workspace_root = g_build_filename (self->run_root, "workspaces", NULL);
  self->control_root = g_build_filename (self->run_root, "control", NULL);
  self->artifact_root = g_build_filename (self->run_root, "artifacts", NULL);

  if (!ensure_directory (self->workspace_root, error) ||
      !ensure_directory (self->control_root, error) ||
      !ensure_directory (self->artifact_root, error))
    return NULL;

  for (guint i = 0; i < self->pipeline->jobs->len; i++)
    {
      PluginGitlabCiJob *job = g_ptr_array_index (self->pipeline->jobs, i);
      g_autofree char *name = NULL;
      JobState *state;

      if (!g_hash_table_contains (self->selected, job->name))
        continue;

      name = safe_name (job->name);
      state = g_new0 (JobState, 1);
      state->job = job;
      state->completion = dex_promise_new ();
      state->workspace = g_build_filename (self->workspace_root, name, NULL);
      state->exit_status = -1;

      g_ptr_array_add (self->state_array, state);
      g_hash_table_insert (self->states, job->name, state);
    }

  return g_steal_pointer (&self);
}

static DexFuture *
runner_fiber (gpointer user_data)
{
  RunnerRequest *request = user_data;
  g_autoptr(Execution) execution = NULL;
  g_autoptr(GPtrArray) futures = NULL;
  g_autoptr(FoundryDirectoryReaper) reaper = NULL;
  g_autoptr(GError) error = NULL;
  gboolean cancelled;

  g_assert (request != NULL);

  if (!(execution = execution_new (request, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  futures = g_ptr_array_new ();
  for (guint i = 0; i < execution->state_array->len; i++)
    {
      JobRequest *job_request = g_new0 (JobRequest, 1);
      DexFuture *future;

      job_request->execution = execution_ref (execution);
      job_request->state = g_ptr_array_index (execution->state_array, i);
      future = dex_scheduler_spawn (NULL,
                                    0,
                                    job_fiber,
                                    job_request,
                                    (GDestroyNotify)job_request_free);
      g_ptr_array_add (futures, future);
    }

  if (!dex_await (foundry_future_all (futures), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  g_ptr_array_set_size (futures, 0);

  dex_await (dex_limiter_close_after_drain (execution->limiter), NULL);

  cancelled = !dex_future_is_pending (DEX_FUTURE (execution->cancellable));

  if (execution->options->shell)
    {
      g_autoptr(GFile) run_root = g_file_new_for_path (execution->run_root);

      reaper = foundry_directory_reaper_new ();
      foundry_directory_reaper_add_directory (reaper, run_root, 0);
      foundry_directory_reaper_add_file (reaper, run_root, 0);

      dex_await (foundry_directory_reaper_execute (reaper), NULL);
    }
  else if (!execution->options->save_workspace)
    {
      g_autoptr(GFile) workspaces = g_file_new_for_path (execution->workspace_root);
      g_autoptr(GFile) controls = g_file_new_for_path (execution->control_root);

      reaper = foundry_directory_reaper_new ();
      foundry_directory_reaper_add_directory (reaper, workspaces, 0);
      foundry_directory_reaper_add_file (reaper, workspaces, 0);
      foundry_directory_reaper_add_directory (reaper, controls, 0);
      foundry_directory_reaper_add_file (reaper, controls, 0);

      dex_await (foundry_directory_reaper_execute (reaper), NULL);
    }

  if (!execution->options->shell)
    {
      g_clear_pointer (&execution->options->result_output_dir, g_free);
      execution->options->result_output_dir = g_strdup (execution->run_root);
    }

  if (cancelled)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_CANCELLED,
                                  "CI run was cancelled");

  return dex_future_new_for_int (execution->n_failed > 0
                                   ? PLUGIN_GITLAB_CI_EXIT_JOB_FAILURE
                                   : PLUGIN_GITLAB_CI_EXIT_SUCCESS);
}

DexFuture *
plugin_gitlab_ci_runner_run (PluginGitlabCiContext    *context,
                             PluginGitlabCiPipeline   *pipeline,
                             PluginGitlabCiRunOptions *options,
                             DexCancellable           *cancellable)
{
  RunnerRequest *request;

  dex_return_error_if_fail (context != NULL);
  dex_return_error_if_fail (pipeline != NULL);
  dex_return_error_if_fail (options != NULL);
  dex_return_error_if_fail (DEX_IS_CANCELLABLE (cancellable));

  request = runner_request_new ();
  request->context = plugin_gitlab_ci_context_ref (context);
  request->pipeline = g_object_ref (pipeline);
  request->options = options;
  request->cancellable = dex_ref (cancellable);

  return dex_scheduler_spawn (NULL,
                              0,
                              runner_fiber,
                              request,
                              (GDestroyNotify)runner_request_free);
}
