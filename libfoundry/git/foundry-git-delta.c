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
#include "foundry-util.h"

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
  char str[GIT_OID_HEXSZ + 1];

  git_oid_tostr (str, sizeof str, &self->old_oid);
  str[GIT_OID_HEXSZ] = 0;

  return g_strdup (str);
}

static char *
foundry_git_delta_dup_new_id (FoundryVcsDelta *delta)
{
  FoundryGitDelta *self = FOUNDRY_GIT_DELTA (delta);
  char str[GIT_OID_HEXSZ + 1];

  git_oid_tostr (str, sizeof str, &self->new_oid);
  str[GIT_OID_HEXSZ] = 0;

  return g_strdup (str);
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

static DexFuture *
foundry_git_delta_list_hunks_thread (gpointer data)
{
  FoundryGitDelta *self = data;
  g_autoptr(FoundryGitPatch) git_patch = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(git_patch) patch = NULL;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_blob) old_blob = NULL;
  g_autoptr(git_blob) new_blob = NULL;
  g_autoptr(GBytes) contents = NULL;
  const git_diff_delta *delta = NULL;
  gsize num_hunks;
  const char *old_path = NULL;
  const char *new_path = NULL;
  g_autofree char *git_dir = NULL;
  int ret;

  g_assert (FOUNDRY_IS_GIT_DELTA (self));
  g_assert (FOUNDRY_IS_GIT_DIFF (self->diff));

  store = g_list_store_new (FOUNDRY_TYPE_GIT_DIFF_HUNK);

  delta = _foundry_git_diff_get_delta (self->diff, self->delta_idx);
  if (delta == NULL)
    return foundry_git_reject_last_error ();

  old_path = delta->old_file.path;
  new_path = delta->new_file.path;

  git_dir = g_strdup (_foundry_git_diff_get_git_dir (self->diff));
  if (git_dir != NULL && git_repository_open (&repository, git_dir) == 0)
    {
      g_autofree char *file_path = NULL;
      const char *workdir;
      const char *buf = NULL;
      gsize buf_len = 0;

      /* Try to create patch from blobs directly first, as git_patch_from_diff
       * can fail with OID type mismatches when the diff was created with
       * a NULL tree or when OIDs are zero */
      git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;

      if (!git_oid_is_zero (&delta->old_file.id))
        git_blob_lookup (&old_blob, repository, &delta->old_file.id);

      if (!git_oid_is_zero (&delta->new_file.id))
        git_blob_lookup (&new_blob, repository, &delta->new_file.id);

      diff_opts.context_lines = self->context_lines;

      /* Read from working directory as fallback for unstaged changes */
      /* Prefer blobs from index/tree for staged changes */
      workdir = git_repository_workdir (repository);

      if (workdir != NULL && new_path != NULL)
        {
          g_autoptr(GError) error = NULL;
          g_autoptr(GFile) file = NULL;

          file_path = g_build_filename (workdir, new_path, NULL);
          file = g_file_new_for_path (file_path);

          if ((contents = g_file_load_bytes (file, NULL, NULL, &error)))
            buf = g_bytes_get_data (contents, &buf_len);
        }

      if (old_blob != NULL && new_blob != NULL)
        {
          /* Both blobs available - compare them */
          ret = git_patch_from_blobs (&patch, old_blob, old_path, new_blob, new_path, &diff_opts);
        }
      else if (old_blob != NULL && buf != NULL)
        {
          /* Compare old blob to working directory file */
          ret = git_patch_from_blob_and_buffer (&patch, old_blob, old_path, buf, buf_len, new_path, &diff_opts);
        }
      else if (old_blob != NULL)
        {
          /* Old blob but no working directory file - file was deleted */
          ret = git_patch_from_blob_and_buffer (&patch, old_blob, old_path, NULL, 0, new_path, &diff_opts);
        }
      else if (new_blob != NULL)
        {
          /* New blob available - prefer blob over workdir for staged changes */
          /* Copy blob contents into GBytes to keep them alive after repository/blob are released */
          if (contents == NULL)
            {
              const char *blob_content = git_blob_rawcontent (new_blob);
              gsize blob_size = git_blob_rawsize (new_blob);
              contents = g_bytes_new (blob_content, blob_size);
            }
          ret = git_patch_from_blob_and_buffer (&patch, NULL, old_path, g_bytes_get_data (contents, NULL), g_bytes_get_size (contents), new_path, &diff_opts);
        }
      else if (buf != NULL)
        {
          /* New file - compare NULL to working directory file */
          ret = git_patch_from_blob_and_buffer (&patch, NULL, old_path, buf, buf_len, new_path, &diff_opts);
        }
      else
        {
          /* Both are zero and no working directory file - empty patch */
          ret = git_patch_from_blob_and_buffer (&patch, NULL, old_path, NULL, 0, new_path, &diff_opts);
        }

      if (ret != 0)
        {
          /* Fallback to git_patch_from_diff if blob-based creation fails */
          ret = _foundry_git_diff_patch_from_diff (self->diff, &patch, self->delta_idx);
        }
    }
  else
    {
      /* No git_dir, try git_patch_from_diff directly */
      ret = _foundry_git_diff_patch_from_diff (self->diff, &patch, self->delta_idx);
    }

  if (ret != 0)
    return foundry_git_reject_last_error ();

  git_patch = _foundry_git_patch_new_with_bytes (g_steal_pointer (&patch), g_steal_pointer (&contents));
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

  return dex_thread_spawn ("[git-delta-list-hunks]",
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
  g_autoptr(git_patch) patch = NULL;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_blob) old_blob = NULL;
  g_autoptr(git_blob) new_blob = NULL;
  g_autoptr(GBytes) contents = NULL;
  const git_diff_delta *delta = NULL;
  gsize num_hunks;
  const char *old_path = NULL;
  const char *new_path = NULL;
  g_autofree char *old_path_dup = NULL;
  g_autofree char *new_path_dup = NULL;
  FoundryVcsDeltaStatus status;
  g_autofree char *git_dir = NULL;
  int ret;

  g_assert (FOUNDRY_IS_GIT_DELTA (self));
  g_assert (FOUNDRY_IS_GIT_DIFF (self->diff));

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

  /* Create patch with specified context_lines */
  delta = _foundry_git_diff_get_delta (self->diff, self->delta_idx);
  if (delta == NULL)
    return foundry_git_reject_last_error ();

  old_path = delta->old_file.path;
  new_path = delta->new_file.path;

  git_dir = g_strdup (_foundry_git_diff_get_git_dir (self->diff));
  if (git_dir != NULL && git_repository_open (&repository, git_dir) == 0)
    {
      g_autofree char *file_path = NULL;
      const char *workdir;
      const char *buf = NULL;
      gsize buf_len = 0;

      /* Try to create patch from blobs directly first, as git_patch_from_diff
       * can fail with OID type mismatches when the diff was created with
       * a NULL tree or when OIDs are zero */
      git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;

      if (!git_oid_is_zero (&delta->old_file.id))
        git_blob_lookup (&old_blob, repository, &delta->old_file.id);

      if (!git_oid_is_zero (&delta->new_file.id))
        git_blob_lookup (&new_blob, repository, &delta->new_file.id);

      diff_opts.context_lines = context_lines;

      /* Read from working directory as fallback for unstaged changes */
      /* Prefer blobs from index/tree for staged changes */
      workdir = git_repository_workdir (repository);

      if (workdir != NULL && new_path != NULL)
        {
          g_autoptr(GError) error = NULL;
          g_autoptr(GFile) file = NULL;

          file_path = g_build_filename (workdir, new_path, NULL);
          file = g_file_new_for_path (file_path);

          if ((contents = g_file_load_bytes (file, NULL, NULL, &error)))
            buf = g_bytes_get_data (contents, &buf_len);
        }

      if (old_blob != NULL && new_blob != NULL)
        {
          /* Both blobs available - compare them */
          ret = git_patch_from_blobs (&patch, old_blob, old_path, new_blob, new_path, &diff_opts);
        }
      else if (old_blob != NULL && buf != NULL)
        {
          /* Compare old blob to working directory file */
          ret = git_patch_from_blob_and_buffer (&patch, old_blob, old_path, buf, buf_len, new_path, &diff_opts);
        }
      else if (old_blob != NULL)
        {
          /* Old blob but no working directory file - file was deleted */
          ret = git_patch_from_blob_and_buffer (&patch, old_blob, old_path, NULL, 0, new_path, &diff_opts);
        }
      else if (new_blob != NULL)
        {
          /* New blob available - prefer blob over workdir for staged changes */
          /* Copy blob contents into GBytes to keep them alive after repository/blob are released */
          if (contents == NULL)
            {
              const char *blob_content = git_blob_rawcontent (new_blob);
              gsize blob_size = git_blob_rawsize (new_blob);
              contents = g_bytes_new (blob_content, blob_size);
            }
          ret = git_patch_from_blob_and_buffer (&patch, NULL, old_path, g_bytes_get_data (contents, NULL), g_bytes_get_size (contents), new_path, &diff_opts);
        }
      else if (buf != NULL)
        {
          /* New file - compare NULL to working directory file */
          ret = git_patch_from_blob_and_buffer (&patch, NULL, old_path, buf, buf_len, new_path, &diff_opts);
        }
      else
        {
          /* Both are zero and no working directory file - empty patch */
          ret = git_patch_from_blob_and_buffer (&patch, NULL, old_path, NULL, 0, new_path, &diff_opts);
        }

      if (ret != 0)
        {
          /* Fallback to git_patch_from_diff if blob-based creation fails */
          ret = _foundry_git_diff_patch_from_diff (self->diff, &patch, self->delta_idx);
        }
    }
  else
    {
      /* No git_dir, try git_patch_from_diff directly */
      ret = _foundry_git_diff_patch_from_diff (self->diff, &patch, self->delta_idx);
    }

  if (ret != 0)
    return foundry_git_reject_last_error ();

  git_patch = _foundry_git_patch_new_with_bytes (g_steal_pointer (&patch), g_steal_pointer (&contents));
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

  return dex_thread_spawn ("[git-delta-serialize]",
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
