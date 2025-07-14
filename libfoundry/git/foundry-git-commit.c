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
#include "foundry-git-signature-private.h"

struct _FoundryGitCommit
{
  FoundryVcsCommit  parent_instance;
  GMutex            mutex;
  git_commit       *commit;
  GDestroyNotify    commit_destroy;
};

G_DEFINE_FINAL_TYPE (FoundryGitCommit, foundry_git_commit, FOUNDRY_TYPE_VCS_COMMIT)

static char *
foundry_git_commit_dup_id (FoundryVcsCommit *commit)
{
  FoundryGitCommit *self = FOUNDRY_GIT_COMMIT (commit);
  g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&self->mutex);
  const git_oid *oid = git_commit_id (self->commit);
  char str[GIT_OID_HEXSZ + 1];

  if (oid == NULL)
    return NULL;

  git_oid_tostr (str, sizeof str, oid);
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

static void
foundry_git_commit_finalize (GObject *object)
{
  FoundryGitCommit *self = (FoundryGitCommit *)object;

  self->commit_destroy (self->commit);
  self->commit_destroy = NULL;
  self->commit = NULL;

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
 *
 * Creates a new [class@Foundry.GitCommit] taking ownership of @commit.
 *
 * Returns: (transfer full):
 */
FoundryGitCommit *
_foundry_git_commit_new (git_commit     *commit,
                         GDestroyNotify  commit_destroy)
{
  FoundryGitCommit *self;

  g_return_val_if_fail (commit != NULL, NULL);

  if (commit_destroy == NULL)
    commit_destroy = (GDestroyNotify)git_commit_free;

  self = g_object_new (FOUNDRY_TYPE_GIT_COMMIT, NULL);
  self->commit = g_steal_pointer (&commit);
  self->commit_destroy = commit_destroy;

  return self;
}
