/* foundry-ci-job.c
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

#include "config.h"

#include "foundry-ci-job.h"
#include "foundry-ci-pipeline.h"
#include "foundry-ci-provider.h"

/**
 * FoundryCiJob:
 *
 * Abstract representation of a job in a continuous integration pipeline.
 *
 * CI providers subclass `FoundryCiJob` and implement the virtual getters.
 * The corresponding properties are read-only. Subclasses whose values can
 * change must emit `notify` for the affected property.
 *
 * Since: 1.2
 */

enum {
  PROP_0,
  PROP_CAN_RUN,
  PROP_CAN_SHELL,
  PROP_DISPOSITION,
  PROP_ID,
  PROP_IMAGE,
  PROP_PIPELINE,
  PROP_PROVIDER,
  PROP_REASON,
  PROP_STAGE,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryCiJob, foundry_ci_job, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static char *
foundry_ci_job_real_dup_string (FoundryCiJob *self)
{
  return NULL;
}

static FoundryCiJobDisposition
foundry_ci_job_real_get_disposition (FoundryCiJob *self)
{
  return FOUNDRY_CI_JOB_DISPOSITION_UNSUPPORTED;
}

static gboolean
foundry_ci_job_real_get_boolean (FoundryCiJob *self)
{
  return FALSE;
}

static FoundryCiPipeline *
foundry_ci_job_real_dup_pipeline (FoundryCiJob *self)
{
  return NULL;
}

static FoundryCiProvider *
foundry_ci_job_real_dup_provider (FoundryCiJob *self)
{
  return NULL;
}

static void
foundry_ci_job_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  FoundryCiJob *self = FOUNDRY_CI_JOB (object);

  switch (prop_id)
    {
    case PROP_CAN_RUN:
      g_value_set_boolean (value, foundry_ci_job_get_can_run (self));
      break;

    case PROP_CAN_SHELL:
      g_value_set_boolean (value, foundry_ci_job_get_can_shell (self));
      break;

    case PROP_DISPOSITION:
      g_value_set_enum (value, foundry_ci_job_get_disposition (self));
      break;

    case PROP_ID:
      g_value_take_string (value, foundry_ci_job_dup_id (self));
      break;

    case PROP_IMAGE:
      g_value_take_string (value, foundry_ci_job_dup_image (self));
      break;

    case PROP_PIPELINE:
      g_value_take_object (value, foundry_ci_job_dup_pipeline (self));
      break;

    case PROP_PROVIDER:
      g_value_take_object (value, foundry_ci_job_dup_provider (self));
      break;

    case PROP_REASON:
      g_value_take_string (value, foundry_ci_job_dup_reason (self));
      break;

    case PROP_STAGE:
      g_value_take_string (value, foundry_ci_job_dup_stage (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, foundry_ci_job_dup_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
foundry_ci_job_class_init (FoundryCiJobClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_ci_job_get_property;

  klass->dup_id = foundry_ci_job_real_dup_string;
  klass->dup_title = foundry_ci_job_real_dup_string;
  klass->dup_stage = foundry_ci_job_real_dup_string;
  klass->dup_image = foundry_ci_job_real_dup_string;
  klass->dup_reason = foundry_ci_job_real_dup_string;
  klass->get_disposition = foundry_ci_job_real_get_disposition;
  klass->get_can_run = foundry_ci_job_real_get_boolean;
  klass->get_can_shell = foundry_ci_job_real_get_boolean;
  klass->dup_pipeline = foundry_ci_job_real_dup_pipeline;
  klass->dup_provider = foundry_ci_job_real_dup_provider;

  /**
   * FoundryCiJob:can-run:
   *
   * Whether the job can be run locally.
   *
   * Since: 1.2
   */
  properties[PROP_CAN_RUN] =
    g_param_spec_boolean ("can-run", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiJob:can-shell:
   *
   * Whether an interactive shell can be opened for the job.
   *
   * Since: 1.2
   */
  properties[PROP_CAN_SHELL] =
    g_param_spec_boolean ("can-shell", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiJob:disposition:
   *
   * How the provider selected or excluded the job.
   *
   * Since: 1.2
   */
  properties[PROP_DISPOSITION] =
    g_param_spec_enum ("disposition", NULL, NULL,
                       FOUNDRY_TYPE_CI_JOB_DISPOSITION,
                       FOUNDRY_CI_JOB_DISPOSITION_SELECTED,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiJob:id:
   *
   * The provider-specific job identifier.
   *
   * Since: 1.2
   */
  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiJob:image:
   *
   * The execution image requested by the job.
   *
   * Since: 1.2
   */
  properties[PROP_IMAGE] =
    g_param_spec_string ("image", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiJob:pipeline:
   *
   * The pipeline containing the job.
   *
   * Since: 1.2
   */
  properties[PROP_PIPELINE] =
    g_param_spec_object ("pipeline", NULL, NULL,
                         FOUNDRY_TYPE_CI_PIPELINE,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiJob:provider:
   *
   * The provider which created the job.
   *
   * Since: 1.2
   */
  properties[PROP_PROVIDER] =
    g_param_spec_object ("provider", NULL, NULL,
                         FOUNDRY_TYPE_CI_PROVIDER,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiJob:reason:
   *
   * The provider's explanation for the job disposition.
   *
   * Since: 1.2
   */
  properties[PROP_REASON] =
    g_param_spec_string ("reason", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiJob:stage:
   *
   * The pipeline stage containing the job.
   *
   * Since: 1.2
   */
  properties[PROP_STAGE] =
    g_param_spec_string ("stage", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiJob:title:
   *
   * The user-visible job title.
   *
   * Since: 1.2
   */
  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_ci_job_init (FoundryCiJob *self)
{
}

/**
 * foundry_ci_job_dup_id:
 * @self: a [class@Foundry.CiJob]
 *
 * Gets the provider-specific identifier for the job.
 *
 * Returns: (transfer full) (nullable): the job identifier
 *
 * Since: 1.2
 */
char *
foundry_ci_job_dup_id (FoundryCiJob *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_JOB (self), NULL);

  return FOUNDRY_CI_JOB_GET_CLASS (self)->dup_id (self);
}

/**
 * foundry_ci_job_dup_title:
 * @self: a [class@Foundry.CiJob]
 *
 * Gets the user-visible title of the job.
 *
 * Returns: (transfer full) (nullable): the job title
 *
 * Since: 1.2
 */
char *
foundry_ci_job_dup_title (FoundryCiJob *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_JOB (self), NULL);

  return FOUNDRY_CI_JOB_GET_CLASS (self)->dup_title (self);
}

/**
 * foundry_ci_job_dup_stage:
 * @self: a [class@Foundry.CiJob]
 *
 * Gets the pipeline stage containing the job.
 *
 * Returns: (transfer full) (nullable): the stage name
 *
 * Since: 1.2
 */
char *
foundry_ci_job_dup_stage (FoundryCiJob *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_JOB (self), NULL);

  return FOUNDRY_CI_JOB_GET_CLASS (self)->dup_stage (self);
}

/**
 * foundry_ci_job_dup_image:
 * @self: a [class@Foundry.CiJob]
 *
 * Gets the execution image requested by the job, if any.
 *
 * Returns: (transfer full) (nullable): the image reference
 *
 * Since: 1.2
 */
char *
foundry_ci_job_dup_image (FoundryCiJob *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_JOB (self), NULL);

  return FOUNDRY_CI_JOB_GET_CLASS (self)->dup_image (self);
}

/**
 * foundry_ci_job_dup_reason:
 * @self: a [class@Foundry.CiJob]
 *
 * Gets the provider's explanation for the job disposition.
 *
 * Returns: (transfer full) (nullable): the disposition reason
 *
 * Since: 1.2
 */
char *
foundry_ci_job_dup_reason (FoundryCiJob *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_JOB (self), NULL);

  return FOUNDRY_CI_JOB_GET_CLASS (self)->dup_reason (self);
}

/**
 * foundry_ci_job_get_disposition:
 * @self: a [class@Foundry.CiJob]
 *
 * Gets how the provider selected or excluded the job.
 *
 * Returns: the job disposition
 *
 * Since: 1.2
 */
FoundryCiJobDisposition
foundry_ci_job_get_disposition (FoundryCiJob *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_JOB (self), 0);

  return FOUNDRY_CI_JOB_GET_CLASS (self)->get_disposition (self);
}

/**
 * foundry_ci_job_get_can_run:
 * @self: a [class@Foundry.CiJob]
 *
 * Checks whether the job can be run locally.
 *
 * Returns: %TRUE if the job can be run
 *
 * Since: 1.2
 */
gboolean
foundry_ci_job_get_can_run (FoundryCiJob *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_JOB (self), FALSE);

  return FOUNDRY_CI_JOB_GET_CLASS (self)->get_can_run (self);
}

/**
 * foundry_ci_job_get_can_shell:
 * @self: a [class@Foundry.CiJob]
 *
 * Checks whether an interactive shell can be opened for the job.
 *
 * Returns: %TRUE if a shell can be opened
 *
 * Since: 1.2
 */
gboolean
foundry_ci_job_get_can_shell (FoundryCiJob *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_JOB (self), FALSE);

  return FOUNDRY_CI_JOB_GET_CLASS (self)->get_can_shell (self);
}

/**
 * foundry_ci_job_dup_pipeline:
 * @self: a [class@Foundry.CiJob]
 *
 * Gets the pipeline containing the job.
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.CiPipeline]
 *
 * Since: 1.2
 */
FoundryCiPipeline *
foundry_ci_job_dup_pipeline (FoundryCiJob *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_JOB (self), NULL);

  return FOUNDRY_CI_JOB_GET_CLASS (self)->dup_pipeline (self);
}

/**
 * foundry_ci_job_dup_provider:
 * @self: a [class@Foundry.CiJob]
 *
 * Gets the provider which created the job.
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.CiProvider]
 *
 * Since: 1.2
 */
FoundryCiProvider *
foundry_ci_job_dup_provider (FoundryCiJob *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_JOB (self), NULL);

  return FOUNDRY_CI_JOB_GET_CLASS (self)->dup_provider (self);
}
