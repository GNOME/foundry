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

struct _FoundryGitBranch
{
  FoundryVcsBranch parent_instance;
  char *name;
  git_oid oid;
  git_branch_t branch_type;
  guint oid_set : 1;
};

G_DEFINE_FINAL_TYPE (FoundryGitBranch, foundry_git_branch, FOUNDRY_TYPE_VCS_BRANCH)

static char *
foundry_git_branch_dup_id (FoundryVcsObject *object)
{
  FoundryGitBranch *self = (FoundryGitBranch *)object;
  char oid_str[GIT_OID_HEXSZ + 1];

  g_assert (FOUNDRY_IS_GIT_BRANCH (self));

  if (self->oid_set == FALSE)
    return NULL;

  git_oid_tostr (oid_str, sizeof oid_str, &self->oid);
  oid_str[GIT_OID_HEXSZ] = 0;

  return g_strdup (oid_str);
}

static char *
foundry_git_branch_dup_name (FoundryVcsObject *object)
{
  FoundryGitBranch *self = (FoundryGitBranch *)object;

  g_assert (FOUNDRY_IS_GIT_BRANCH (self));

  return g_strdup (self->name);
}

static gboolean
foundry_git_branch_is_local (FoundryVcsObject *object)
{
  FoundryGitBranch *self = (FoundryGitBranch *)object;

  g_assert (FOUNDRY_IS_GIT_BRANCH (self));

  return self->branch_type == GIT_BRANCH_LOCAL;
}

static void
foundry_git_branch_finalize (GObject *object)
{
  FoundryGitBranch *self = (FoundryGitBranch *)object;

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (foundry_git_branch_parent_class)->finalize (object);
}

static void
foundry_git_branch_class_init (FoundryGitBranchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsObjectClass *vcs_object_class = FOUNDRY_VCS_OBJECT_CLASS (klass);

  object_class->finalize = foundry_git_branch_finalize;

  vcs_object_class->dup_id = foundry_git_branch_dup_id;
  vcs_object_class->dup_name = foundry_git_branch_dup_name;
  vcs_object_class->is_local = foundry_git_branch_is_local;
}

static void
foundry_git_branch_init (FoundryGitBranch *self)
{
}

/**
 * foundry_git_branch_new:
 */
FoundryGitBranch *
foundry_git_branch_new (git_reference *ref,
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
  self->name = g_strdup (branch_name);
  self->branch_type = branch_type;

  if (oid != NULL)
    {
      self->oid = *oid;
      self->oid_set = TRUE;
    }

  return self;
}

