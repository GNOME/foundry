/* foundry-git-patch-private.h
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

#include <glib.h>
#include <git2.h>

G_BEGIN_DECLS

typedef struct _FoundryGitPatch FoundryGitPatch;

FoundryGitPatch     *_foundry_git_patch_new                   (git_patch       *patch);
FoundryGitPatch     *_foundry_git_patch_ref                   (FoundryGitPatch *patch);
void                 _foundry_git_patch_unref                 (FoundryGitPatch *patch);
gsize                _foundry_git_patch_get_num_hunks         (FoundryGitPatch *patch);
const git_diff_hunk *_foundry_git_patch_get_hunk              (FoundryGitPatch *patch,
                                                               gsize            hunk_idx);
gsize                _foundry_git_patch_get_num_lines_in_hunk (FoundryGitPatch *patch,
                                                               gsize            hunk_idx);
const git_diff_line *_foundry_git_patch_get_line              (FoundryGitPatch *patch,
                                                               gsize            hunk_idx,
                                                               gsize            line_idx);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FoundryGitPatch, _foundry_git_patch_unref)

G_END_DECLS
