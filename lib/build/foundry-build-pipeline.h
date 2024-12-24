/* foundry-build-pipeline.h
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

#include "foundry-contextual.h"
#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_BUILD_PIPELINE (foundry_build_pipeline_get_type())

typedef enum _FoundryBuildPipelinePhase
{
  FOUNDRY_BUILD_PIPELINE_PHASE_NONE         = 0,
  FOUNDRY_BUILD_PIPELINE_PHASE_PREPARE      = 1 << 0,
  FOUNDRY_BUILD_PIPELINE_PHASE_DOWNLOADS    = 1 << 1,
  FOUNDRY_BUILD_PIPELINE_PHASE_DEPENDENCIES = 1 << 2,
  FOUNDRY_BUILD_PIPELINE_PHASE_AUTOGEN      = 1 << 3,
  FOUNDRY_BUILD_PIPELINE_PHASE_CONFIGURE    = 1 << 4,
  FOUNDRY_BUILD_PIPELINE_PHASE_BUILD        = 1 << 6,
  FOUNDRY_BUILD_PIPELINE_PHASE_INSTALL      = 1 << 7,
  FOUNDRY_BUILD_PIPELINE_PHASE_COMMIT       = 1 << 8,
  FOUNDRY_BUILD_PIPELINE_PHASE_EXPORT       = 1 << 9,
  FOUNDRY_BUILD_PIPELINE_PHASE_FINAL        = 1 << 10,

  FOUNDRY_BUILD_PIPELINE_PHASE_BEFORE       = 1 << 28,
  FOUNDRY_BUILD_PIPELINE_PHASE_AFTER        = 1 << 29,
  FOUNDRY_BUILD_PIPELINE_PHASE_FINISHED     = 1 << 30,
  FOUNDRY_BUILD_PIPELINE_PHASE_FAILED       = 1 << 31,
} FoundryBuildPipelinePhase;

#define FOUNDRY_BUILD_PIPELINE_PHASE_MASK(p)        ((p) & ((1<<11)-1))
#define FOUNDRY_BUILD_PIPELINE_PHASE_WHENCE_MASK(p) ((p) & (FOUNDRY_BUILD_PIPELINE_PHASE_BEFORE|FOUNDRY_BUILD_PIPELINE_PHASE_AFTER))

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (FoundryBuildPipeline, foundry_build_pipeline, FOUNDRY, BUILD_PIPELINE, FoundryContextual)

FOUNDRY_AVAILABLE_IN_ALL
DexFuture            *foundry_build_pipeline_new          (FoundryContext            *context,
                                                           FoundryConfig             *config,
                                                           FoundryDevice             *device,
                                                           FoundrySdk                *sdk) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_ALL
FoundryBuildProgress *foundry_build_pipeline_build        (FoundryBuildPipeline      *self,
                                                           FoundryBuildPipelinePhase  phase) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_ALL
FoundryConfig        *foundry_build_pipeline_dup_config   (FoundryBuildPipeline      *self);
FOUNDRY_AVAILABLE_IN_ALL
FoundryDevice        *foundry_build_pipeline_dup_device   (FoundryBuildPipeline      *self);
FOUNDRY_AVAILABLE_IN_ALL
FoundrySdk           *foundry_build_pipeline_dup_sdk      (FoundryBuildPipeline      *self);
FOUNDRY_AVAILABLE_IN_ALL
void                  foundry_build_pipeline_add_stage    (FoundryBuildPipeline      *self,
                                                           FoundryBuildStage         *stage);
FOUNDRY_AVAILABLE_IN_ALL
void                  foundry_build_pipeline_remove_stage (FoundryBuildPipeline      *self,
                                                           FoundryBuildStage         *stage);

G_END_DECLS
