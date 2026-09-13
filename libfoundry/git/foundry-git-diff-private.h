/* foundry-git-diff-private.h
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

#include "foundry-git-diff.h"
#include "foundry-git-repository-paths-private.h"
#include "foundry-vcs-diff-options.h"

typedef enum _FoundryGitDiffEndpointKind
{
  FOUNDRY_GIT_DIFF_ENDPOINT_EMPTY,
  FOUNDRY_GIT_DIFF_ENDPOINT_TREE,
  FOUNDRY_GIT_DIFF_ENDPOINT_INDEX,
  FOUNDRY_GIT_DIFF_ENDPOINT_WORKTREE,
} FoundryGitDiffEndpointKind;

G_BEGIN_DECLS

gboolean                    _foundry_git_diff_contains_file     (FoundryGitDiff              *self,
                                                                 const char                  *relative_path);
FoundryGitRepositoryPaths  *_foundry_git_diff_dup_paths         (FoundryGitDiff              *diff);
const git_diff_delta       *_foundry_git_diff_get_delta         (FoundryGitDiff              *self,
                                                                 gsize                        delta_idx);
FoundryGitDiffEndpointKind  _foundry_git_diff_get_new_kind      (FoundryGitDiff              *self);
gsize                       _foundry_git_diff_get_num_deltas    (FoundryGitDiff              *self);
FoundryGitDiffEndpointKind  _foundry_git_diff_get_old_kind      (FoundryGitDiff              *self);
int                         _foundry_git_diff_get_stats         (FoundryGitDiff              *self,
                                                                 git_diff_stats             **out);
int                         _foundry_git_diff_patch_from_diff   (FoundryGitDiff              *self,
                                                                 git_patch                  **out,
                                                                 gsize                        delta_idx);
guint                       _foundry_git_diff_get_context_lines (FoundryGitDiff              *self);
FoundryGitDiff             *_foundry_git_diff_new_full          (git_diff                    *diff,
                                                                 FoundryGitRepositoryPaths   *paths,
                                                                 FoundryGitDiffEndpointKind   old_kind,
                                                                 FoundryGitDiffEndpointKind   new_kind,
                                                                 FoundryVcsDiffOptions       *options);
FoundryGitDiff             *_foundry_git_diff_new_with_paths    (git_diff                    *diff,
                                                                 FoundryGitRepositoryPaths   *paths);

G_END_DECLS
