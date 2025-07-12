/* foundry-git-reference.c
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

#include "foundry-git-reference-private.h"

struct _FoundryGitReference
{
  FoundryVcsReference parent_instance;
  FoundryGitVcs *vcs;
  char *name;
  git_oid oid;
  guint oid_set : 1;
};

G_DEFINE_FINAL_TYPE (FoundryGitReference, foundry_git_reference, FOUNDRY_TYPE_VCS_REFERENCE)

static char *
foundry_git_reference_dup_id (FoundryVcsReference *reference)
{
  FoundryGitReference *self = (FoundryGitReference *)reference;
  char oid_str[GIT_OID_HEXSZ + 1];

  g_assert (FOUNDRY_IS_GIT_REFERENCE (self));

  if (self->oid_set)
    {
      git_oid_tostr (oid_str, sizeof oid_str, &self->oid);
      oid_str[GIT_OID_HEXSZ] = 0;

      return g_strdup (oid_str);
    }

  return g_strdup (self->name);
}

static gboolean
foundry_git_reference_is_symbolic (FoundryVcsReference *reference)
{
  FoundryGitReference *self = (FoundryGitReference *)reference;

  g_assert (FOUNDRY_IS_GIT_REFERENCE (self));

  return !self->oid_set;
}

static DexFuture *
foundry_git_reference_resolve (FoundryVcsReference *reference)
{
  FoundryGitReference *self = (FoundryGitReference *)reference;

  g_assert (FOUNDRY_IS_GIT_REFERENCE (self));

  if (self->oid_set)
    return dex_future_new_take_object (g_object_ref (self));

  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self->vcs));
  dex_return_error_if_fail (self->name != NULL);

  return _foundry_git_vcs_resolve_name (self->vcs, self->name);
}

static void
foundry_git_reference_finalize (GObject *object)
{
  FoundryGitReference *self = (FoundryGitReference *)object;

  g_clear_object (&self->vcs);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (foundry_git_reference_parent_class)->finalize (object);
}

static void
foundry_git_reference_class_init (FoundryGitReferenceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsReferenceClass *ref_class = FOUNDRY_VCS_REFERENCE_CLASS (klass);

  object_class->finalize = foundry_git_reference_finalize;

  ref_class->dup_id = foundry_git_reference_dup_id;
  ref_class->is_symbolic = foundry_git_reference_is_symbolic;
  ref_class->resolve = foundry_git_reference_resolve;
}

static void
foundry_git_reference_init (FoundryGitReference *self)
{
}

FoundryGitReference *
_foundry_git_reference_new (FoundryGitVcs *vcs,
                            const git_oid *oid)
{
  FoundryGitReference *self;

  g_return_val_if_fail (FOUNDRY_IS_GIT_VCS (vcs), NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_VCS, NULL);
  self->vcs = g_object_ref (vcs);
  self->oid = *oid;
  self->oid_set = TRUE;

  return self;
}

FoundryGitReference *
_foundry_git_reference_new_symbolic (FoundryGitVcs *vcs,
                                     const char    *name)
{
  FoundryGitReference *self;

  g_return_val_if_fail (FOUNDRY_IS_GIT_VCS (vcs), NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_VCS, NULL);
  self->vcs = g_object_ref (vcs);
  self->name = g_strdup (name);

  return self;
}
