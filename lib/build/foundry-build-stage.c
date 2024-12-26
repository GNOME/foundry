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
#include "foundry-build-stage-private.h"

typedef struct
{
  GWeakRef pipeline_wr;
  char *kind;
  char *title;
} FoundryBuildStagePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryBuildStage, foundry_build_stage, FOUNDRY_TYPE_CONTEXTUAL)

enum {
  PROP_0,
  PROP_KIND,
  PROP_PHASE,
  PROP_PIPELINE,
  PROP_PRIORITY,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

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
foundry_build_stage_finalize (GObject *object)
{
  FoundryBuildStage *self = (FoundryBuildStage *)object;
  FoundryBuildStagePrivate *priv = foundry_build_stage_get_instance_private (self);

  g_weak_ref_clear (&priv->pipeline_wr);

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
    case PROP_KIND:
      g_value_take_string (value, foundry_build_stage_dup_kind (self));
      break;

    case PROP_PHASE:
      g_value_set_flags (value, foundry_build_stage_get_phase (self));
      break;

    case PROP_PIPELINE:
      g_value_take_object (value, foundry_build_stage_dup_pipeline (self));
      break;

    case PROP_PRIORITY:
      g_value_set_uint (value, foundry_build_stage_get_priority (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, foundry_build_stage_dup_title (self));
      break;

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
    case PROP_KIND:
      foundry_build_stage_set_kind (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      foundry_build_stage_set_title (self, g_value_get_string (value));
      break;

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

  klass->build = foundry_build_stage_real_build;
  klass->clean = foundry_build_stage_real_clean;
  klass->purge = foundry_build_stage_real_purge;

  properties[PROP_KIND] =
    g_param_spec_string ("kind", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PHASE] =
    g_param_spec_flags ("phase", NULL, NULL,
                        FOUNDRY_TYPE_BUILD_PIPELINE_PHASE,
                        FOUNDRY_BUILD_PIPELINE_PHASE_NONE,
                        (G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_PIPELINE] =
    g_param_spec_object ("pipeline", NULL, NULL,
                         FOUNDRY_TYPE_BUILD_PIPELINE,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PRIORITY] =
    g_param_spec_uint ("priority", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_build_stage_init (FoundryBuildStage *self)
{
  FoundryBuildStagePrivate *priv = foundry_build_stage_get_instance_private (self);

  priv->kind = g_strdup ("unspecified");
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

gboolean
_foundry_build_stage_matches (FoundryBuildStage         *self,
                              FoundryBuildPipelinePhase  phase)
{
  FoundryBuildPipelinePhase our_phase;

  g_return_val_if_fail (FOUNDRY_IS_BUILD_STAGE (self), FALSE);
  g_return_val_if_fail (FOUNDRY_BUILD_PIPELINE_PHASE_MASK (phase) != 0, FALSE);

  our_phase = foundry_build_stage_get_phase (self);

  return FOUNDRY_BUILD_PIPELINE_PHASE_MASK (our_phase) <= FOUNDRY_BUILD_PIPELINE_PHASE_MASK (phase);
}

/**
 * foundry_build_stage_dup_pipeline:
 * @self: a [class@Foundry.BuildStage]
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.Pipeline] or %NULL
 */
FoundryBuildPipeline *
foundry_build_stage_dup_pipeline (FoundryBuildStage *self)
{
  FoundryBuildStagePrivate *priv = foundry_build_stage_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_BUILD_STAGE (self), NULL);

  return g_weak_ref_get (&priv->pipeline_wr);
}

void
_foundry_build_stage_set_pipeline (FoundryBuildStage    *self,
                                   FoundryBuildPipeline *pipeline)
{
  FoundryBuildStagePrivate *priv = foundry_build_stage_get_instance_private (self);
  g_autoptr(FoundryBuildPipeline) prev_pipeline = NULL;

  g_return_if_fail (FOUNDRY_IS_BUILD_STAGE (self));
  g_return_if_fail (!pipeline || FOUNDRY_IS_BUILD_PIPELINE (pipeline));

  prev_pipeline = g_weak_ref_get (&priv->pipeline_wr);

  if (prev_pipeline == pipeline)
    return;

  if (prev_pipeline != NULL && pipeline != NULL)
    {
      g_critical ("Attempt to set pipeline on %s while already attached to a pipeline.",
                  G_OBJECT_TYPE_NAME (self));
      return;
    }

  g_weak_ref_set (&priv->pipeline_wr, pipeline);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PIPELINE]);
}

/**
 * foundry_build_stage_dup_title:
 * @self: a [class@Foundry.BuildStage]
 *
 * Returns: (transfer full) (nullable): the title of the stage
 */
char *
foundry_build_stage_dup_title (FoundryBuildStage *self)
{
  FoundryBuildStagePrivate *priv = foundry_build_stage_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_BUILD_STAGE (self), NULL);

  return g_strdup (priv->title);
}

void
foundry_build_stage_set_title (FoundryBuildStage *self,
                               const char        *title)
{
  FoundryBuildStagePrivate *priv = foundry_build_stage_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_BUILD_STAGE (self));

  if (g_set_str (&priv->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

/**
 * foundry_build_stage_dup_kind:
 * @self: a [class@Foundry.BuildStage]
 *
 * Returns: (transfer full) (nullable): the kind of the stage such as "flatpak"
 */
char *
foundry_build_stage_dup_kind (FoundryBuildStage *self)
{
  FoundryBuildStagePrivate *priv = foundry_build_stage_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_BUILD_STAGE (self), NULL);

  return g_strdup (priv->kind);
}

void
foundry_build_stage_set_kind (FoundryBuildStage *self,
                               const char        *kind)
{
  FoundryBuildStagePrivate *priv = foundry_build_stage_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_BUILD_STAGE (self));

  if (kind == NULL)
    kind = "unspecified";

  if (g_set_str (&priv->kind, kind))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KIND]);
}
