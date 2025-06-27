/* foundry-git-vcs-remote.c
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

#include "foundry-git-vcs-remote-private.h"

struct _FoundryGitVcsRemote
{
  FoundryVcsRemote parent_instance;
  FoundryGitVcs *vcs;
  git_remote *remote;
  char *spec;
};

G_DEFINE_FINAL_TYPE (FoundryGitVcsRemote, foundry_git_vcs_remote, FOUNDRY_TYPE_VCS_REMOTE)

static char *
foundry_git_vcs_remote_dup_name (FoundryVcsRemote *remote)
{
  FoundryGitVcsRemote *self = FOUNDRY_GIT_VCS_REMOTE (remote);
  const char *ret = git_remote_name (self->remote);

  if (ret == NULL)
    ret = self->spec;

  return g_strdup (ret);
}

static void
foundry_git_vcs_remote_finalize (GObject *object)
{
  FoundryGitVcsRemote *self = (FoundryGitVcsRemote *)object;

  g_clear_pointer (&self->remote, git_remote_free);
  g_clear_pointer (&self->spec, g_free);
  g_clear_object (&self->vcs);

  G_OBJECT_CLASS (foundry_git_vcs_remote_parent_class)->finalize (object);
}

static void
foundry_git_vcs_remote_class_init (FoundryGitVcsRemoteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsRemoteClass *remote_class = FOUNDRY_VCS_REMOTE_CLASS (klass);

  object_class->finalize = foundry_git_vcs_remote_finalize;

  remote_class->dup_name = foundry_git_vcs_remote_dup_name;
}

static void
foundry_git_vcs_remote_init (FoundryGitVcsRemote *self)
{
}

FoundryVcsRemote *
foundry_git_vcs_remote_new (FoundryGitVcs *vcs,
                           const char   *spec,
                           git_remote   *remote)
{
  FoundryGitVcsRemote *self;

  self = g_object_new (FOUNDRY_TYPE_GIT_VCS_REMOTE, NULL);
  self->vcs = g_object_ref (vcs);
  self->remote = remote;
  self->spec = g_strdup (spec);

  return FOUNDRY_VCS_REMOTE (self);
}
