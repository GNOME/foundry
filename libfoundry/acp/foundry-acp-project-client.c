/* foundry-acp-project-client.c
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-acp-client.h"
#include "foundry-acp-enums.h"
#include "foundry-acp-permission-policy.h"
#include "foundry-acp-project-client.h"
#include "foundry-acp-session.h"
#include "foundry-acp-session-private.h"
#include "foundry-acp-terminal-output.h"
#include "foundry-acp-terminal-private.h"
#include "foundry-context.h"
#include "foundry-process-launcher.h"
#include "foundry-service.h"
#include "foundry-util.h"
#include "foundry-vcs-manager.h"

#ifdef FOUNDRY_FEATURE_GIT
# include "foundry-git-status-entry.h"
# include "foundry-git-vcs.h"
#endif

struct _FoundryAcpProjectClient
{
  GObject parent_instance;
  FoundryContext *context;
  GFile *project_directory;
  FoundryAcpPermissionPolicy *permission_policy;
  FoundryVcsManager *vcs_manager;
  GHashTable *terminals;
  GHashTable *file_snapshot;
  guint next_terminal_id;
};

typedef struct _ProjectTerminal
{
  gatomicrefcount ref_count;
  GMutex mutex;
  FoundryAcpTerminal *terminal;
  GSubprocess *subprocess;
  GInputStream *stdout_stream;
  GString *pending_output;
  DexFuture *reader;
  DexFuture *waiter;
  char *id;
  char *exit_signal;
  gssize output_byte_limit;
  int exit_status;
  guint truncated : 1;
  guint has_exit_status : 1;
} ProjectTerminal;

typedef struct _FileSnapshot
{
  gint64 mtime_us;
  goffset size;
} FileSnapshot;

static void acp_client_iface_init (FoundryAcpClientInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryAcpProjectClient,
                               foundry_acp_project_client,
                               G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (FOUNDRY_TYPE_ACP_CLIENT, acp_client_iface_init))

static ProjectTerminal *
project_terminal_ref (ProjectTerminal *terminal)
{
  g_assert (terminal != NULL);

  g_atomic_ref_count_inc (&terminal->ref_count);

  return terminal;
}

static void
project_terminal_unref (ProjectTerminal *terminal)
{
  if (terminal != NULL)
    {
      if (g_atomic_ref_count_dec (&terminal->ref_count))
        {
          dex_clear (&terminal->reader);
          dex_clear (&terminal->waiter);
          g_clear_object (&terminal->terminal);
          g_clear_object (&terminal->subprocess);
          g_clear_object (&terminal->stdout_stream);
          if (terminal->pending_output != NULL)
            g_string_free (terminal->pending_output, TRUE);
          g_clear_pointer (&terminal->id, g_free);
          g_clear_pointer (&terminal->exit_signal, g_free);
          g_mutex_clear (&terminal->mutex);
          g_free (terminal);
        }
    }
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ProjectTerminal, project_terminal_unref)

static FileSnapshot *
file_snapshot_new (GFileInfo *info)
{
  g_autoptr(GDateTime) modified = NULL;
  FileSnapshot *snapshot;

  g_assert (G_IS_FILE_INFO (info));

  snapshot = g_new0 (FileSnapshot, 1);
  snapshot->size = g_file_info_get_size (info);

  if ((modified = g_file_info_get_modification_date_time (info)))
    snapshot->mtime_us = (g_date_time_to_unix (modified) * G_USEC_PER_SEC +
                          g_date_time_get_microsecond (modified));

  return snapshot;
}

static gboolean
file_snapshot_equal (const FileSnapshot *a,
                     const FileSnapshot *b)
{
  g_assert (a != NULL);
  g_assert (b != NULL);

  return a->size == b->size && a->mtime_us == b->mtime_us;
}

static void
foundry_acp_project_client_finalize (GObject *object)
{
  FoundryAcpProjectClient *self = (FoundryAcpProjectClient *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->project_directory);
  g_clear_object (&self->permission_policy);
  g_clear_object (&self->vcs_manager);
  g_clear_pointer (&self->terminals, g_hash_table_unref);
  g_clear_pointer (&self->file_snapshot, g_hash_table_unref);

  G_OBJECT_CLASS (foundry_acp_project_client_parent_class)->finalize (object);
}

static void
foundry_acp_project_client_class_init (FoundryAcpProjectClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_acp_project_client_finalize;
}

static void
foundry_acp_project_client_init (FoundryAcpProjectClient *self)
{
  self->terminals = g_hash_table_new_full (g_str_hash,
                                           g_str_equal,
                                           g_free,
                                           (GDestroyNotify)project_terminal_unref);
  self->file_snapshot = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  self->next_terminal_id = 1;
}

#ifdef FOUNDRY_FEATURE_GIT
static FoundryAcpChangedFileFlags
changed_file_flags_from_git_status_entry (FoundryGitStatusEntry *entry)
{
  FoundryAcpChangedFileFlags flags = FOUNDRY_ACP_CHANGED_FILE_NONE;

  g_assert (FOUNDRY_IS_GIT_STATUS_ENTRY (entry));

  if (foundry_git_status_entry_has_staged_changed (entry))
    flags |= FOUNDRY_ACP_CHANGED_FILE_STAGED;

  if (foundry_git_status_entry_has_unstaged_changed (entry))
    flags |= FOUNDRY_ACP_CHANGED_FILE_UNSTAGED;

  if (foundry_git_status_entry_is_new_file (entry))
    flags |= FOUNDRY_ACP_CHANGED_FILE_UNTRACKED;

  return flags;
}

static FoundryAcpChangedFileKind
changed_file_kind_from_git_status_entry (FoundryAcpProjectClient *self,
                                         FoundryGitStatusEntry   *entry,
                                         const char              *path)
{
  g_autoptr(GFile) file = NULL;

  g_assert (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  g_assert (FOUNDRY_IS_GIT_STATUS_ENTRY (entry));
  g_assert (path != NULL);

  if (foundry_git_status_entry_is_new_file (entry))
    return FOUNDRY_ACP_CHANGED_FILE_CREATED;

  file = g_file_resolve_relative_path (self->project_directory, path);

  if (!g_file_query_exists (file, NULL))
    return FOUNDRY_ACP_CHANGED_FILE_DELETED;

  return FOUNDRY_ACP_CHANGED_FILE_MODIFIED;
}

static void
refresh_changed_files_from_git_status (FoundryAcpProjectClient *self,
                                       FoundryAcpSession       *session,
                                       GListModel              *status)
{
  guint n_items;

  g_assert (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (G_IS_LIST_MODEL (status));

  n_items = g_list_model_get_n_items (status);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryGitStatusEntry) entry = NULL;
      g_autofree char *path = NULL;
      FoundryAcpChangedFileFlags flags;
      FoundryAcpChangedFileKind kind;

      if (!(entry = g_list_model_get_item (status, i)))
        continue;

      if (!(path = foundry_git_status_entry_dup_path (entry)))
        continue;

      flags = changed_file_flags_from_git_status_entry (entry);
      kind = changed_file_kind_from_git_status_entry (self, entry, path);

      _foundry_acp_session_note_changed_file (session, path, kind, NULL, flags);
    }
}
#endif

static gboolean
should_ignore_snapshot_child (const char *name)
{
  return g_strcmp0 (name, ".git") == 0 ||
         g_strcmp0 (name, ".foundry") == 0;
}

static void
snapshot_directory (GFile      *directory,
                    const char *prefix,
                    GHashTable *snapshot)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_FILE (directory));
  g_assert (snapshot != NULL);

  enumerator = g_file_enumerate_children (directory,
                                          (G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                           G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                           G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                           G_FILE_ATTRIBUTE_TIME_MODIFIED ","
                                           G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC),
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          NULL,
                                          &error);

  if (enumerator == NULL)
    return;

  for (;;)
    {
      g_autoptr(GFileInfo) info = NULL;
      g_autoptr(GFile) child = NULL;
      g_autofree char *path = NULL;
      const char *name;
      GFileType file_type;

      if (!(info = g_file_enumerator_next_file (enumerator, NULL, &error)))
        break;

      name = g_file_info_get_name (info);

      if (name == NULL || should_ignore_snapshot_child (name))
        continue;

      child = g_file_get_child (directory, name);

      if (prefix != NULL && prefix[0] != 0)
        path = g_build_filename (prefix, name, NULL);
      else
        path = g_strdup (name);

      file_type = g_file_info_get_file_type (info);

      if (file_type == G_FILE_TYPE_DIRECTORY)
        snapshot_directory (child, path, snapshot);
      else if (file_type == G_FILE_TYPE_REGULAR)
        g_hash_table_insert (snapshot, g_steal_pointer (&path), file_snapshot_new (info));
    }
}

static GHashTable *
project_client_take_file_snapshot (FoundryAcpProjectClient *self)
{
  GHashTable *snapshot;

  g_assert (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  g_assert (G_IS_FILE (self->project_directory));

  snapshot = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  snapshot_directory (self->project_directory, NULL, snapshot);

  return snapshot;
}

static void
project_client_refresh_changed_files_from_snapshot (FoundryAcpProjectClient *self,
                                                    FoundryAcpSession       *session)
{
  GHashTableIter iter;
  GHashTable *snapshot;
  gpointer key;
  gpointer value;

  g_assert (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));

  snapshot = project_client_take_file_snapshot (self);

  g_hash_table_iter_init (&iter, snapshot);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const char *path = key;
      FileSnapshot *current = value;
      FileSnapshot *previous = g_hash_table_lookup (self->file_snapshot, path);

      if (previous == NULL)
        _foundry_acp_session_note_changed_file (session,
                                                path,
                                                FOUNDRY_ACP_CHANGED_FILE_CREATED,
                                                NULL,
                                                FOUNDRY_ACP_CHANGED_FILE_UNSTAGED);
      else if (!file_snapshot_equal (current, previous))
        _foundry_acp_session_note_changed_file (session,
                                                path,
                                                FOUNDRY_ACP_CHANGED_FILE_MODIFIED,
                                                NULL,
                                                FOUNDRY_ACP_CHANGED_FILE_UNSTAGED);
    }

  g_hash_table_iter_init (&iter, self->file_snapshot);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const char *path = key;

      if (!g_hash_table_contains (snapshot, path))
        _foundry_acp_session_note_changed_file (session,
                                                path,
                                                FOUNDRY_ACP_CHANGED_FILE_DELETED,
                                                NULL,
                                                FOUNDRY_ACP_CHANGED_FILE_UNSTAGED);
    }

  g_hash_table_unref (self->file_snapshot);
  self->file_snapshot = snapshot;
}

static ProjectTerminal *
project_terminal_new (const char   *id,
                      gssize        output_byte_limit,
                      GSubprocess  *subprocess,
                      GInputStream *stdout_stream)
{
  ProjectTerminal *terminal;

  g_assert (id != NULL);
  g_assert (G_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_INPUT_STREAM (stdout_stream));

  terminal = g_new0 (ProjectTerminal, 1);
  g_atomic_ref_count_init (&terminal->ref_count);
  g_mutex_init (&terminal->mutex);
  terminal->id = g_strdup (id);
  terminal->terminal = foundry_acp_terminal_new (id);
  terminal->subprocess = g_object_ref (subprocess);
  terminal->stdout_stream = g_object_ref (stdout_stream);
  terminal->pending_output = g_string_new (NULL);
  terminal->output_byte_limit = output_byte_limit;
  _foundry_acp_terminal_set_output_byte_limit (terminal->terminal, output_byte_limit);

  return terminal;
}

static ProjectTerminal *
project_client_lookup_terminal (FoundryAcpProjectClient *self,
                                const char              *terminal_id)
{
  g_assert (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  g_assert (terminal_id != NULL);

  return g_hash_table_lookup (self->terminals, terminal_id);
}

static void
project_terminal_append_output (ProjectTerminal *terminal,
                                const char      *data,
                                gsize            len)
{
  g_autofree char *valid = NULL;
  gsize max_len;

  g_assert (terminal != NULL);

  if (len == 0)
    return;

  valid = g_utf8_make_valid (data, len);

  g_mutex_lock (&terminal->mutex);

  g_string_append (terminal->pending_output, valid);

  if (terminal->output_byte_limit > 0)
    max_len = terminal->output_byte_limit;
  else
    max_len = 64 * 1024;

  if (terminal->pending_output->len > max_len)
    {
      const char *str = terminal->pending_output->str;
      const char *cut = g_utf8_find_next_char (str + terminal->pending_output->len - max_len, NULL);

      if (cut == NULL)
        cut = str + terminal->pending_output->len - max_len;

      g_string_erase (terminal->pending_output, 0, cut - str);
      terminal->truncated = TRUE;
    }

  g_mutex_unlock (&terminal->mutex);
}

static DexFuture *
project_terminal_reader_fiber (ProjectTerminal *terminal)
{
  g_autoptr(ProjectTerminal) hold = terminal;
  g_autoptr(GError) error = NULL;

  g_assert (terminal != NULL);
  g_assert (G_IS_INPUT_STREAM (terminal->stdout_stream));

  for (;;)
    {
      g_autoptr(GBytes) bytes = NULL;
      gconstpointer data;
      gsize len;

      bytes = dex_await_boxed (dex_input_stream_read_bytes (terminal->stdout_stream,
                                                            4096,
                                                            G_PRIORITY_DEFAULT),
                               &error);

      if (bytes == NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      data = g_bytes_get_data (bytes, &len);

      if (len == 0)
        break;

      project_terminal_append_output (terminal, data, len);
    }

  return dex_future_new_true ();
}

static DexFuture *
project_terminal_wait_fiber (ProjectTerminal *terminal)
{
  g_autoptr(ProjectTerminal) hold = terminal;
  g_autoptr(GError) error = NULL;

  g_assert (terminal != NULL);
  g_assert (G_IS_SUBPROCESS (terminal->subprocess));

  if (!dex_await (dex_subprocess_wait (terminal->subprocess), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  g_mutex_lock (&terminal->mutex);

  terminal->has_exit_status = TRUE;

  if (g_subprocess_get_if_exited (terminal->subprocess))
    {
      terminal->exit_status = g_subprocess_get_exit_status (terminal->subprocess);
      g_clear_pointer (&terminal->exit_signal, g_free);
    }
  else if (g_subprocess_get_if_signaled (terminal->subprocess))
    {
      terminal->exit_status = -1;
      g_clear_pointer (&terminal->exit_signal, g_free);
      terminal->exit_signal = g_strdup_printf ("%d",
                                               g_subprocess_get_term_sig (terminal->subprocess));
    }

  g_mutex_unlock (&terminal->mutex);

  _foundry_acp_terminal_set_state (terminal->terminal, FOUNDRY_ACP_TERMINAL_EXITED);
  _foundry_acp_terminal_set_exited_at (terminal->terminal, g_get_real_time ());

  return dex_future_new_true ();
}

static FoundryAcpTerminalOutput *
project_terminal_dup_output (ProjectTerminal *terminal)
{
  g_autofree char *exit_signal = NULL;
  g_autofree char *text = NULL;
  gboolean has_exit_status;
  gboolean truncated;
  int exit_status;

  g_assert (terminal != NULL);

  g_mutex_lock (&terminal->mutex);

  text = g_strdup (terminal->pending_output->str);
  g_string_truncate (terminal->pending_output, 0);
  truncated = terminal->truncated;
  terminal->truncated = FALSE;
  has_exit_status = terminal->has_exit_status;
  exit_status = terminal->exit_status;
  exit_signal = g_strdup (terminal->exit_signal);

  g_mutex_unlock (&terminal->mutex);

  return foundry_acp_terminal_output_new (text ? text : "",
                                          truncated,
                                          has_exit_status,
                                          exit_status,
                                          exit_signal);
}

static DexFuture *
project_client_refresh_changed_files_fiber (FoundryAcpProjectClient *self,
                                            FoundryAcpSession       *session)
{
  g_autoptr(FoundryVcs) vcs = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));

  if (self->vcs_manager == NULL)
    {
      project_client_refresh_changed_files_from_snapshot (self, session);
      return dex_future_new_true ();
    }

  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (self->vcs_manager)), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(vcs = foundry_vcs_manager_dup_vcs (self->vcs_manager)))
    {
      project_client_refresh_changed_files_from_snapshot (self, session);
      return dex_future_new_true ();
    }

#ifdef FOUNDRY_FEATURE_GIT
  if (FOUNDRY_IS_GIT_VCS (vcs))
    {
      g_autoptr(GListModel) status = NULL;

      if (!(status = dex_await_object (foundry_git_vcs_list_status (FOUNDRY_GIT_VCS (vcs)), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      refresh_changed_files_from_git_status (self, session, status);
      g_hash_table_unref (self->file_snapshot);
      self->file_snapshot = project_client_take_file_snapshot (self);

      return dex_future_new_true ();
    }
#endif

  project_client_refresh_changed_files_from_snapshot (self, session);

  return dex_future_new_true ();
}

static GFile *
foundry_acp_project_client_dup_file_for_path (FoundryAcpProjectClient  *self,
                                              const char               *path,
                                              GError                  **error)
{
  g_autoptr(GFile) file = NULL;

  g_assert (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  g_assert (path != NULL);

  if (!foundry_acp_permission_policy_contains_path (self->permission_policy, path))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_PERMISSION_DENIED,
                   "Path `%s` is outside the project directory",
                   path);
      return NULL;
    }

  if (g_path_is_absolute (path))
    file = g_file_new_for_path (path);
  else
    file = g_file_resolve_relative_path (self->project_directory, path);

  return g_steal_pointer (&file);
}

static char *
slice_lines (const char *content,
             guint       line,
             guint       limit)
{
  const char *begin = content;
  const char *end = content + strlen (content);
  const char *iter = content;
  guint current_line = 1;
  guint lines_seen = 0;

  g_assert (content != NULL);

  if (line > 1)
    {
      while (*iter != '\0' && current_line < line)
        {
          if (*iter++ == '\n')
            current_line++;
        }

      begin = iter;
    }

  if (limit > 0)
    {
      iter = begin;

      while (*iter != '\0' && lines_seen < limit)
        {
          if (*iter++ == '\n')
            lines_seen++;
        }

      end = iter;
    }

  return g_strndup (begin, end - begin);
}

static DexFuture *
project_client_read_text_file_fiber (FoundryAcpProjectClient *self,
                                     FoundryAcpSession       *session,
                                     const char              *path,
                                     guint                    line,
                                     guint                    limit)
{
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree char *content = NULL;
  const char *data;
  gsize len;

  g_assert (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (path != NULL);

  if (!(file = foundry_acp_project_client_dup_file_for_path (self, path, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(bytes = dex_await_boxed (dex_file_load_contents_bytes (file), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  data = g_bytes_get_data (bytes, &len);

  if (!g_utf8_validate (data, len, NULL))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "File `%s` is not valid UTF-8",
                                  path);

  content = g_strndup (data, len);

  return dex_future_new_take_string (slice_lines (content, line, limit));
}

static DexFuture *
project_client_write_text_file_fiber (FoundryAcpProjectClient *self,
                                      FoundryAcpSession       *session,
                                      const char              *path,
                                      const char              *content)
{
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  FoundryAcpChangedFileKind kind;

  g_assert (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (path != NULL);
  g_assert (content != NULL);

  if (!g_utf8_validate (content, -1, NULL))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "Content for `%s` is not valid UTF-8",
                                  path);

  if (!(file = foundry_acp_project_client_dup_file_for_path (self, path, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  kind = g_file_query_exists (file, NULL) ?
    FOUNDRY_ACP_CHANGED_FILE_MODIFIED :
    FOUNDRY_ACP_CHANGED_FILE_CREATED;

  bytes = g_bytes_new (content, strlen (content));

  if (!dex_await (dex_file_replace_contents_bytes (file,
                                                   bytes,
                                                   NULL,
                                                   FALSE,
                                                   G_FILE_CREATE_REPLACE_DESTINATION),
                  &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  _foundry_acp_session_note_changed_file (session,
                                          path,
                                          kind,
                                          NULL,
                                          FOUNDRY_ACP_CHANGED_FILE_UNSTAGED);

  return dex_future_new_true ();
}

static DexFuture *
project_client_request_permission (FoundryAcpClient            *client,
                                   FoundryAcpSession           *session,
                                   FoundryAcpPermissionRequest *request)
{
  FoundryAcpProjectClient *self = FOUNDRY_ACP_PROJECT_CLIENT (client);

  g_assert (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (request != NULL);

  return foundry_acp_permission_policy_request_permission (self->permission_policy, request);
}

static DexFuture *
project_client_read_text_file (FoundryAcpClient  *client,
                               FoundryAcpSession *session,
                               const char        *path,
                               guint              line,
                               guint              limit)
{
  FoundryAcpProjectClient *self = FOUNDRY_ACP_PROJECT_CLIENT (client);

  g_assert (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (path != NULL);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (project_client_read_text_file_fiber),
                                  5,
                                  FOUNDRY_TYPE_ACP_PROJECT_CLIENT, self,
                                  FOUNDRY_TYPE_ACP_SESSION, session,
                                  G_TYPE_STRING, path,
                                  G_TYPE_UINT, line,
                                  G_TYPE_UINT, limit);
}

static DexFuture *
project_client_write_text_file (FoundryAcpClient  *client,
                                FoundryAcpSession *session,
                                const char        *path,
                                const char        *content)
{
  FoundryAcpProjectClient *self = FOUNDRY_ACP_PROJECT_CLIENT (client);

  g_assert (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (path != NULL);
  g_assert (content != NULL);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (project_client_write_text_file_fiber),
                                  4,
                                  FOUNDRY_TYPE_ACP_PROJECT_CLIENT, self,
                                  FOUNDRY_TYPE_ACP_SESSION, session,
                                  G_TYPE_STRING, path,
                                  G_TYPE_STRING, content);
}

static DexFuture *
project_client_create_terminal (FoundryAcpClient   *client,
                                FoundryAcpSession  *session,
                                const char         *command,
                                const char * const *argv,
                                const char         *cwd,
                                const char * const *environ,
                                gssize              output_byte_limit)
{
  FoundryAcpProjectClient *self = FOUNDRY_ACP_PROJECT_CLIENT (client);
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GFile) cwd_file = NULL;
  g_autofree char *cwd_path = NULL;
  g_autofree char *terminal_id = NULL;
  GInputStream *stdout_stream;
  ProjectTerminal *terminal;
  g_autoptr(GError) error = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));
  dex_return_error_if_fail (command != NULL);

  if (cwd != NULL)
    {
      if (!(cwd_file = foundry_acp_project_client_dup_file_for_path (self, cwd, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      cwd_path = g_file_get_path (cwd_file);
    }
  else
    {
      cwd_path = g_file_get_path (self->project_directory);
    }

  launcher = foundry_process_launcher_new ();
  foundry_process_launcher_set_cwd (launcher, cwd_path);
  foundry_process_launcher_append_argv (launcher, command);

  if (argv != NULL)
    foundry_process_launcher_append_args (launcher, argv);

  if (environ != NULL)
    foundry_process_launcher_add_environ (launcher, environ);

  if (!(subprocess = foundry_process_launcher_spawn_with_flags (launcher,
                                                                (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                                                 G_SUBPROCESS_FLAGS_STDERR_MERGE),
                                                                &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  stdout_stream = g_subprocess_get_stdout_pipe (subprocess);
  terminal_id = g_strdup_printf ("project-terminal-%u", self->next_terminal_id++);
  terminal = project_terminal_new (terminal_id, output_byte_limit, subprocess, stdout_stream);

  _foundry_acp_terminal_set_command (terminal->terminal, command);
  _foundry_acp_terminal_set_argv (terminal->terminal, argv);
  _foundry_acp_terminal_set_cwd (terminal->terminal, cwd_path);
  _foundry_acp_terminal_set_started_at (terminal->terminal, g_get_real_time ());
  _foundry_acp_terminal_set_state (terminal->terminal, FOUNDRY_ACP_TERMINAL_RUNNING);
  _foundry_acp_session_add_terminal (session, terminal->terminal);

  terminal->reader = foundry_scheduler_spawn (NULL, 0,
                                              G_CALLBACK (project_terminal_reader_fiber),
                                              1,
                                              G_TYPE_POINTER,
                                              project_terminal_ref (terminal));
  terminal->waiter = foundry_scheduler_spawn (NULL, 0,
                                              G_CALLBACK (project_terminal_wait_fiber),
                                              1,
                                              G_TYPE_POINTER,
                                              project_terminal_ref (terminal));

  g_hash_table_insert (self->terminals, g_strdup (terminal_id), terminal);

  return dex_future_new_for_object (terminal->terminal);
}

static DexFuture *
project_client_terminal_output (FoundryAcpClient  *client,
                                FoundryAcpSession *session,
                                const char        *terminal_id)
{
  FoundryAcpProjectClient *self = FOUNDRY_ACP_PROJECT_CLIENT (client);
  ProjectTerminal *terminal;
  g_autoptr(FoundryAcpTerminalOutput) output = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));
  dex_return_error_if_fail (terminal_id != NULL);

  if (!(terminal = project_client_lookup_terminal (self, terminal_id)))
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_RESOURCE_NOT_FOUND,
                                  "No such terminal `%s`", terminal_id);

  output = project_terminal_dup_output (terminal);
  _foundry_acp_terminal_apply_output (terminal->terminal, output);

  return dex_future_new_take_object (g_steal_pointer (&output));
}

static DexFuture *
project_client_terminal_wait_for_exit_fiber (FoundryAcpProjectClient *self,
                                             FoundryAcpSession       *session,
                                             const char              *terminal_id)
{
  ProjectTerminal *terminal;

  g_assert (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (terminal_id != NULL);

  if (!(terminal = project_client_lookup_terminal (self, terminal_id)))
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_RESOURCE_NOT_FOUND,
                                  "No such terminal `%s`", terminal_id);

  dex_await (dex_ref (terminal->waiter), NULL);
  dex_await (dex_ref (terminal->reader), NULL);
  dex_await (foundry_acp_project_client_refresh_changed_files (self, session), NULL);

  return dex_future_new_true ();
}

static DexFuture *
project_client_terminal_wait_for_exit (FoundryAcpClient  *client,
                                       FoundryAcpSession *session,
                                       const char        *terminal_id)
{
  FoundryAcpProjectClient *self = FOUNDRY_ACP_PROJECT_CLIENT (client);

  dex_return_error_if_fail (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));
  dex_return_error_if_fail (terminal_id != NULL);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (project_client_terminal_wait_for_exit_fiber),
                                  3,
                                  FOUNDRY_TYPE_ACP_PROJECT_CLIENT, self,
                                  FOUNDRY_TYPE_ACP_SESSION, session,
                                  G_TYPE_STRING, terminal_id);
}

static DexFuture *
project_client_terminal_kill_fiber (FoundryAcpProjectClient *self,
                                    FoundryAcpSession       *session,
                                    const char              *terminal_id)
{
  ProjectTerminal *terminal;

  g_assert (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (terminal_id != NULL);

  if (!(terminal = project_client_lookup_terminal (self, terminal_id)))
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_RESOURCE_NOT_FOUND,
                                  "No such terminal `%s`", terminal_id);

  g_subprocess_force_exit (terminal->subprocess);
  dex_await (dex_ref (terminal->waiter), NULL);
  dex_await (dex_ref (terminal->reader), NULL);
  _foundry_acp_terminal_set_state (terminal->terminal, FOUNDRY_ACP_TERMINAL_CANCELLED);
  dex_await (foundry_acp_project_client_refresh_changed_files (self, session), NULL);

  return dex_future_new_true ();
}

static DexFuture *
project_client_terminal_kill (FoundryAcpClient  *client,
                              FoundryAcpSession *session,
                              const char        *terminal_id)
{
  FoundryAcpProjectClient *self = FOUNDRY_ACP_PROJECT_CLIENT (client);

  dex_return_error_if_fail (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));
  dex_return_error_if_fail (terminal_id != NULL);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (project_client_terminal_kill_fiber),
                                  3,
                                  FOUNDRY_TYPE_ACP_PROJECT_CLIENT, self,
                                  FOUNDRY_TYPE_ACP_SESSION, session,
                                  G_TYPE_STRING, terminal_id);
}

static DexFuture *
project_client_terminal_release (FoundryAcpClient  *client,
                                 FoundryAcpSession *session,
                                 const char        *terminal_id)
{
  FoundryAcpProjectClient *self = FOUNDRY_ACP_PROJECT_CLIENT (client);
  ProjectTerminal *terminal;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));
  dex_return_error_if_fail (terminal_id != NULL);

  if (!(terminal = project_client_lookup_terminal (self, terminal_id)))
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_RESOURCE_NOT_FOUND,
                                  "No such terminal `%s`", terminal_id);

  if (!terminal->has_exit_status)
    g_subprocess_force_exit (terminal->subprocess);

  _foundry_acp_session_remove_terminal (session, terminal_id);
  g_hash_table_remove (self->terminals, terminal_id);

  return dex_future_new_true ();
}

static DexFuture *
project_client_refresh_changed_files (FoundryAcpClient  *client,
                                      FoundryAcpSession *session)
{
  FoundryAcpProjectClient *self = FOUNDRY_ACP_PROJECT_CLIENT (client);

  dex_return_error_if_fail (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));

  return foundry_acp_project_client_refresh_changed_files (self, session);
}

static void
acp_client_iface_init (FoundryAcpClientInterface *iface)
{
  iface->request_permission = project_client_request_permission;
  iface->read_text_file = project_client_read_text_file;
  iface->write_text_file = project_client_write_text_file;
  iface->create_terminal = project_client_create_terminal;
  iface->terminal_output = project_client_terminal_output;
  iface->terminal_wait_for_exit = project_client_terminal_wait_for_exit;
  iface->terminal_kill = project_client_terminal_kill;
  iface->terminal_release = project_client_terminal_release;
  iface->refresh_changed_files = project_client_refresh_changed_files;
}

/**
 * foundry_acp_project_client_new:
 * @context: a [class@Foundry.Context]
 *
 * Creates a project-scoped ACP client for @context.
 *
 * Returns: (transfer full): a [class@Foundry.AcpProjectClient]
 *
 * Since: 1.2
 */
FoundryAcpProjectClient *
foundry_acp_project_client_new (FoundryContext *context)
{
  g_autoptr(GFile) project_directory = NULL;
  FoundryAcpProjectClient *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);

  project_directory = foundry_context_dup_project_directory (context);
  self = foundry_acp_project_client_new_for_project_directory (project_directory);
  self->context = g_object_ref (context);
  self->vcs_manager = foundry_context_dup_vcs_manager (context);

  return self;
}

/**
 * foundry_acp_project_client_new_for_project_directory:
 * @project_directory: the project root used for containment checks
 *
 * Creates a project-scoped ACP client using @project_directory.
 *
 * Returns: (transfer full): a [class@Foundry.AcpProjectClient]
 *
 * Since: 1.2
 */
FoundryAcpProjectClient *
foundry_acp_project_client_new_for_project_directory (GFile *project_directory)
{
  FoundryAcpProjectClient *self;

  g_return_val_if_fail (G_IS_FILE (project_directory), NULL);

  self = g_object_new (FOUNDRY_TYPE_ACP_PROJECT_CLIENT, NULL);
  self->project_directory = g_object_ref (project_directory);
  self->permission_policy =
    foundry_acp_permission_policy_new_for_project_directory (project_directory);
  g_hash_table_unref (self->file_snapshot);
  self->file_snapshot = project_client_take_file_snapshot (self);

  return self;
}

/**
 * foundry_acp_project_client_dup_project_directory:
 * @self: a [class@Foundry.AcpProjectClient]
 *
 * Returns: (transfer full): the project directory used for containment checks
 *
 * Since: 1.2
 */
GFile *
foundry_acp_project_client_dup_project_directory (FoundryAcpProjectClient *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PROJECT_CLIENT (self), NULL);

  return g_object_ref (self->project_directory);
}

/**
 * foundry_acp_project_client_dup_permission_policy:
 * @self: a [class@Foundry.AcpProjectClient]
 *
 * Returns: (transfer full): the permission policy used by @self
 *
 * Since: 1.2
 */
FoundryAcpPermissionPolicy *
foundry_acp_project_client_dup_permission_policy (FoundryAcpProjectClient *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PROJECT_CLIENT (self), NULL);

  return g_object_ref (self->permission_policy);
}

/**
 * foundry_acp_project_client_refresh_changed_files:
 * @self: a [class@Foundry.AcpProjectClient]
 * @session: a [class@Foundry.AcpSession]
 *
 * Refreshes @session's changed-file model from the active project VCS.
 *
 * This currently uses Git status data when the active VCS is
 * [class@Foundry.GitVcs]. Other VCS implementations are ignored until they
 * expose a list-status API.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when refresh
 *   has completed
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_project_client_refresh_changed_files (FoundryAcpProjectClient *self,
                                                  FoundryAcpSession       *session)
{
  dex_return_error_if_fail (FOUNDRY_IS_ACP_PROJECT_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (project_client_refresh_changed_files_fiber),
                                  2,
                                  FOUNDRY_TYPE_ACP_PROJECT_CLIENT, self,
                                  FOUNDRY_TYPE_ACP_SESSION, session);
}
