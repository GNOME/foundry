/* foundry-git-repository-private.h
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

#pragma once

#include <git2.h>
#include <glib-object.h>
#include <libdex.h>

#include "foundry-context.h"
#include "foundry-operation.h"
#include "foundry-vcs-remote.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_GIT_REPOSITORY (foundry_git_repository_get_type())

G_DECLARE_FINAL_TYPE (FoundryGitRepository, foundry_git_repository, FOUNDRY, GIT_REPOSITORY, GObject)

FoundryGitRepository *_foundry_git_repository_new             (git_repository       *repository);
char                 *_foundry_git_repository_dup_branch_name (FoundryGitRepository *self);
DexFuture            *_foundry_git_repository_list_branches   (FoundryGitRepository *self);
DexFuture            *_foundry_git_repository_list_tags       (FoundryGitRepository *self);
DexFuture            *_foundry_git_repository_list_remotes    (FoundryGitRepository *self);
DexFuture            *_foundry_git_repository_list_files      (FoundryGitRepository *self);
DexFuture            *_foundry_git_repository_find_file       (FoundryGitRepository *self,
                                                               GFile                *file);
gboolean              _foundry_git_repository_is_ignored      (FoundryGitRepository *self,
                                                               const char           *relative_path);
DexFuture            *_foundry_git_repository_blame           (FoundryGitRepository *self,
                                                               const char           *relative_path,
                                                               GBytes               *bytes);
DexFuture            *_foundry_git_repository_find_remote     (FoundryGitRepository *self,
                                                               const char           *name);
DexFuture            *_foundry_git_repository_fetch           (FoundryGitRepository *self,
                                                               FoundryContext       *context,
                                                               FoundryVcsRemote     *remote,
                                                               FoundryOperation     *operation);

G_END_DECLS
