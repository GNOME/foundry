/* foundry-git-repository-paths-private.h
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

G_BEGIN_DECLS

typedef struct _FoundryGitRepositoryPaths FoundryGitRepositoryPaths;

GType                      foundry_git_repository_paths_get_type                  (void) G_GNUC_CONST;
FoundryGitRepositoryPaths *foundry_git_repository_paths_new                       (const char                 *git_dir,
                                                                                   const char                 *workdir);
FoundryGitRepositoryPaths *foundry_git_repository_paths_ref                       (FoundryGitRepositoryPaths  *self);
void                       foundry_git_repository_paths_unref                     (FoundryGitRepositoryPaths  *self);
gboolean                   foundry_git_repository_paths_open                      (FoundryGitRepositoryPaths  *self,
                                                                                   git_repository            **repository_out,
                                                                                   GError                    **error);
GFile                     *foundry_git_repository_paths_get_workdir_file          (FoundryGitRepositoryPaths  *self,
                                                                                   const char                 *path);
char                      *foundry_git_repository_paths_get_workdir_path          (FoundryGitRepositoryPaths  *self,
                                                                                   const char                 *path);
char                      *foundry_git_repository_paths_dup_workdir               (FoundryGitRepositoryPaths  *self);
char                      *foundry_git_repository_paths_dup_git_dir               (FoundryGitRepositoryPaths  *self);
char                      *foundry_git_repository_paths_get_workdir_relative_path (FoundryGitRepositoryPaths  *self,
                                                                                   GFile                      *file);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FoundryGitRepositoryPaths, foundry_git_repository_paths_unref)

G_END_DECLS

