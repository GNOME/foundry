/* foundry-git-diff.c
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

#include <git2.h>

#include "foundry-git-autocleanups.h"
#include "foundry-git-delta-private.h"
#include "foundry-git-diff-private.h"
#include "foundry-git-error.h"
#include "foundry-git-stats-private.h"

struct _FoundryGitDiff
{
  FoundryVcsDiff  parent_instance;
  GMutex          mutex;
  git_diff       *diff;
  char           *git_dir;
};

G_DEFINE_FINAL_TYPE (FoundryGitDiff, foundry_git_diff, FOUNDRY_TYPE_VCS_DIFF)

static DexFuture *
foundry_git_diff_list_deltas_thread (gpointer data)
{
  FoundryGitDiff *self = data;
  g_autoptr(GListStore) store = NULL;
  gsize n_deltas;

  g_assert (FOUNDRY_IS_GIT_DIFF (self));

  store = g_list_store_new (FOUNDRY_TYPE_GIT_DELTA);
  n_deltas = _foundry_git_diff_get_num_deltas (self);

  for (gsize i = 0; i < n_deltas; i++)
    {
      g_autoptr(FoundryGitDelta) delta = NULL;

      delta = _foundry_git_delta_new (self, i);
      g_list_store_append (store, delta);
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static DexFuture *
foundry_git_diff_list_deltas (FoundryVcsDiff *diff)
{
  g_assert (FOUNDRY_IS_GIT_DIFF (diff));

  return dex_thread_spawn ("[git-diff-list-deltas]",
                           foundry_git_diff_list_deltas_thread,
                           g_object_ref (diff),
                           g_object_unref);
}

static DexFuture *
foundry_git_diff_load_stats_thread (gpointer data)
{
  FoundryGitDiff *self = data;
  g_autoptr(git_diff_stats) stats = NULL;

  g_assert (FOUNDRY_IS_GIT_DIFF (self));

  if (_foundry_git_diff_get_stats (self, &stats) != 0)
    return foundry_git_reject_last_error ();

  return dex_future_new_take_object (_foundry_git_stats_new (stats));
}

static DexFuture *
foundry_git_diff_load_stats (FoundryVcsDiff *diff)
{
  g_assert (FOUNDRY_IS_GIT_DIFF (diff));

  return dex_thread_spawn ("[git-diff-load-stats]",
                           foundry_git_diff_load_stats_thread,
                           g_object_ref (diff),
                           g_object_unref);
}

static void
foundry_git_diff_finalize (GObject *object)
{
  FoundryGitDiff *self = (FoundryGitDiff *)object;

  g_clear_pointer (&self->diff, git_diff_free);
  g_clear_pointer (&self->git_dir, g_free);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (foundry_git_diff_parent_class)->finalize (object);
}

static void
foundry_git_diff_class_init (FoundryGitDiffClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsDiffClass *vcs_diff_class = FOUNDRY_VCS_DIFF_CLASS (klass);

  object_class->finalize = foundry_git_diff_finalize;

  vcs_diff_class->list_deltas = foundry_git_diff_list_deltas;
  vcs_diff_class->load_stats = foundry_git_diff_load_stats;
}

static void
foundry_git_diff_init (FoundryGitDiff *self)
{
  g_mutex_init (&self->mutex);
}

gsize
_foundry_git_diff_get_num_deltas (FoundryGitDiff *self)
{
  gsize ret;

  g_return_val_if_fail (FOUNDRY_IS_GIT_DIFF (self), 0);

  g_mutex_lock (&self->mutex);
  ret = git_diff_num_deltas (self->diff);
  g_mutex_unlock (&self->mutex);

  return ret;
}

int
_foundry_git_diff_get_stats (FoundryGitDiff *self,
                             git_diff_stats **out)
{
  int ret;

  g_return_val_if_fail (FOUNDRY_IS_GIT_DIFF (self), -1);
  g_return_val_if_fail (out != NULL, -1);

  g_mutex_lock (&self->mutex);
  ret = git_diff_get_stats (out, self->diff);
  g_mutex_unlock (&self->mutex);

  return ret;
}

const git_diff_delta *
_foundry_git_diff_get_delta (FoundryGitDiff *self,
                             gsize           delta_idx)
{
  const git_diff_delta *ret;

  g_return_val_if_fail (FOUNDRY_IS_GIT_DIFF (self), NULL);

  g_mutex_lock (&self->mutex);
  ret = git_diff_get_delta (self->diff, delta_idx);
  g_mutex_unlock (&self->mutex);

  return ret;
}

const char *
_foundry_git_diff_get_git_dir (FoundryGitDiff *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_DIFF (self), NULL);

  return self->git_dir;
}

int
_foundry_git_diff_patch_from_diff (FoundryGitDiff  *self,
                                   git_patch      **out,
                                   gsize            delta_idx)
{
  int ret;

  g_return_val_if_fail (FOUNDRY_IS_GIT_DIFF (self), -1);
  g_return_val_if_fail (out != NULL, -1);

  g_mutex_lock (&self->mutex);
  ret = git_patch_from_diff (out, self->diff, delta_idx);
  g_mutex_unlock (&self->mutex);

  return ret;
}

gboolean
_foundry_git_diff_contains_file (FoundryGitDiff *self,
                                 const char     *relative_path)
{
  g_autoptr(GMutexLocker) locker = NULL;
  gsize n_deltas;
  gboolean ret = FALSE;

  g_return_val_if_fail (FOUNDRY_IS_GIT_DIFF (self), FALSE);
  g_return_val_if_fail (relative_path != NULL, FALSE);

  locker = g_mutex_locker_new (&self->mutex);

  n_deltas = git_diff_num_deltas (self->diff);

  for (gsize i = 0; i < n_deltas; i++)
    {
      const git_diff_delta *delta = git_diff_get_delta (self->diff, i);

      if (delta != NULL &&
          (g_strcmp0 (delta->new_file.path, relative_path) == 0 ||
           g_strcmp0 (delta->old_file.path, relative_path) == 0))
        {
          ret = TRUE;
          break;
        }
    }

  return ret;
}

FoundryGitDiff *
_foundry_git_diff_new (git_diff *diff)
{
  FoundryGitDiff *self;

  g_return_val_if_fail (diff != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_DIFF, NULL);
  self->diff = g_steal_pointer (&diff);

  return self;
}

FoundryGitDiff *
_foundry_git_diff_new_with_dir (git_diff   *diff,
                                const char *git_dir)
{
  FoundryGitDiff *self;

  g_return_val_if_fail (diff != NULL, NULL);

  self = _foundry_git_diff_new (diff);
  self->git_dir = g_strdup (git_dir);

  return self;
}
