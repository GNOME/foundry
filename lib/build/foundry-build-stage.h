/* foundry-build-stage.h
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "foundry-build-pipeline.h"
#include "foundry-contextual.h"
#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_BUILD_STAGE (foundry_build_stage_get_type())

FOUNDRY_DECLARE_INTERNAL_TYPE (FoundryBuildStage, foundry_build_stage, FOUNDRY, BUILD_STAGE, FoundryContextual)

struct _FoundryBuildStage
{
  FoundryContextual parent_instance;
};

struct _FoundryBuildStageClass
{
  FoundryContextualClass parent_class;

  FoundryBuildPipelinePhase (*get_phase)    (FoundryBuildStage *self);
  guint                     (*get_priority) (FoundryBuildStage *self);

  /*< private >*/
  gpointer _reserved[8];
};

FOUNDRY_AVAILABLE_IN_ALL
FoundryBuildPipelinePhase foundry_build_stage_get_phase    (FoundryBuildStage *self);
FOUNDRY_AVAILABLE_IN_ALL
guint                     foundry_build_stage_get_priority (FoundryBuildStage *self);

G_END_DECLS
