/* plugin-git-vcs-signature.c
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

#include "plugin-git-vcs-signature.h"
#include "plugin-git-time.h"

struct _PluginGitVcsSignature
{
  FoundryVcsSignature parent_instance;
  git_oid oid;
  git_signature *signature;
};

G_DEFINE_FINAL_TYPE (PluginGitVcsSignature, plugin_git_vcs_signature, FOUNDRY_TYPE_VCS_SIGNATURE)

static char *
plugin_git_vcs_signature_dup_name (FoundryVcsSignature *signature)
{
  PluginGitVcsSignature *self = (PluginGitVcsSignature *)signature;

  g_assert (PLUGIN_IS_GIT_VCS_SIGNATURE (self));

  if (self->signature == NULL)
    return NULL;

  return g_strdup (self->signature->name);
}

static char *
plugin_git_vcs_signature_dup_email (FoundryVcsSignature *signature)
{
  PluginGitVcsSignature *self = (PluginGitVcsSignature *)signature;

  g_assert (PLUGIN_IS_GIT_VCS_SIGNATURE (self));

  if (self->signature == NULL)
    return NULL;

  return g_strdup (self->signature->email);
}

static GDateTime *
plugin_git_vcs_signature_dup_when (FoundryVcsSignature *signature)
{
  PluginGitVcsSignature *self = (PluginGitVcsSignature *)signature;
  g_autoptr(GDateTime) utf = NULL;

  g_assert (PLUGIN_IS_GIT_VCS_SIGNATURE (self));

  if (self->signature == NULL)
    return NULL;

  return plugin_git_time_to_date_time (&self->signature->when);
}

static void
plugin_git_vcs_signature_finalize (GObject *object)
{
  PluginGitVcsSignature *self = (PluginGitVcsSignature *)object;

  g_clear_pointer (&self->signature, git_signature_free);

  G_OBJECT_CLASS (plugin_git_vcs_signature_parent_class)->finalize (object);
}

static void
plugin_git_vcs_signature_class_init (PluginGitVcsSignatureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsSignatureClass *signature_class = FOUNDRY_VCS_SIGNATURE_CLASS (klass);

  object_class->finalize = plugin_git_vcs_signature_finalize;

  signature_class->dup_name = plugin_git_vcs_signature_dup_name;
  signature_class->dup_email = plugin_git_vcs_signature_dup_email;
  signature_class->dup_when = plugin_git_vcs_signature_dup_when;
}

static void
plugin_git_vcs_signature_init (PluginGitVcsSignature *self)
{
}

FoundryVcsSignature *
plugin_git_vcs_signature_new (const git_oid       *oid,
                              const git_signature *signature)
{
  PluginGitVcsSignature *self;
  git_signature *copy;

  g_return_val_if_fail (oid != NULL, NULL);
  g_return_val_if_fail (signature != NULL, NULL);

  if (git_signature_dup (&copy, signature) != 0)
    return NULL;

  self = g_object_new (PLUGIN_TYPE_GIT_VCS_SIGNATURE, NULL);
  self->signature = copy;
  self->oid = *oid;

  return FOUNDRY_VCS_SIGNATURE (self);
}
