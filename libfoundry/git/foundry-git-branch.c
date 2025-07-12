/* foundry-git-branch.c
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

#include "foundry-git-branch-private.h"
#include "foundry-git-reference-private.h"
#include "foundry-git-vcs-private.h"

struct _FoundryGitBranch
{
  FoundryVcsBranch parent_instance;
  FoundryGitVcs *vcs;
  char *name;
  git_oid oid;
  git_branch_t branch_type;
  guint oid_set : 1;
};

G_DEFINE_FINAL_TYPE (FoundryGitBranch, foundry_git_branch, FOUNDRY_TYPE_VCS_BRANCH)

static char *
foundry_git_branch_dup_id (FoundryVcsBranch *branch)
{
  FoundryGitBranch *self = (FoundryGitBranch *)branch;
  char oid_str[GIT_OID_HEXSZ + 1];

  g_assert (FOUNDRY_IS_GIT_BRANCH (self));

  if (self->oid_set == FALSE)
    return g_strdup (self->name);

  git_oid_tostr (oid_str, sizeof oid_str, &self->oid);
  oid_str[GIT_OID_HEXSZ] = 0;

  return g_strdup (oid_str);
}

static char *
foundry_git_branch_dup_title (FoundryVcsBranch *object)
{
  FoundryGitBranch *self = (FoundryGitBranch *)object;

  g_assert (FOUNDRY_IS_GIT_BRANCH (self));

  return g_strdup (self->name);
}

static gboolean
foundry_git_branch_is_local (FoundryVcsBranch *branch)
{
  return FOUNDRY_GIT_BRANCH (branch)->branch_type == GIT_BRANCH_LOCAL;
}

static DexFuture *
foundry_git_branch_load_target (FoundryVcsBranch *branch)
{
  FoundryGitBranch *self = (FoundryGitBranch *)branch;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_BRANCH (self));
  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self->vcs));

  if (self->oid_set)
    return dex_future_new_take_object (_foundry_git_reference_new (self->vcs, &self->oid));

  return NULL;
}

static void
foundry_git_branch_finalize (GObject *object)
{
  FoundryGitBranch *self = (FoundryGitBranch *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_object (&self->vcs);

  G_OBJECT_CLASS (foundry_git_branch_parent_class)->finalize (object);
}

static void
foundry_git_branch_class_init (FoundryGitBranchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsBranchClass *vcs_branch_class = FOUNDRY_VCS_BRANCH_CLASS (klass);

  object_class->finalize = foundry_git_branch_finalize;

  vcs_branch_class->dup_id = foundry_git_branch_dup_id;
  vcs_branch_class->dup_title = foundry_git_branch_dup_title;
  vcs_branch_class->load_target = foundry_git_branch_load_target;
  vcs_branch_class->is_local = foundry_git_branch_is_local;
}

static void
foundry_git_branch_init (FoundryGitBranch *self)
{
}

FoundryGitBranch *
_foundry_git_branch_new (FoundryGitVcs *vcs,
                         git_reference *ref,
                         git_branch_t   branch_type)
{
  FoundryGitBranch *self;
  const char *branch_name;
  const git_oid *oid;

  g_return_val_if_fail (ref != NULL, NULL);

  if (git_branch_name (&branch_name, ref) != 0)
    return NULL;

  oid = git_reference_target (ref);

  self = g_object_new (FOUNDRY_TYPE_GIT_BRANCH, NULL);
  self->vcs = g_object_ref (vcs);
  self->name = g_strdup (branch_name);
  self->branch_type = branch_type;

  if (oid != NULL)
    {
      self->oid = *oid;
      self->oid_set = TRUE;
    }

  return self;
}
