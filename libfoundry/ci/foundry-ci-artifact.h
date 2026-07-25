/* foundry-ci-artifact.h
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

#include <libdex.h>

#include "foundry-ci-enums.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_CI_ARTIFACT (foundry_ci_artifact_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_FINAL_TYPE (FoundryCiArtifact, foundry_ci_artifact, FOUNDRY, CI_ARTIFACT, GObject)

FOUNDRY_AVAILABLE_IN_1_2
FoundryCiArtifact     *foundry_ci_artifact_new      (const char            *name,
                                                     GFile                 *file,
                                                     FoundryCiArtifactKind  kind);
FOUNDRY_AVAILABLE_IN_1_2
char                  *foundry_ci_artifact_dup_name (FoundryCiArtifact     *self);
FOUNDRY_AVAILABLE_IN_1_2
GFile                 *foundry_ci_artifact_dup_file (FoundryCiArtifact     *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryCiArtifactKind  foundry_ci_artifact_get_kind (FoundryCiArtifact     *self);

G_END_DECLS
