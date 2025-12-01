/* foundry-git-commit.c
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
#include "foundry-git-commit-private.h"
#include "foundry-git-delta-private.h"
#include "foundry-git-diff-private.h"
#include "foundry-git-error.h"
#include "foundry-git-repository-paths-private.h"
#include "foundry-git-signature-private.h"
#include "foundry-git-tree-private.h"

struct _FoundryGitCommit
{
  FoundryVcsCommit           parent_instance;
  GMutex                     mutex;
  git_commit                *commit;
  GDestroyNotify             commit_destroy;
  git_oid                    oid;
  FoundryGitRepositoryPaths *paths;
};

G_DEFINE_FINAL_TYPE (FoundryGitCommit, foundry_git_commit, FOUNDRY_TYPE_VCS_COMMIT)

static char *
foundry_git_commit_dup_id (FoundryVcsCommit *commit)
{
  FoundryGitCommit *self = FOUNDRY_GIT_COMMIT (commit);
  char str[GIT_OID_HEXSZ + 1];

  git_oid_tostr (str, sizeof str, &self->oid);
  str[GIT_OID_HEXSZ] = 0;

  return g_strdup (str);
}

static char *
foundry_git_commit_dup_title (FoundryVcsCommit *commit)
{
  FoundryGitCommit *self = FOUNDRY_GIT_COMMIT (commit);
  g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&self->mutex);
  const char *message = git_commit_message (self->commit);
  const char *endline;

  if (message == NULL)
    return NULL;

  if ((endline = strchr (message, '\n')))
    return g_utf8_make_valid (message, endline - message);

  return g_utf8_make_valid (message, -1);
}

static FoundryVcsSignature *
foundry_git_commit_dup_author (FoundryVcsCommit *commit)
{
  FoundryGitCommit *self = FOUNDRY_GIT_COMMIT (commit);
  g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&self->mutex);
  const git_signature *signature = git_commit_author (self->commit);
  g_autoptr(git_signature) copy = NULL;

  if (git_signature_dup (&copy, signature) == 0)
    return _foundry_git_signature_new (g_steal_pointer (&copy));

  return NULL;
}

static FoundryVcsSignature *
foundry_git_commit_dup_committer (FoundryVcsCommit *commit)
{
  FoundryGitCommit *self = FOUNDRY_GIT_COMMIT (commit);
  g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&self->mutex);
  const git_signature *signature = git_commit_committer (self->commit);
  g_autoptr(git_signature) copy = NULL;

  if (git_signature_dup (&copy, signature) == 0)
    return _foundry_git_signature_new (g_steal_pointer (&copy));

  return NULL;
}

static guint
foundry_git_commit_get_n_parents (FoundryVcsCommit *commit)
{
  FoundryGitCommit *self = FOUNDRY_GIT_COMMIT (commit);
  g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&self->mutex);

  return git_commit_parentcount (self->commit);
}

typedef struct _LoadParent
{
  FoundryGitCommit *self;
  guint index;
} LoadParent;

static void
load_parent_free (LoadParent *state)
{
  g_clear_object (&state->self);
  g_free (state);
}

static DexFuture *
foundry_git_commit_load_parent_thread (gpointer data)
{
  LoadParent *state = data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_commit) commit = NULL;
  g_autoptr(git_commit) parent = NULL;
  git_oid oid;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_GIT_COMMIT (state->self));

  _foundry_git_commit_get_oid (state->self, &oid);

  if (!foundry_git_repository_paths_open (state->self->paths, &repository, NULL) ||
      git_commit_lookup (&commit, repository, &oid) != 0 ||
      git_commit_parent (&parent, commit, state->index) != 0)
    return foundry_git_reject_last_error ();

  return dex_future_new_take_object (_foundry_git_commit_new (g_steal_pointer (&parent),
                                                              (GDestroyNotify) git_commit_free,
                                                              foundry_git_repository_paths_ref (state->self->paths)));
}

static DexFuture *
foundry_git_commit_load_parent (FoundryVcsCommit *commit,
                                guint             index)
{
  FoundryGitCommit *self = (FoundryGitCommit *)commit;
  LoadParent *state;

  g_assert (FOUNDRY_IS_GIT_COMMIT (self));

  state = g_new0 (LoadParent, 1);
  state->self = g_object_ref (self);
  state->index = index;

  return dex_thread_spawn ("[git-load-parent]",
                           foundry_git_commit_load_parent_thread,
                           state,
                           (GDestroyNotify) load_parent_free);
}

static DexFuture *
foundry_git_commit_load_tree_thread (gpointer data)
{
  FoundryGitCommit *self = data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_commit) commit = NULL;
  g_autoptr(git_tree) tree = NULL;
  git_oid oid;

  g_assert (FOUNDRY_IS_GIT_COMMIT (self));

  _foundry_git_commit_get_oid (self, &oid);

  if (!foundry_git_repository_paths_open (self->paths, &repository, NULL) ||
      git_commit_lookup (&commit, repository, &oid) != 0 ||
      git_commit_tree (&tree, commit) != 0)
    return foundry_git_reject_last_error ();

  return dex_future_new_take_object (_foundry_git_tree_new (g_steal_pointer (&tree)));
}

static DexFuture *
foundry_git_commit_load_tree (FoundryVcsCommit *commit)
{
  g_assert (FOUNDRY_IS_GIT_COMMIT (commit));

  return dex_thread_spawn ("[git-load-tree]",
                           foundry_git_commit_load_tree_thread,
                           g_object_ref (commit),
                           g_object_unref);
}

typedef struct _LoadDelta
{
  FoundryGitCommit *self;
  char *relative_path;
} LoadDelta;

static void
load_delta_free (LoadDelta *state)
{
  g_clear_object (&state->self);
  g_free (state->relative_path);
  g_free (state);
}

static DexFuture *
foundry_git_commit_load_delta_fiber (gpointer data)
{
  LoadDelta *state = data;
  g_autoptr(GError) error = NULL;
  g_autoptr(FoundryVcsTree) commit_tree = NULL;
  g_autoptr(FoundryVcsCommit) parent_commit = NULL;
  g_autoptr(FoundryVcsTree) parent_tree = NULL;
  g_autoptr(FoundryVcsDiff) diff = NULL;
  g_autoptr(GListStore) deltas = NULL;
  FoundryVcsDelta *matching_delta = NULL;
  guint n_deltas;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_GIT_COMMIT (state->self));
  g_assert (state->relative_path != NULL);

  if (!(commit_tree = dex_await_object (foundry_vcs_commit_load_tree (FOUNDRY_VCS_COMMIT (state->self)), &error)) ||
      !(parent_commit = dex_await_object (foundry_vcs_commit_load_parent (FOUNDRY_VCS_COMMIT (state->self), 0), &error)) ||
      !(parent_tree = dex_await_object (foundry_vcs_commit_load_tree (parent_commit), &error)) ||
      !(diff = dex_await_object (_foundry_git_tree_diff (FOUNDRY_GIT_TREE (parent_tree),
                                                         FOUNDRY_GIT_TREE (commit_tree),
                                                         state->self->paths), &error)) ||
      !(deltas = dex_await_object (foundry_vcs_diff_list_deltas (diff), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  n_deltas = g_list_model_get_n_items (G_LIST_MODEL (deltas));

  for (guint i = 0; i < n_deltas; i++)
    {
      g_autoptr(FoundryVcsDelta) delta = NULL;
      g_autofree char *old_path = NULL;
      g_autofree char *new_path = NULL;

      delta = g_list_model_get_item (G_LIST_MODEL (deltas), i);
      old_path = foundry_vcs_delta_dup_old_path (delta);
      new_path = foundry_vcs_delta_dup_new_path (delta);

      if ((old_path != NULL && g_strcmp0 (old_path, state->relative_path) == 0) ||
          (new_path != NULL && g_strcmp0 (new_path, state->relative_path) == 0))
        {
          matching_delta = g_steal_pointer (&delta);
          break;
        }
    }

  if (matching_delta == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                   G_IO_ERROR_NOT_FOUND,
                                   "Delta not found for file");

  return dex_future_new_take_object (matching_delta);
}

static DexFuture *
foundry_git_commit_load_delta (FoundryVcsCommit *commit,
                               const char       *relative_path)
{
  FoundryGitCommit *self = (FoundryGitCommit *)commit;
  LoadDelta *state;

  g_assert (FOUNDRY_IS_GIT_COMMIT (self));
  g_assert (relative_path != NULL);

  state = g_new0 (LoadDelta, 1);
  state->self = g_object_ref (self);
  state->relative_path = g_strdup (relative_path);

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                               foundry_git_commit_load_delta_fiber,
                               state,
                               (GDestroyNotify) load_delta_free);
}

static void
foundry_git_commit_finalize (GObject *object)
{
  FoundryGitCommit *self = (FoundryGitCommit *)object;

  self->commit_destroy (self->commit);
  self->commit_destroy = NULL;
  self->commit = NULL;

  g_clear_pointer (&self->paths, foundry_git_repository_paths_unref);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (foundry_git_commit_parent_class)->finalize (object);
}

static void
foundry_git_commit_class_init (FoundryGitCommitClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsCommitClass *commit_class = FOUNDRY_VCS_COMMIT_CLASS (klass);

  object_class->finalize = foundry_git_commit_finalize;

  commit_class->dup_id =foundry_git_commit_dup_id;
  commit_class->dup_title =foundry_git_commit_dup_title;
  commit_class->dup_author = foundry_git_commit_dup_author;
  commit_class->dup_committer = foundry_git_commit_dup_committer;
  commit_class->get_n_parents = foundry_git_commit_get_n_parents;
  commit_class->load_parent = foundry_git_commit_load_parent;
  commit_class->load_tree = foundry_git_commit_load_tree;
  commit_class->load_delta = foundry_git_commit_load_delta;
}

static void
foundry_git_commit_init (FoundryGitCommit *self)
{
  g_mutex_init (&self->mutex);
}

/**
 * _foundry_git_commit_new:
 * @commit: (transfer full): the git_commit to wrap
 * @commit_destroy: destroy callback for @commit
 * @paths: (transfer full): the repository paths
 *
 * Creates a new [class@Foundry.GitCommit] taking ownership of @commit.
 *
 * Returns: (transfer full):
 */
FoundryGitCommit *
_foundry_git_commit_new (git_commit                *commit,
                         GDestroyNotify             commit_destroy,
                         FoundryGitRepositoryPaths *paths)
{
  FoundryGitCommit *self;

  g_return_val_if_fail (commit != NULL, NULL);
  g_return_val_if_fail (paths != NULL, NULL);

  if (commit_destroy == NULL)
    commit_destroy = (GDestroyNotify)git_commit_free;

  self = g_object_new (FOUNDRY_TYPE_GIT_COMMIT, NULL);
  self->oid = *git_commit_id (commit);
  self->commit = g_steal_pointer (&commit);
  self->commit_destroy = commit_destroy;
  self->paths = paths;

  return self;
}

void
_foundry_git_commit_get_oid (FoundryGitCommit *self,
                             git_oid          *oid)
{
  g_return_if_fail (FOUNDRY_IS_GIT_COMMIT (self));

  *oid = self->oid;
}

gboolean
_foundry_git_commit_get_tree_id (FoundryGitCommit *self,
                                 git_oid          *tree_id)
{
  g_autoptr(GMutexLocker) locker = NULL;
  const git_oid *oid;

  g_return_val_if_fail (FOUNDRY_IS_GIT_COMMIT (self), FALSE);
  g_return_val_if_fail (tree_id != NULL, FALSE);

  locker = g_mutex_locker_new (&self->mutex);

  oid = git_commit_tree_id (self->commit);

  if (oid != NULL)
    {
      *tree_id = *oid;
      return TRUE;
    }

  return FALSE;
}
