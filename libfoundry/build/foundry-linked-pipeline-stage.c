/* foundry-linked-pipeline-stage.c
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
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

#include <glib/gi18n-lib.h>

#include "foundry-linked-pipeline-stage.h"
#include "foundry-build-progress.h"

/**
 * FoundryLinkedPipelineStage:
 *
 * A pipeline stage that will execute another pipline before continuing
 * the current pipeline.
 */

struct _FoundryLinkedPipelineStage
{
  FoundryBuildStage          parent_instance;
  FoundryBuildPipeline      *linked_pipeline;
  FoundryBuildPipelinePhase  phase;
  FoundryBuildPipelinePhase  linked_phase;
};

enum {
  PROP_0,
  PROP_LINKED_PIPELINE,
  PROP_PHASE,
  PROP_LINKED_PHASE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryLinkedPipelineStage, foundry_linked_pipeline_stage, FOUNDRY_TYPE_BUILD_STAGE)

static GParamSpec *properties[N_PROPS];

static FoundryBuildPipelinePhase
foundry_linked_pipeline_stage_get_phase (FoundryBuildStage *stage)
{
  return FOUNDRY_LINKED_PIPELINE_STAGE (stage)->phase;
}

static DexFuture *
foundry_linked_pipeline_stage_query_fiber (gpointer data)
{
  FoundryLinkedPipelineStage *self = data;
  FoundryBuildPipelinePhase pipeline_phase;
  FoundryBuildPipelinePhase pipeline_phase_masked;
  FoundryBuildPipelinePhase linked_phase_masked;

  g_assert (FOUNDRY_IS_LINKED_PIPELINE_STAGE (self));

  dex_await (foundry_build_pipeline_query (self->linked_pipeline), NULL);

  pipeline_phase = foundry_build_pipeline_get_phase (self->linked_pipeline);
  pipeline_phase_masked = FOUNDRY_BUILD_PIPELINE_PHASE_MASK (pipeline_phase);
  linked_phase_masked = FOUNDRY_BUILD_PIPELINE_PHASE_MASK (self->linked_phase);

  if (pipeline_phase_masked >= linked_phase_masked)
    foundry_build_stage_set_completed (FOUNDRY_BUILD_STAGE (self), TRUE);
  else
    foundry_build_stage_set_completed (FOUNDRY_BUILD_STAGE (self), FALSE);

  return dex_future_new_true ();
}


static DexFuture *
foundry_linked_pipeline_stage_query (FoundryBuildStage *stage)
{
  FoundryLinkedPipelineStage *self = FOUNDRY_LINKED_PIPELINE_STAGE (stage);

  g_assert (FOUNDRY_IS_LINKED_PIPELINE_STAGE (self));

  return dex_scheduler_spawn (NULL, 0,
                              foundry_linked_pipeline_stage_query_fiber,
                              g_object_ref (stage),
                              g_object_unref);
}

static DexFuture *
foundry_linked_pipeline_stage_build (FoundryBuildStage    *stage,
                                     FoundryBuildProgress *progress)
{
  FoundryLinkedPipelineStage *self = FOUNDRY_LINKED_PIPELINE_STAGE (stage);
  g_autoptr(FoundryBuildProgress) linked_progress = NULL;
  g_autoptr(DexCancellable) cancellable = NULL;
  g_autoptr(FoundryContext) other_context = NULL;
  g_autoptr(GFile) state = NULL;

  g_assert (FOUNDRY_IS_LINKED_PIPELINE_STAGE (self));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (progress));

  other_context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self->linked_pipeline));
  state = foundry_context_dup_project_directory (other_context);

  g_debug ("Building linked pipeline at `%s`",
           g_file_peek_path (state));

  cancellable = foundry_build_progress_dup_cancellable (progress);

  linked_progress = foundry_build_pipeline_build (self->linked_pipeline,
                                                  self->linked_phase,
                                                  -1,
                                                  cancellable);

  return foundry_build_progress_await (linked_progress);
}

static DexFuture *
foundry_linked_pipeline_stage_clean (FoundryBuildStage    *stage,
                                     FoundryBuildProgress *progress)
{
  FoundryLinkedPipelineStage *self = FOUNDRY_LINKED_PIPELINE_STAGE (stage);
  g_autoptr(FoundryBuildProgress) linked_progress = NULL;
  g_autoptr(DexCancellable) cancellable = NULL;
  g_autoptr(FoundryContext) other_context = NULL;
  g_autoptr(GFile) state = NULL;

  g_assert (FOUNDRY_IS_LINKED_PIPELINE_STAGE (self));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (progress));

  other_context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self->linked_pipeline));
  state = foundry_context_dup_project_directory (other_context);

  g_debug ("Cleaning linked pipeline at `%s`",
           g_file_peek_path (state));

  cancellable = foundry_build_progress_dup_cancellable (progress);

  linked_progress = foundry_build_pipeline_clean (self->linked_pipeline,
                                                  self->linked_phase,
                                                  -1,
                                                  cancellable);

  return foundry_build_progress_await (linked_progress);
}

static DexFuture *
foundry_linked_pipeline_stage_purge (FoundryBuildStage    *stage,
                                     FoundryBuildProgress *progress)
{
  FoundryLinkedPipelineStage *self = FOUNDRY_LINKED_PIPELINE_STAGE (stage);
  g_autoptr(FoundryBuildProgress) linked_progress = NULL;
  g_autoptr(DexCancellable) cancellable = NULL;
  g_autoptr(FoundryContext) other_context = NULL;
  g_autoptr(GFile) state = NULL;

  g_assert (FOUNDRY_IS_LINKED_PIPELINE_STAGE (self));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (progress));

  other_context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self->linked_pipeline));
  state = foundry_context_dup_project_directory (other_context);

  g_debug ("Purging linked pipeline at `%s`",
           g_file_peek_path (state));

  cancellable = foundry_build_progress_dup_cancellable (progress);

  linked_progress = foundry_build_pipeline_purge (self->linked_pipeline,
                                                  self->linked_phase,
                                                  -1,
                                                  cancellable);

  return foundry_build_progress_await (linked_progress);
}

static void
foundry_linked_pipeline_stage_dispose (GObject *object)
{
  FoundryLinkedPipelineStage *self = (FoundryLinkedPipelineStage *)object;

  g_clear_object (&self->linked_pipeline);

  G_OBJECT_CLASS (foundry_linked_pipeline_stage_parent_class)->dispose (object);
}

static void
foundry_linked_pipeline_stage_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  FoundryLinkedPipelineStage *self = FOUNDRY_LINKED_PIPELINE_STAGE (object);

  switch (prop_id)
    {
    case PROP_LINKED_PIPELINE:
      g_value_set_object (value, self->linked_pipeline);
      break;

    case PROP_PHASE:
      g_value_set_flags (value, self->phase);
      break;

    case PROP_LINKED_PHASE:
      g_value_set_flags (value, self->linked_phase);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_linked_pipeline_stage_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  FoundryLinkedPipelineStage *self = FOUNDRY_LINKED_PIPELINE_STAGE (object);

  switch (prop_id)
    {
    case PROP_LINKED_PIPELINE:
      self->linked_pipeline = g_value_dup_object (value);
      break;

    case PROP_PHASE:
      self->phase = g_value_get_flags (value);
      break;

    case PROP_LINKED_PHASE:
      self->linked_phase = g_value_get_flags (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_linked_pipeline_stage_class_init (FoundryLinkedPipelineStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryBuildStageClass *build_stage_class = FOUNDRY_BUILD_STAGE_CLASS (klass);

  object_class->dispose = foundry_linked_pipeline_stage_dispose;
  object_class->get_property = foundry_linked_pipeline_stage_get_property;
  object_class->set_property = foundry_linked_pipeline_stage_set_property;

  build_stage_class->get_phase = foundry_linked_pipeline_stage_get_phase;
  build_stage_class->query = foundry_linked_pipeline_stage_query;
  build_stage_class->build = foundry_linked_pipeline_stage_build;
  build_stage_class->clean = foundry_linked_pipeline_stage_clean;
  build_stage_class->purge = foundry_linked_pipeline_stage_purge;

  properties[PROP_LINKED_PIPELINE] =
    g_param_spec_object ("linked-pipeline", NULL, NULL,
                         FOUNDRY_TYPE_BUILD_PIPELINE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PHASE] =
    g_param_spec_flags ("phase", NULL, NULL,
                        FOUNDRY_TYPE_BUILD_PIPELINE_PHASE,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_LINKED_PHASE] =
    g_param_spec_flags ("linked-phase", NULL, NULL,
                        FOUNDRY_TYPE_BUILD_PIPELINE_PHASE,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_linked_pipeline_stage_init (FoundryLinkedPipelineStage *self)
{
}

FoundryBuildStage *
foundry_linked_pipeline_stage_new (FoundryContext            *context,
                                   FoundryBuildPipeline      *linked_pipeline,
                                   FoundryBuildPipelinePhase  phase)
{
  return foundry_linked_pipeline_stage_new_full (context,
                                                 linked_pipeline,
                                                 phase,
                                                 FOUNDRY_BUILD_PIPELINE_PHASE_INSTALL);
}

/**
 * foundry_linked_pipeline_stage_new_full:
 * @context: a [class@Foundry.Context]
 * @linked_pipeline: a [class@Foundry.BuildPipeline] to link
 * @phase: the phase of our pipeline when this stage should run
 * @linked_phase: the phase of the linked pipeline to execute
 *
 * Creates a new linked pipeline stage that will execute @linked_pipeline
 * at @linked_phase when our pipeline reaches @phase.
 *
 * Returns: (transfer full): a new [class@Foundry.BuildStage]
 *
 * Since: 1.1
 */
FoundryBuildStage *
foundry_linked_pipeline_stage_new_full (FoundryContext            *context,
                                        FoundryBuildPipeline      *linked_pipeline,
                                        FoundryBuildPipelinePhase  phase,
                                        FoundryBuildPipelinePhase  linked_phase)
{
  g_autoptr(FoundryContext) other_context = NULL;
  g_autofree char *title = NULL;

  g_return_val_if_fail (FOUNDRY_IS_BUILD_PIPELINE (linked_pipeline), NULL);
  g_return_val_if_fail (phase != 0, NULL);
  g_return_val_if_fail (linked_phase != 0, NULL);

  if ((other_context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (linked_pipeline))))
    {
      g_autofree char *context_title = foundry_context_dup_title (other_context);

      if (context_title == NULL)
        {
          g_autoptr(GFile) file = foundry_context_dup_project_directory (other_context);
          g_autofree char *basename = g_file_get_basename (file);

          context_title = g_utf8_make_valid (basename, -1);
        }

      /* translators: %s is replaced with the name of the linked project, such as "GTK" or "GLib" */
      title = g_strdup_printf (_("Build %s"), context_title);
    }

  return g_object_new (FOUNDRY_TYPE_LINKED_PIPELINE_STAGE,
                       "context", context,
                       "linked-pipeline", linked_pipeline,
                       "phase", phase,
                       "linked-phase", linked_phase,
                       "title", title,
                       "kind", "linked-workspace",
                       NULL);
}
