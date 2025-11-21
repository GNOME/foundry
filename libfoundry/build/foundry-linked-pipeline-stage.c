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
};

enum {
  PROP_0,
  PROP_LINKED_PIPELINE,
  PROP_PHASE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryLinkedPipelineStage, foundry_linked_pipeline_stage, FOUNDRY_TYPE_BUILD_STAGE)

static GParamSpec *properties[N_PROPS];

static FoundryBuildPipelinePhase
foundry_linked_pipeline_stage_get_phase (FoundryBuildStage *stage)
{
  return FOUNDRY_LINKED_PIPELINE_STAGE (stage)->phase;
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
  g_autoptr(FoundryContext) other_context = NULL;
  g_autofree char *title = NULL;

  g_return_val_if_fail (FOUNDRY_IS_BUILD_PIPELINE (linked_pipeline), NULL);
  g_return_val_if_fail (phase != 0, NULL);

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
                       "title", title,
                       "kind", "linked-workspace",
                       NULL);
}
