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

#include "foundry-build-stage.h"

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryBuildStage, foundry_build_stage, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static void
foundry_build_stage_finalize (GObject *object)
{
  G_OBJECT_CLASS (foundry_build_stage_parent_class)->finalize (object);
}

static void
foundry_build_stage_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundryBuildStage *self = FOUNDRY_BUILD_STAGE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_build_stage_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  FoundryBuildStage *self = FOUNDRY_BUILD_STAGE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_build_stage_class_init (FoundryBuildStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_build_stage_finalize;
  object_class->get_property = foundry_build_stage_get_property;
  object_class->set_property = foundry_build_stage_set_property;
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
