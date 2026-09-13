/* foundry-git-delta.c
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

#include "foundry-git-autocleanups.h"
#include "foundry-git-delta-private.h"
#include "foundry-git-diff-hunk-private.h"
#include "foundry-git-diff-private.h"
#include "foundry-git-error.h"
#include "foundry-git-patch-private.h"
#include "foundry-git-private.h"
#include "foundry-util.h"
#include "foundry-vcs-content-private.h"

#include "foundry-trace-private.h"

struct _FoundryGitDelta
{
  GObject parent_instance;

  FoundryGitDiff *diff;
  gsize delta_idx;

  char *old_path;
  char *new_path;

  git_oid old_oid;
  git_oid new_oid;

  guint old_mode;
  guint new_mode;
  FoundryVcsDeltaStatus status;
  guint context_lines;
};

G_DEFINE_FINAL_TYPE (FoundryGitDelta, foundry_git_delta, FOUNDRY_TYPE_VCS_DELTA)

static char *
foundry_git_delta_dup_old_path (FoundryVcsDelta *delta)
{
  return g_strdup (FOUNDRY_GIT_DELTA (delta)->old_path);
}

static char *
foundry_git_delta_dup_new_path (FoundryVcsDelta *delta)
{
  return g_strdup (FOUNDRY_GIT_DELTA (delta)->new_path);
}

static char *
foundry_git_delta_dup_old_id (FoundryVcsDelta *delta)
{
  FoundryGitDelta *self = FOUNDRY_GIT_DELTA (delta);

  return _foundry_git_oid_dup_string (&self->old_oid);
}

static char *
foundry_git_delta_dup_new_id (FoundryVcsDelta *delta)
{
  FoundryGitDelta *self = FOUNDRY_GIT_DELTA (delta);

  return _foundry_git_oid_dup_string (&self->new_oid);
}

static guint
foundry_git_delta_get_old_mode (FoundryVcsDelta *delta)
{
  return FOUNDRY_GIT_DELTA (delta)->old_mode;
}

static guint
foundry_git_delta_get_new_mode (FoundryVcsDelta *delta)
{
  return FOUNDRY_GIT_DELTA (delta)->new_mode;
}

static FoundryVcsDeltaStatus
foundry_git_delta_get_status (FoundryVcsDelta *delta)
{
  return FOUNDRY_GIT_DELTA (delta)->status;
}

static void
foundry_git_delta_finalize (GObject *object)
{
  FoundryGitDelta *self = (FoundryGitDelta *)object;

  g_clear_object (&self->diff);
  g_clear_pointer (&self->old_path, g_free);
  g_clear_pointer (&self->new_path, g_free);

  G_OBJECT_CLASS (foundry_git_delta_parent_class)->finalize (object);
}

static gboolean
file_info_equal (GFileInfo *a,
                 GFileInfo *b)
{
  const char *a_etag;
  const char *b_etag;

  g_assert (G_IS_FILE_INFO (a));
  g_assert (G_IS_FILE_INFO (b));

  if (g_file_info_get_size (a) != g_file_info_get_size (b))
    return FALSE;

  if (g_file_info_get_attribute_uint64 (a, G_FILE_ATTRIBUTE_TIME_MODIFIED) !=
      g_file_info_get_attribute_uint64 (b, G_FILE_ATTRIBUTE_TIME_MODIFIED))
    return FALSE;

  if (g_file_info_get_attribute_uint32 (a, G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC) !=
      g_file_info_get_attribute_uint32 (b, G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC))
    return FALSE;

  a_etag = g_file_info_get_etag (a);
  b_etag = g_file_info_get_etag (b);

  return g_strcmp0 (a_etag, b_etag) == 0;
}

static gboolean
capture_worktree_content (FoundryGitRepositoryPaths  *paths,
                          const char                 *path,
                          GBytes                    **bytes,
                          char                      **id,
                          GError                    **error)
{
  static const char *attributes = G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                  G_FILE_ATTRIBUTE_TIME_MODIFIED ","
                                  G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC ","
                                  G_FILE_ATTRIBUTE_ETAG_VALUE;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFileInfo) before = NULL;
  g_autoptr(GFileInfo) after = NULL;
  g_autoptr(GBytes) local_bytes = NULL;
  g_autofree char *local_id = NULL;
  guint64 mtime;
  guint32 usec;

  g_assert (paths != NULL);
  g_assert (path != NULL);
  g_assert (bytes != NULL);
  g_assert (id != NULL);

  file = foundry_git_repository_paths_get_workdir_file (paths, path);
  before = g_file_query_info (file, attributes, G_FILE_QUERY_INFO_NONE, NULL, error);

  if (before == NULL)
    return FALSE;

  if (!(local_bytes = g_file_load_bytes (file, NULL, NULL, error)))
    return FALSE;

  after = g_file_query_info (file, attributes, G_FILE_QUERY_INFO_NONE, NULL, error);

  if (after == NULL)
    return FALSE;

  if (!file_info_equal (before, after))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_BUSY,
                   "Working tree file '%s' changed while reading",
                   path);
      return FALSE;
    }

  mtime = g_file_info_get_attribute_uint64 (after, G_FILE_ATTRIBUTE_TIME_MODIFIED);
  usec = g_file_info_get_attribute_uint32 (after, G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC);
  local_id = g_strdup_printf ("worktree:%s:%"G_GUINT64_FORMAT":%"G_GUINT64_FORMAT":%u",
                              path,
                              (guint64)g_bytes_get_size (local_bytes),
                              mtime,
                              usec);

  *bytes = g_steal_pointer (&local_bytes);
  *id = g_steal_pointer (&local_id);

  return TRUE;
}

static gboolean
load_blob_content (git_repository  *repository,
                   const git_oid   *oid,
                   GBytes         **bytes,
                   char           **id,
                   GError         **error)
{
  g_autoptr(git_blob) blob = NULL;

  g_assert (repository != NULL);
  g_assert (oid != NULL);
  g_assert (bytes != NULL);
  g_assert (id != NULL);

  if (git_blob_lookup (&blob, repository, oid) != 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "%s",
                   git_error_last () ? git_error_last ()->message : "Failed to load blob");
      return FALSE;
    }

  *bytes = g_bytes_new (git_blob_rawcontent (blob), git_blob_rawsize (blob));
  *id = _foundry_git_oid_dup_string (oid);

  return TRUE;
}

static gboolean
resolve_delta_side (FoundryGitDelta  *self,
                    git_repository   *repository,
                    FoundryVcsDeltaSide side,
                    gboolean         *present,
                    GBytes          **bytes,
                    char            **id,
                    GError          **error)
{
  g_autoptr(FoundryGitRepositoryPaths) paths = NULL;
  const git_diff_delta *delta;
  const git_diff_file *file;
  FoundryGitDiffEndpointKind kind;

  g_assert (FOUNDRY_IS_GIT_DELTA (self));
  g_assert (repository != NULL);
  g_assert (present != NULL);
  g_assert (bytes != NULL);
  g_assert (id != NULL);

  delta = _foundry_git_diff_get_delta (self->diff, self->delta_idx);
  if (delta == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "%s",
                   git_error_last () ? git_error_last ()->message : "Failed to load delta");
      return FALSE;
    }

  file = side == FOUNDRY_VCS_DELTA_SIDE_OLD ? &delta->old_file : &delta->new_file;
  kind = side == FOUNDRY_VCS_DELTA_SIDE_OLD ?
         _foundry_git_diff_get_old_kind (self->diff) :
         _foundry_git_diff_get_new_kind (self->diff);

  *present = file->path != NULL && file->mode != 0;

  if (!*present)
    {
      *bytes = NULL;
      *id = g_strdup ("absent");
      return TRUE;
    }

  if (kind == FOUNDRY_GIT_DIFF_ENDPOINT_WORKTREE)
    {
      paths = _foundry_git_diff_dup_paths (self->diff);
      return capture_worktree_content (paths, file->path, bytes, id, error);
    }

  if (git_oid_is_zero (&file->id))
    {
      *present = FALSE;
      *bytes = NULL;
      *id = g_strdup ("absent");
      return TRUE;
    }

  return load_blob_content (repository, &file->id, bytes, id, error);
}

static FoundryGitPatch *
foundry_git_delta_create_patch (FoundryGitDelta  *self,
                                guint             context_lines,
                                GError          **error)
{
  g_autoptr(FoundryGitRepositoryPaths) paths = NULL;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_patch) patch = NULL;
  g_autoptr(GBytes) old_bytes = NULL;
  g_autoptr(GBytes) new_bytes = NULL;
  g_autofree char *old_id = NULL;
  g_autofree char *new_id = NULL;
  const git_diff_delta *delta;
  const void *old_buf = NULL;
  const void *new_buf = NULL;
  gsize old_len = 0;
  gsize new_len = 0;
  gboolean old_present = FALSE;
  gboolean new_present = FALSE;
  git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;

  g_assert (FOUNDRY_IS_GIT_DELTA (self));
  g_assert (error != NULL);

  delta = _foundry_git_diff_get_delta (self->diff, self->delta_idx);
  if (delta == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "%s",
                   git_error_last () ? git_error_last ()->message : "Failed to load delta");
      return NULL;
    }

  paths = _foundry_git_diff_dup_paths (self->diff);

  if (!foundry_git_repository_paths_open (paths, &repository, error))
    return NULL;

  if (!resolve_delta_side (self,
                           repository,
                           FOUNDRY_VCS_DELTA_SIDE_OLD,
                           &old_present,
                           &old_bytes,
                           &old_id,
                           error) ||
      !resolve_delta_side (self,
                           repository,
                           FOUNDRY_VCS_DELTA_SIDE_NEW,
                           &new_present,
                           &new_bytes,
                           &new_id,
                           error))
    return NULL;

  if (old_present && old_bytes != NULL)
    old_buf = g_bytes_get_data (old_bytes, &old_len);

  if (new_present && new_bytes != NULL)
    new_buf = g_bytes_get_data (new_bytes, &new_len);

  diff_opts.context_lines = context_lines;

  if (git_patch_from_buffers (&patch,
                              old_buf, old_len, delta->old_file.path,
                              new_buf, new_len, delta->new_file.path,
                              &diff_opts) != 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "%s",
                   git_error_last () ? git_error_last ()->message : "Failed to create patch");
      return NULL;
    }

  return _foundry_git_patch_new_with_two_bytes (g_steal_pointer (&patch),
                                                g_steal_pointer (&old_bytes),
                                                g_steal_pointer (&new_bytes));
}

static DexFuture *
foundry_git_delta_list_hunks_thread (gpointer data)
{
  FoundryGitDelta *self = data;
  g_autoptr(FoundryGitPatch) git_patch = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GError) error = NULL;
  gsize num_hunks;

  g_assert (FOUNDRY_IS_GIT_DELTA (self));
  g_assert (FOUNDRY_IS_GIT_DIFF (self->diff));

  FOUNDRY_TRACE_SCOPE_FUNC ();

  store = g_list_store_new (FOUNDRY_TYPE_GIT_DIFF_HUNK);

  if (!(git_patch = foundry_git_delta_create_patch (self, self->context_lines, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  num_hunks = _foundry_git_patch_get_num_hunks (git_patch);

  if (num_hunks >= G_MAXUINT)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Too many hunks in patch");

  for (gsize i = 0; i < num_hunks; i++)
    {
      g_autoptr(FoundryGitDiffHunk) hunk = _foundry_git_diff_hunk_new (git_patch, i);

      g_list_store_append (store, hunk);
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static DexFuture *
foundry_git_delta_list_hunks (FoundryVcsDelta *delta)
{
  FoundryGitDelta *self = FOUNDRY_GIT_DELTA (delta);

  g_assert (FOUNDRY_IS_GIT_DELTA (self));

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-delta-list-hunks]",
                                 foundry_git_delta_list_hunks_thread,
                                 g_object_ref (self),
                                 g_object_unref);
}

typedef struct
{
  FoundryGitDelta *delta;
  guint context_lines;
} SerializeData;

static void
serialize_data_free (gpointer data)
{
  SerializeData *serialize_data = data;

  g_object_unref (serialize_data->delta);
  g_free (serialize_data);
}

static DexFuture *
foundry_git_delta_serialize_thread (gpointer data)
{
  SerializeData *serialize_data = data;
  FoundryGitDelta *self = serialize_data->delta;
  guint context_lines = serialize_data->context_lines;
  g_autoptr(GString) diff_text = NULL;
  g_autoptr(FoundryGitPatch) git_patch = NULL;
  g_autoptr(GError) error = NULL;
  gsize num_hunks;
  g_autofree char *old_path_dup = NULL;
  g_autofree char *new_path_dup = NULL;
  FoundryVcsDeltaStatus status;

  g_assert (FOUNDRY_IS_GIT_DELTA (self));
  g_assert (FOUNDRY_IS_GIT_DIFF (self->diff));

  FOUNDRY_TRACE_SCOPE_FUNC ();

  diff_text = g_string_new (NULL);

  old_path_dup = foundry_vcs_delta_dup_old_path (FOUNDRY_VCS_DELTA (self));
  new_path_dup = foundry_vcs_delta_dup_new_path (FOUNDRY_VCS_DELTA (self));
  status = foundry_vcs_delta_get_status (FOUNDRY_VCS_DELTA (self));

  /* Print diff header */
  if (status == FOUNDRY_VCS_DELTA_STATUS_RENAMED)
    g_string_append_printf (diff_text, "diff --git a/%s b/%s\n",
                            old_path_dup ? old_path_dup : "/dev/null",
                            new_path_dup ? new_path_dup : "/dev/null");
  else if (status == FOUNDRY_VCS_DELTA_STATUS_DELETED)
    g_string_append_printf (diff_text, "diff --git a/%s b/%s\n",
                            old_path_dup ? old_path_dup : "/dev/null",
                            "/dev/null");
  else if (status == FOUNDRY_VCS_DELTA_STATUS_ADDED)
    g_string_append_printf (diff_text, "diff --git a/%s b/%s\n",
                            "/dev/null",
                            new_path_dup ? new_path_dup : "/dev/null");
  else
    g_string_append_printf (diff_text, "diff --git a/%s b/%s\n",
                            old_path_dup ? old_path_dup : "/dev/null",
                            new_path_dup ? new_path_dup : "/dev/null");

  if (old_path_dup && new_path_dup && g_strcmp0 (old_path_dup, new_path_dup) != 0)
    g_string_append_printf (diff_text, "rename from %s\nrename to %s\n", old_path_dup, new_path_dup);

  if (!(git_patch = foundry_git_delta_create_patch (self, context_lines, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  num_hunks = _foundry_git_patch_get_num_hunks (git_patch);

  /* Print each hunk */
  for (gsize i = 0; i < num_hunks; i++)
    {
      const git_diff_hunk *ghunk;
      gsize num_lines;

      ghunk = _foundry_git_patch_get_hunk (git_patch, i);
      if (ghunk != NULL)
        g_string_append (diff_text, ghunk->header);

      num_lines = _foundry_git_patch_get_num_lines_in_hunk (git_patch, i);

      for (gsize j = 0; j < num_lines; j++)
        {
          const git_diff_line *gline;

          gline = _foundry_git_patch_get_line (git_patch, i, j);
          if (gline != NULL)
            {
              if (g_ascii_isprint (gline->origin))
                g_string_append_c (diff_text, (char)gline->origin);

              if (gline->content != NULL && gline->content_len > 0)
                g_string_append_len (diff_text, gline->content, gline->content_len);

              if (gline->content_len == 0 || gline->content[gline->content_len - 1] != '\n')
                g_string_append_c (diff_text, '\n');
            }
        }
    }

  return dex_future_new_take_string (g_string_free (g_steal_pointer (&diff_text), FALSE));
}

typedef struct
{
  FoundryGitDelta *delta;
  FoundryVcsDeltaSide side;
} OpenContentData;

static void
open_content_data_free (gpointer data)
{
  OpenContentData *open_data = data;

  g_object_unref (open_data->delta);
  g_free (open_data);
}

static DexFuture *
foundry_git_delta_open_content_thread (gpointer data)
{
  OpenContentData *open_data = data;
  FoundryGitDelta *self = open_data->delta;
  g_autoptr(FoundryGitRepositoryPaths) paths = NULL;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *id = NULL;
  gboolean present = FALSE;

  g_assert (FOUNDRY_IS_GIT_DELTA (self));

  FOUNDRY_TRACE_SCOPE_FUNC ();

  paths = _foundry_git_diff_dup_paths (self->diff);

  if (!foundry_git_repository_paths_open (paths, &repository, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!resolve_delta_side (self,
                           repository,
                           open_data->side,
                           &present,
                           &bytes,
                           &id,
                           &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!present)
    return dex_future_new_take_object (_foundry_vcs_content_new_absent (id));

  return dex_future_new_take_object (_foundry_vcs_content_new_present (id, bytes));
}

static DexFuture *
foundry_git_delta_open_content (FoundryVcsDelta     *delta,
                                FoundryVcsDeltaSide  side)
{
  FoundryGitDelta *self = FOUNDRY_GIT_DELTA (delta);
  OpenContentData *open_data;

  g_assert (FOUNDRY_IS_GIT_DELTA (self));

  open_data = g_new0 (OpenContentData, 1);
  open_data->delta = g_object_ref (self);
  open_data->side = side;

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-delta-open-content]",
                                 foundry_git_delta_open_content_thread,
                                 open_data,
                                 open_content_data_free);
}

static DexFuture *
foundry_git_delta_serialize (FoundryVcsDelta *delta,
                             guint            context_lines)
{
  FoundryGitDelta *self = FOUNDRY_GIT_DELTA (delta);
  SerializeData *serialize_data;

  g_assert (FOUNDRY_IS_GIT_DELTA (self));

  serialize_data = g_new0 (SerializeData, 1);
  serialize_data->delta = g_object_ref (self);
  serialize_data->context_lines = context_lines;

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-delta-serialize]",
                                 foundry_git_delta_serialize_thread,
                                 serialize_data,
                                 serialize_data_free);
}

static void
foundry_git_delta_class_init (FoundryGitDeltaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsDeltaClass *vcs_delta_class = FOUNDRY_VCS_DELTA_CLASS (klass);

  object_class->finalize = foundry_git_delta_finalize;

  vcs_delta_class->dup_old_path = foundry_git_delta_dup_old_path;
  vcs_delta_class->dup_new_path = foundry_git_delta_dup_new_path;
  vcs_delta_class->dup_old_id = foundry_git_delta_dup_old_id;
  vcs_delta_class->dup_new_id = foundry_git_delta_dup_new_id;
  vcs_delta_class->get_old_mode = foundry_git_delta_get_old_mode;
  vcs_delta_class->get_new_mode = foundry_git_delta_get_new_mode;
  vcs_delta_class->get_status = foundry_git_delta_get_status;
  vcs_delta_class->list_hunks = foundry_git_delta_list_hunks;
  vcs_delta_class->serialize = foundry_git_delta_serialize;
  vcs_delta_class->open_content = foundry_git_delta_open_content;
}

static void
foundry_git_delta_init (FoundryGitDelta *self)
{
  self->context_lines = 3;
}

static FoundryVcsDeltaStatus
map_git_delta_status (git_delta_t git_status)
{
  switch (git_status)
    {
    case GIT_DELTA_UNMODIFIED:
      return FOUNDRY_VCS_DELTA_STATUS_UNMODIFIED;
    case GIT_DELTA_ADDED:
      return FOUNDRY_VCS_DELTA_STATUS_ADDED;
    case GIT_DELTA_DELETED:
      return FOUNDRY_VCS_DELTA_STATUS_DELETED;
    case GIT_DELTA_MODIFIED:
      return FOUNDRY_VCS_DELTA_STATUS_MODIFIED;
    case GIT_DELTA_RENAMED:
      return FOUNDRY_VCS_DELTA_STATUS_RENAMED;
    case GIT_DELTA_COPIED:
      return FOUNDRY_VCS_DELTA_STATUS_COPIED;
    case GIT_DELTA_IGNORED:
      return FOUNDRY_VCS_DELTA_STATUS_IGNORED;
    case GIT_DELTA_UNTRACKED:
      return FOUNDRY_VCS_DELTA_STATUS_UNTRACKED;
    case GIT_DELTA_TYPECHANGE:
      return FOUNDRY_VCS_DELTA_STATUS_TYPECHANGE;
    case GIT_DELTA_UNREADABLE:
      return FOUNDRY_VCS_DELTA_STATUS_UNREADABLE;
    case GIT_DELTA_CONFLICTED:
      return FOUNDRY_VCS_DELTA_STATUS_CONFLICTED;
    default:
      return FOUNDRY_VCS_DELTA_STATUS_UNMODIFIED;
    }
}

FoundryGitDelta *
_foundry_git_delta_new (FoundryGitDiff *diff,
                        gsize           delta_idx)
{
  FoundryGitDelta *self;
  const git_diff_delta *delta;

  g_return_val_if_fail (FOUNDRY_IS_GIT_DIFF (diff), NULL);

  delta = _foundry_git_diff_get_delta (diff, delta_idx);
  g_return_val_if_fail (delta != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_DELTA, NULL);
  self->diff = g_object_ref (diff);
  self->delta_idx = delta_idx;
  self->context_lines = _foundry_git_diff_get_context_lines (diff);
  self->old_path = g_strdup (delta->old_file.path);
  self->new_path = g_strdup (delta->new_file.path);
  self->old_oid = delta->old_file.id;
  self->new_oid = delta->new_file.id;
  self->old_mode = delta->old_file.mode;
  self->new_mode = delta->new_file.mode;
  self->status = map_git_delta_status (delta->status);

  return self;
}

void
_foundry_git_delta_set_context_lines (FoundryGitDelta *self,
                                      guint            context_lines)
{
  g_return_if_fail (FOUNDRY_IS_GIT_DELTA (self));

  self->context_lines = context_lines;
}

gboolean
_foundry_git_delta_is_effectively_empty (FoundryGitDelta  *self,
                                         GError          **error)
{
  g_autoptr(FoundryGitPatch) patch = NULL;

  g_return_val_if_fail (FOUNDRY_IS_GIT_DELTA (self), FALSE);

  if (self->status != FOUNDRY_VCS_DELTA_STATUS_MODIFIED)
    return FALSE;

  if (!(patch = foundry_git_delta_create_patch (self, self->context_lines, error)))
    return FALSE;

  return _foundry_git_patch_get_num_hunks (patch) == 0;
}
