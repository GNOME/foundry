/* foundry-vcs-revision.h
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_VCS_REVISION      (foundry_vcs_revision_get_type())
#define FOUNDRY_TYPE_VCS_REVISION_KIND (foundry_vcs_revision_kind_get_type())

typedef enum _FoundryVcsRevisionKind
{
  FOUNDRY_VCS_REVISION_KIND_COMMIT,
  FOUNDRY_VCS_REVISION_KIND_TREE,
  FOUNDRY_VCS_REVISION_KIND_INDEX,
  FOUNDRY_VCS_REVISION_KIND_WORKTREE,
  FOUNDRY_VCS_REVISION_KIND_EMPTY,
} FoundryVcsRevisionKind;

FOUNDRY_AVAILABLE_IN_1_3
G_DECLARE_FINAL_TYPE (FoundryVcsRevision, foundry_vcs_revision, FOUNDRY, VCS_REVISION, GObject)

FOUNDRY_AVAILABLE_IN_1_3
GType                   foundry_vcs_revision_kind_get_type    (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_1_3
FoundryVcsRevision     *foundry_vcs_revision_new_empty        (void);
FOUNDRY_AVAILABLE_IN_1_3
FoundryVcsRevision     *foundry_vcs_revision_new_for_commit   (FoundryVcsCommit   *commit);
FOUNDRY_AVAILABLE_IN_1_3
FoundryVcsRevision     *foundry_vcs_revision_new_for_index    (void);
FOUNDRY_AVAILABLE_IN_1_3
FoundryVcsRevision     *foundry_vcs_revision_new_for_tree     (FoundryVcsTree     *tree);
FOUNDRY_AVAILABLE_IN_1_3
FoundryVcsRevision     *foundry_vcs_revision_new_for_worktree (void);
FOUNDRY_AVAILABLE_IN_1_3
FoundryVcsRevisionKind  foundry_vcs_revision_get_kind         (FoundryVcsRevision *self);
FOUNDRY_AVAILABLE_IN_1_3
FoundryVcsCommit       *foundry_vcs_revision_dup_commit       (FoundryVcsRevision *self);
FOUNDRY_AVAILABLE_IN_1_3
FoundryVcsTree         *foundry_vcs_revision_dup_tree         (FoundryVcsRevision *self);

G_END_DECLS
