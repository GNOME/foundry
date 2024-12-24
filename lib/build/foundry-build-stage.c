/* foundry-build-stage.c
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

#include "config.h"

#include "foundry-build-progress.h"
#include "foundry-build-stage.h"

G_DEFINE_ABSTRACT_TYPE (FoundryBuildStage, foundry_build_stage, FOUNDRY_TYPE_CONTEXTUAL)

static DexFuture *
foundry_build_stage_real_build (FoundryBuildStage    *self,
                                FoundryBuildProgress *progress)
{
  return dex_future_new_true ();
}

static DexFuture *
foundry_build_stage_real_clean (FoundryBuildStage    *self,
                                FoundryBuildProgress *progress)
{
  return dex_future_new_true ();
}

static DexFuture *
foundry_build_stage_real_purge (FoundryBuildStage    *self,
                                FoundryBuildProgress *progress)
{
  return dex_future_new_true ();
}

static void
foundry_build_stage_class_init (FoundryBuildStageClass *klass)
{
  klass->build = foundry_build_stage_real_build;
  klass->clean = foundry_build_stage_real_clean;
  klass->purge = foundry_build_stage_real_purge;
}

static void
foundry_build_stage_init (FoundryBuildStage *self)
{
}

FoundryBuildPipelinePhase
foundry_build_stage_get_phase (FoundryBuildStage *self)
{
  g_return_val_if_fail (FOUNDRY_IS_BUILD_STAGE (self), 0);

  if (FOUNDRY_BUILD_STAGE_GET_CLASS (self)->get_phase)
    return FOUNDRY_BUILD_STAGE_GET_CLASS (self)->get_phase (self);

  g_return_val_if_reached (0);
}

guint
foundry_build_stage_get_priority (FoundryBuildStage *self)
{
  g_return_val_if_fail (FOUNDRY_IS_BUILD_STAGE (self), 0);

  if (FOUNDRY_BUILD_STAGE_GET_CLASS (self)->get_priority)
    return FOUNDRY_BUILD_STAGE_GET_CLASS (self)->get_priority (self);

  return 0;
}

/**
 * foundry_build_stage_build:
 * @self: a [class@Foundry.BuildStage]
 *
 * Run the build for the stage.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   any value or rejects with an error.
 */
DexFuture *
foundry_build_stage_build (FoundryBuildStage    *self,
                           FoundryBuildProgress *progress)
{
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_STAGE (self));
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_PROGRESS (progress));

  return FOUNDRY_BUILD_STAGE_GET_CLASS (self)->build (self, progress);
}

/**
 * foundry_build_stage_clean:
 * @self: a [class@Foundry.BuildStage]
 *
 * Clean operation for the stage.
 *
 * This is used to perform an equivalent of a `make clean` or
 * `ninja clean` for the build. It is not necessary on all stages
 * but any stage may implement it.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   any value or rejects with an error.
 */
DexFuture *
foundry_build_stage_clean (FoundryBuildStage    *self,
                           FoundryBuildProgress *progress)
{
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_STAGE (self));
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_PROGRESS (progress));

  return FOUNDRY_BUILD_STAGE_GET_CLASS (self)->clean (self, progress);
}

/**
 * foundry_build_stage_purge:
 * @self: a [class@Foundry.BuildStage]
 *
 * Purge operation for the stage.
 *
 * This is used to perform a purge of an existing pipeline.
 *
 * The purge command is run when doing a rebuild.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   any value or rejects with an error.
 */
DexFuture *
foundry_build_stage_purge (FoundryBuildStage    *self,
                           FoundryBuildProgress *progress)
{
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_STAGE (self));
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_PROGRESS (progress));

  return FOUNDRY_BUILD_STAGE_GET_CLASS (self)->purge (self, progress);
}
