/* foundry-git-vcs-branch.c
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

#include "foundry-git-vcs-branch-private.h"

struct _FoundryGitVcsBranch
{
  FoundryVcsBranch parent_instance;
  git_reference *ref;
  git_branch_t branch_type;
};

G_DEFINE_FINAL_TYPE (FoundryGitVcsBranch, foundry_git_vcs_branch, FOUNDRY_TYPE_VCS_BRANCH)

static char *
foundry_git_vcs_branch_dup_id (FoundryVcsObject *object)
{
  FoundryGitVcsBranch *self = (FoundryGitVcsBranch *)object;
  const git_oid *oid;

  g_assert (FOUNDRY_IS_GIT_VCS_BRANCH (self));
  g_assert (self->ref != NULL);

  if ((oid = git_reference_target (self->ref)))
    {
      char oid_str[GIT_OID_HEXSZ + 1];

      git_oid_tostr (oid_str, sizeof oid_str, oid);
      oid_str[GIT_OID_HEXSZ] = 0;

      return g_strdup (oid_str);
    }

  return NULL;
}

static char *
foundry_git_vcs_branch_dup_name (FoundryVcsObject *object)
{
  FoundryGitVcsBranch *self = (FoundryGitVcsBranch *)object;
  const char *branch_name;

  g_assert (FOUNDRY_IS_GIT_VCS_BRANCH (self));
  g_assert (self->ref != NULL);

  if (git_branch_name (&branch_name, self->ref) == 0)
    return g_strdup (branch_name);

  return NULL;
}

static gboolean
foundry_git_vcs_branch_is_local (FoundryVcsObject *object)
{
  FoundryGitVcsBranch *self = (FoundryGitVcsBranch *)object;

  g_assert (FOUNDRY_IS_GIT_VCS_BRANCH (self));
  g_assert (self->ref != NULL);

  return self->branch_type == GIT_BRANCH_LOCAL;
}

static void
foundry_git_vcs_branch_finalize (GObject *object)
{
  FoundryGitVcsBranch *self = (FoundryGitVcsBranch *)object;

  g_clear_pointer (&self->ref, git_reference_free);

  G_OBJECT_CLASS (foundry_git_vcs_branch_parent_class)->finalize (object);
}

static void
foundry_git_vcs_branch_class_init (FoundryGitVcsBranchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsObjectClass *vcs_object_class = FOUNDRY_VCS_OBJECT_CLASS (klass);

  object_class->finalize = foundry_git_vcs_branch_finalize;

  vcs_object_class->dup_id = foundry_git_vcs_branch_dup_id;
  vcs_object_class->dup_name = foundry_git_vcs_branch_dup_name;
  vcs_object_class->is_local = foundry_git_vcs_branch_is_local;
}

static void
foundry_git_vcs_branch_init (FoundryGitVcsBranch *self)
{
}

/**
 * foundry_git_vcs_branch_new:
 * @ref: (transfer full): the underlying reference
 */
FoundryGitVcsBranch *
foundry_git_vcs_branch_new (git_reference *ref,
                           git_branch_t   branch_type)
{
  FoundryGitVcsBranch *self;

  g_return_val_if_fail (ref != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_VCS_BRANCH, NULL);
  self->ref = g_steal_pointer (&ref);
  self->branch_type = branch_type;

  return self;
}
