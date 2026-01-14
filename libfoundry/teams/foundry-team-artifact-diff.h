/* foundry-team-artifact-diff.h
 *
 * Copyright 2026 Christian Hergert
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

#include "foundry-team-artifact.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_TEAM_ARTIFACT_DIFF (foundry_team_artifact_diff_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryTeamArtifactDiff, foundry_team_artifact_diff, FOUNDRY, TEAM_ARTIFACT_DIFF, FoundryTeamArtifact)

FOUNDRY_AVAILABLE_IN_1_1
FoundryTeamArtifactDiff *foundry_team_artifact_diff_new      (const char              *diff);
FOUNDRY_AVAILABLE_IN_1_1
char                    *foundry_team_artifact_diff_dup_diff (FoundryTeamArtifactDiff *self);

G_END_DECLS
