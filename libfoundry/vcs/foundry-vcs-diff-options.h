/* foundry-vcs-diff-options.h
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

#define FOUNDRY_TYPE_VCS_DIFF_OPTIONS (foundry_vcs_diff_options_get_type())

FOUNDRY_AVAILABLE_IN_1_3
G_DECLARE_FINAL_TYPE (FoundryVcsDiffOptions, foundry_vcs_diff_options, FOUNDRY, VCS_DIFF_OPTIONS, GObject)

FOUNDRY_AVAILABLE_IN_1_3
FoundryVcsDiffOptions *foundry_vcs_diff_options_copy                  (FoundryVcsDiffOptions *self);
FOUNDRY_AVAILABLE_IN_1_3
guint                  foundry_vcs_diff_options_get_context_lines     (FoundryVcsDiffOptions *self);
FOUNDRY_AVAILABLE_IN_1_3
gboolean               foundry_vcs_diff_options_get_include_untracked (FoundryVcsDiffOptions *self);
FOUNDRY_AVAILABLE_IN_1_3
FoundryVcsDiffOptions *foundry_vcs_diff_options_new                   (void);
FOUNDRY_AVAILABLE_IN_1_3
void                   foundry_vcs_diff_options_set_context_lines     (FoundryVcsDiffOptions *self,
                                                                       guint                  context_lines);
FOUNDRY_AVAILABLE_IN_1_3
void                   foundry_vcs_diff_options_set_include_untracked (FoundryVcsDiffOptions *self,
                                                                       gboolean               include_untracked);

G_END_DECLS
