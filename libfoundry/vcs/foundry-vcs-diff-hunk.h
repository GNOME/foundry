/* foundry-vcs-diff-hunk.h
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <libdex.h>

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_VCS_DIFF_HUNK (foundry_vcs_diff_hunk_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryVcsDiffHunk, foundry_vcs_diff_hunk, FOUNDRY, VCS_DIFF_HUNK, GObject)

struct _FoundryVcsDiffHunkClass
{
  GObjectClass parent_class;

  DexFuture *(*list_lines)    (FoundryVcsDiffHunk *self);
  char      *(*dup_header)    (FoundryVcsDiffHunk *self);
  guint      (*get_old_start) (FoundryVcsDiffHunk *self);
  guint      (*get_old_lines) (FoundryVcsDiffHunk *self);
  guint      (*get_new_start) (FoundryVcsDiffHunk *self);
  guint      (*get_new_lines) (FoundryVcsDiffHunk *self);

  /*< private >*/
  gpointer _reserved[17];
};

FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_vcs_diff_hunk_list_lines    (FoundryVcsDiffHunk *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_vcs_diff_hunk_dup_header    (FoundryVcsDiffHunk *self);
FOUNDRY_AVAILABLE_IN_1_1
guint      foundry_vcs_diff_hunk_get_old_start (FoundryVcsDiffHunk *self);
FOUNDRY_AVAILABLE_IN_1_1
guint      foundry_vcs_diff_hunk_get_old_lines (FoundryVcsDiffHunk *self);
FOUNDRY_AVAILABLE_IN_1_1
guint      foundry_vcs_diff_hunk_get_new_start (FoundryVcsDiffHunk *self);
FOUNDRY_AVAILABLE_IN_1_1
guint      foundry_vcs_diff_hunk_get_new_lines (FoundryVcsDiffHunk *self);

G_END_DECLS
