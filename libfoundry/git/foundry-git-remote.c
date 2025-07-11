/* foundry-git-remote.c
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

#include "foundry-git-remote-private.h"

struct _FoundryGitRemote
{
  FoundryVcsRemote parent_instance;
  FoundryGitVcs *vcs;
  char *name;
  char *spec;
};

G_DEFINE_FINAL_TYPE (FoundryGitRemote, foundry_git_remote, FOUNDRY_TYPE_VCS_REMOTE)

static char *
foundry_git_remote_dup_name (FoundryVcsRemote *remote)
{
  FoundryGitRemote *self = FOUNDRY_GIT_REMOTE (remote);

  if (self->name == NULL)
    return g_strdup (self->spec);

  return g_strdup (self->name);
}

static void
foundry_git_remote_finalize (GObject *object)
{
  FoundryGitRemote *self = (FoundryGitRemote *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->spec, g_free);
  g_clear_object (&self->vcs);

  G_OBJECT_CLASS (foundry_git_remote_parent_class)->finalize (object);
}

static void
foundry_git_remote_class_init (FoundryGitRemoteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsRemoteClass *remote_class = FOUNDRY_VCS_REMOTE_CLASS (klass);

  object_class->finalize = foundry_git_remote_finalize;

  remote_class->dup_name = foundry_git_remote_dup_name;
}

static void
foundry_git_remote_init (FoundryGitRemote *self)
{
}

FoundryVcsRemote *
foundry_git_remote_new (FoundryGitVcs *vcs,
                            const char    *spec,
                            git_remote    *remote)
{
  FoundryGitRemote *self;

  g_return_val_if_fail (FOUNDRY_IS_GIT_VCS (vcs), NULL);
  g_return_val_if_fail (remote != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_REMOTE, NULL);
  self->vcs = g_object_ref (vcs);
  self->name = g_strdup (git_remote_name (remote));
  self->spec = g_strdup (spec);

  return FOUNDRY_VCS_REMOTE (self);
}

