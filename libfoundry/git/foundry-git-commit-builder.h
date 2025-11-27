/* foundry-git-commit-builder.h
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

#include <libdex.h>

#include "foundry-git-commit.h"
#include "foundry-git-vcs.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_GIT_COMMIT_BUILDER (foundry_git_commit_builder_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryGitCommitBuilder, foundry_git_commit_builder, FOUNDRY, GIT_COMMIT_BUILDER, GObject)

FOUNDRY_AVAILABLE_IN_1_1
DexFuture  *foundry_git_commit_builder_new                 (FoundryGitVcs           *vcs,
                                                            FoundryGitCommit        *parent,
                                                            guint                    context_lines);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture  *foundry_git_commit_builder_new_similar         (FoundryGitCommitBuilder *self);
FOUNDRY_AVAILABLE_IN_1_1
gboolean    foundry_git_commit_builder_get_can_commit      (FoundryGitCommitBuilder *self);
FOUNDRY_AVAILABLE_IN_1_1
GListModel *foundry_git_commit_builder_list_staged         (FoundryGitCommitBuilder *self);
FOUNDRY_AVAILABLE_IN_1_1
GListModel *foundry_git_commit_builder_list_unstaged       (FoundryGitCommitBuilder *self);
FOUNDRY_AVAILABLE_IN_1_1
GListModel *foundry_git_commit_builder_list_untracked      (FoundryGitCommitBuilder *self);
FOUNDRY_AVAILABLE_IN_1_1
char       *foundry_git_commit_builder_dup_author_name     (FoundryGitCommitBuilder *self);
FOUNDRY_AVAILABLE_IN_1_1
void        foundry_git_commit_builder_set_author_name     (FoundryGitCommitBuilder *self,
                                                            const char              *author_name);
FOUNDRY_AVAILABLE_IN_1_1
char       *foundry_git_commit_builder_dup_author_email    (FoundryGitCommitBuilder *self);
FOUNDRY_AVAILABLE_IN_1_1
void        foundry_git_commit_builder_set_author_email    (FoundryGitCommitBuilder *self,
                                                            const char              *author_email);
FOUNDRY_AVAILABLE_IN_1_1
GDateTime  *foundry_git_commit_builder_dup_when            (FoundryGitCommitBuilder *self);
FOUNDRY_AVAILABLE_IN_1_1
void        foundry_git_commit_builder_set_when            (FoundryGitCommitBuilder *self,
                                                            GDateTime               *when);
FOUNDRY_AVAILABLE_IN_1_1
char       *foundry_git_commit_builder_dup_signing_key     (FoundryGitCommitBuilder *self);
FOUNDRY_AVAILABLE_IN_1_1
void        foundry_git_commit_builder_set_signing_key     (FoundryGitCommitBuilder *self,
                                                            const char              *signing_key);
FOUNDRY_AVAILABLE_IN_1_1
char       *foundry_git_commit_builder_dup_signing_format  (FoundryGitCommitBuilder *self);
FOUNDRY_AVAILABLE_IN_1_1
void        foundry_git_commit_builder_set_signing_format  (FoundryGitCommitBuilder *self,
                                                            const char              *signing_format);
FOUNDRY_AVAILABLE_IN_1_1
char       *foundry_git_commit_builder_dup_message         (FoundryGitCommitBuilder *self);
FOUNDRY_AVAILABLE_IN_1_1
void        foundry_git_commit_builder_set_message         (FoundryGitCommitBuilder *self,
                                                            const char              *message);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture  *foundry_git_commit_builder_stage_file          (FoundryGitCommitBuilder *self,
                                                            GFile                   *file);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture  *foundry_git_commit_builder_unstage_file        (FoundryGitCommitBuilder *self,
                                                            GFile                   *file);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture  *foundry_git_commit_builder_commit              (FoundryGitCommitBuilder *self);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture  *foundry_git_commit_builder_load_staged_delta   (FoundryGitCommitBuilder *self,
                                                            GFile                   *file);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture  *foundry_git_commit_builder_load_unstaged_delta (FoundryGitCommitBuilder *self,
                                                            GFile                   *file);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture  *foundry_git_commit_builder_stage_hunks         (FoundryGitCommitBuilder *self,
                                                            GFile                   *file,
                                                            GListModel              *hunks);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture  *foundry_git_commit_builder_stage_lines         (FoundryGitCommitBuilder *self,
                                                            GFile                   *file,
                                                            GListModel              *lines);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture  *foundry_git_commit_builder_unstage_hunks       (FoundryGitCommitBuilder *self,
                                                            GFile                   *file,
                                                            GListModel              *hunks);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture  *foundry_git_commit_builder_unstage_lines       (FoundryGitCommitBuilder *self,
                                                            GFile                   *file,
                                                            GListModel              *lines);

G_END_DECLS
