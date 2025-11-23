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
  gsize num_hunks;

  g_assert (FOUNDRY_IS_GIT_DELTA (self));
  g_assert (FOUNDRY_IS_GIT_DIFF (self->diff));

  store = g_list_store_new (FOUNDRY_TYPE_GIT_DIFF_HUNK);

  if (_foundry_git_diff_patch_from_diff (self->diff, &patch, self->delta_idx) != 0)
    return foundry_git_reject_last_error ();

  git_patch = _foundry_git_patch_new (g_steal_pointer (&patch));
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
}

static void
foundry_git_delta_init (FoundryGitDelta *self)
{
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
