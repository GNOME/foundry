/* foundry-git-vcs-signature.c
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

#include "foundry-git-vcs-signature-private.h"
#include "foundry-git-time.h"

struct _FoundryGitVcsSignature
{
  FoundryVcsSignature parent_instance;
  git_oid oid;
  git_signature *signature;
};

G_DEFINE_FINAL_TYPE (FoundryGitVcsSignature, foundry_git_vcs_signature, FOUNDRY_TYPE_VCS_SIGNATURE)

static char *
foundry_git_vcs_signature_dup_name (FoundryVcsSignature *signature)
{
  FoundryGitVcsSignature *self = (FoundryGitVcsSignature *)signature;

  g_assert (FOUNDRY_IS_GIT_VCS_SIGNATURE (self));

  if (self->signature == NULL || self->signature->name == NULL)
    return NULL;

  return g_utf8_make_valid (self->signature->name, -1);
}

static char *
foundry_git_vcs_signature_dup_email (FoundryVcsSignature *signature)
{
  FoundryGitVcsSignature *self = (FoundryGitVcsSignature *)signature;

  g_assert (FOUNDRY_IS_GIT_VCS_SIGNATURE (self));

  if (self->signature == NULL || self->signature->email == NULL)
    return NULL;

  return g_utf8_make_valid (self->signature->email, -1);
}

static GDateTime *
foundry_git_vcs_signature_dup_when (FoundryVcsSignature *signature)
{
  FoundryGitVcsSignature *self = (FoundryGitVcsSignature *)signature;
  g_autoptr(GDateTime) utf = NULL;

  g_assert (FOUNDRY_IS_GIT_VCS_SIGNATURE (self));

  if (self->signature == NULL)
    return NULL;

  return foundry_git_time_to_date_time (&self->signature->when);
}

static void
foundry_git_vcs_signature_finalize (GObject *object)
{
  FoundryGitVcsSignature *self = (FoundryGitVcsSignature *)object;

  g_clear_pointer (&self->signature, git_signature_free);

  G_OBJECT_CLASS (foundry_git_vcs_signature_parent_class)->finalize (object);
}

static void
foundry_git_vcs_signature_class_init (FoundryGitVcsSignatureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsSignatureClass *signature_class = FOUNDRY_VCS_SIGNATURE_CLASS (klass);

  object_class->finalize = foundry_git_vcs_signature_finalize;

  signature_class->dup_name = foundry_git_vcs_signature_dup_name;
  signature_class->dup_email = foundry_git_vcs_signature_dup_email;
  signature_class->dup_when = foundry_git_vcs_signature_dup_when;
}

static void
foundry_git_vcs_signature_init (FoundryGitVcsSignature *self)
{
}

FoundryVcsSignature *
foundry_git_vcs_signature_new (const git_oid       *oid,
                              const git_signature *signature)
{
  FoundryGitVcsSignature *self;
  git_signature *copy;

  g_return_val_if_fail (oid != NULL, NULL);
  g_return_val_if_fail (signature != NULL, NULL);

  if (git_signature_dup (&copy, signature) != 0)
    return NULL;

  self = g_object_new (FOUNDRY_TYPE_GIT_VCS_SIGNATURE, NULL);
  self->signature = copy;
  self->oid = *oid;

  return FOUNDRY_VCS_SIGNATURE (self);
}
