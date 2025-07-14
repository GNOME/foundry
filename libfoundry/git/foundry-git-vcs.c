/* foundry-git-vcs.c
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

#include <glib/gi18n-lib.h>

#include "foundry-auth-prompt.h"
#include "foundry-git-autocleanups.h"
#include "foundry-git-file-list-private.h"
#include "foundry-git-error.h"
#include "foundry-git-blame-private.h"
#include "foundry-git-branch-private.h"
#include "foundry-git-file-private.h"
#include "foundry-git-reference-private.h"
#include "foundry-git-repository-private.h"
#include "foundry-git-remote-private.h"
#include "foundry-git-tag-private.h"
#include "foundry-git-vcs-private.h"
#include "foundry-operation.h"
#include "foundry-util.h"

struct _FoundryGitVcs
{
  FoundryVcs            parent_instance;
  FoundryGitRepository *repository;
  GFile                *workdir;
};

G_DEFINE_FINAL_TYPE (FoundryGitVcs, foundry_git_vcs, FOUNDRY_TYPE_VCS)

static char *
foundry_git_vcs_dup_id (FoundryVcs *vcs)
{
  return g_strdup ("git");
}

static char *
foundry_git_vcs_dup_name (FoundryVcs *vcs)
{
  return g_strdup (_("Git"));
}

static char *
foundry_git_vcs_dup_branch_name (FoundryVcs *vcs)
{
  return _foundry_git_repository_dup_branch_name (FOUNDRY_GIT_VCS (vcs)->repository);
}

static guint
foundry_git_vcs_get_priority (FoundryVcs *vcs)
{
  return 100;
}

static gboolean
foundry_git_vcs_is_ignored (FoundryVcs *vcs,
                            const char *relative_path)
{
  FoundryGitVcs *self = FOUNDRY_GIT_VCS (vcs);

  return _foundry_git_repository_is_ignored (self->repository, relative_path);
}

static gboolean
foundry_git_vcs_is_file_ignored (FoundryVcs *vcs,
                                 GFile      *file)
{
  FoundryGitVcs *self = FOUNDRY_GIT_VCS (vcs);
  g_autofree char *relative_path = NULL;

  if (!g_file_has_prefix (file, self->workdir))
    return FALSE;

  relative_path = g_file_get_relative_path (self->workdir, file);

  return _foundry_git_repository_is_ignored (self->repository, relative_path);
}

static DexFuture *
foundry_git_vcs_list_files (FoundryVcs *vcs)
{
  return _foundry_git_repository_list_files (FOUNDRY_GIT_VCS (vcs)->repository);
}

static DexFuture *
foundry_git_vcs_list_branches (FoundryVcs *vcs)
{
  return _foundry_git_repository_list_branches (FOUNDRY_GIT_VCS (vcs)->repository);
}

static DexFuture *
foundry_git_vcs_list_tags (FoundryVcs *vcs)
{
  return _foundry_git_repository_list_tags (FOUNDRY_GIT_VCS (vcs)->repository);
}

static DexFuture *
foundry_git_vcs_list_remotes (FoundryVcs *vcs)
{
  return _foundry_git_repository_list_remotes (FOUNDRY_GIT_VCS (vcs)->repository);
}

static DexFuture *
foundry_git_vcs_find_file (FoundryVcs *vcs,
                           GFile      *file)
{
  return _foundry_git_repository_find_file (FOUNDRY_GIT_VCS (vcs)->repository, file);
}

static DexFuture *
foundry_git_vcs_find_remote (FoundryVcs *vcs,
                             const char *name)
{
  return _foundry_git_repository_find_remote (FOUNDRY_GIT_VCS (vcs)->repository, name);
}

static DexFuture *
foundry_git_vcs_blame (FoundryVcs     *vcs,
                       FoundryVcsFile *file,
                       GBytes         *bytes)
{
  FoundryGitVcs *self = FOUNDRY_GIT_VCS (vcs);
  g_autofree char *relative_path = foundry_vcs_file_dup_relative_path (file);

  return _foundry_git_repository_blame (self->repository, relative_path, bytes);
}

static DexFuture *
foundry_git_vcs_fetch (FoundryVcs       *vcs,
                       FoundryVcsRemote *remote,
                       FoundryOperation *operation)
{
  g_autoptr(FoundryContext) context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (vcs));

  return _foundry_git_repository_fetch (FOUNDRY_GIT_VCS (vcs)->repository, context, remote, operation);
}

static void
foundry_git_vcs_finalize (GObject *object)
{
  FoundryGitVcs *self = (FoundryGitVcs *)object;

  g_clear_object (&self->repository);
  g_clear_object (&self->workdir);

  G_OBJECT_CLASS (foundry_git_vcs_parent_class)->finalize (object);
}

static void
foundry_git_vcs_class_init (FoundryGitVcsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsClass *vcs_class = FOUNDRY_VCS_CLASS (klass);

  object_class->finalize = foundry_git_vcs_finalize;

  vcs_class->blame = foundry_git_vcs_blame;
  vcs_class->dup_branch_name = foundry_git_vcs_dup_branch_name;
  vcs_class->dup_id = foundry_git_vcs_dup_id;
  vcs_class->dup_name = foundry_git_vcs_dup_name;
  vcs_class->fetch = foundry_git_vcs_fetch;
  vcs_class->find_file = foundry_git_vcs_find_file;
  vcs_class->find_remote = foundry_git_vcs_find_remote;
  vcs_class->get_priority = foundry_git_vcs_get_priority;
  vcs_class->is_file_ignored = foundry_git_vcs_is_file_ignored;
  vcs_class->is_ignored = foundry_git_vcs_is_ignored;
  vcs_class->list_branches = foundry_git_vcs_list_branches;
  vcs_class->list_files = foundry_git_vcs_list_files;
  vcs_class->list_remotes = foundry_git_vcs_list_remotes;
  vcs_class->list_tags = foundry_git_vcs_list_tags;
}

static void
foundry_git_vcs_init (FoundryGitVcs *self)
{
}

DexFuture *
_foundry_git_vcs_new (FoundryContext *context,
                      git_repository *repository)
{
  FoundryGitVcs *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (repository != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_VCS,
                       "context", context,
                       NULL);

  self->workdir = g_file_new_for_path (git_repository_workdir (repository));
  self->repository = _foundry_git_repository_new (g_steal_pointer (&repository));

  return dex_future_new_take_object (g_steal_pointer (&self));
}
