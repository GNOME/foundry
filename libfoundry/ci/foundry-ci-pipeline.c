/* foundry-ci-pipeline.c
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
 * FoundryCiPipeline:
 *
 * Abstract representation of a continuous integration pipeline.
 *
 * CI providers subclass `FoundryCiPipeline` to expose provider-specific
 * pipelines and their jobs. The properties are read-only. Subclasses whose
 * values can change must emit `notify` for the affected property and emit
 * [signal@Gio.ListModel::items-changed] from the model returned by
 * [method@Foundry.CiPipeline.list_jobs].
 *
 * Since: 1.2
 */

enum {
  PROP_0,
  PROP_ID,
  PROP_PROVIDER,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryCiPipeline, foundry_ci_pipeline, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static char *
foundry_ci_pipeline_real_dup_string (FoundryCiPipeline *self)
{
  return NULL;
}

static FoundryCiProvider *
foundry_ci_pipeline_real_dup_provider (FoundryCiPipeline *self)
{
  return NULL;
}

static GListModel *
foundry_ci_pipeline_real_list_jobs (FoundryCiPipeline *self)
{
  return G_LIST_MODEL (g_list_store_new (FOUNDRY_TYPE_CI_JOB));
}

static void
foundry_ci_pipeline_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundryCiPipeline *self = FOUNDRY_CI_PIPELINE (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_take_string (value, foundry_ci_pipeline_dup_id (self));
      break;

    case PROP_PROVIDER:
      g_value_take_object (value, foundry_ci_pipeline_dup_provider (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, foundry_ci_pipeline_dup_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
foundry_ci_pipeline_class_init (FoundryCiPipelineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_ci_pipeline_get_property;

  klass->dup_id = foundry_ci_pipeline_real_dup_string;
  klass->dup_title = foundry_ci_pipeline_real_dup_string;
  klass->dup_provider = foundry_ci_pipeline_real_dup_provider;
  klass->list_jobs = foundry_ci_pipeline_real_list_jobs;

  /**
   * FoundryCiPipeline:id:
   *
   * The provider-specific pipeline identifier.
   *
   * Since: 1.2
   */
  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiPipeline:provider:
   *
   * The provider which created the pipeline.
   *
   * Since: 1.2
   */
  properties[PROP_PROVIDER] =
    g_param_spec_object ("provider", NULL, NULL,
                         FOUNDRY_TYPE_CI_PROVIDER,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiPipeline:title:
   *
   * The user-visible pipeline title.
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
foundry_ci_pipeline_init (FoundryCiPipeline *self)
{
}

/**
 * foundry_ci_pipeline_dup_id:
 * @self: a [class@Foundry.CiPipeline]
 *
 * Gets the provider-specific identifier for the pipeline.
 *
 * Returns: (transfer full) (nullable): the pipeline identifier
 *
 * Since: 1.2
 */
char *
foundry_ci_pipeline_dup_id (FoundryCiPipeline *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_PIPELINE (self), NULL);

  return FOUNDRY_CI_PIPELINE_GET_CLASS (self)->dup_id (self);
}

/**
 * foundry_ci_pipeline_dup_title:
 * @self: a [class@Foundry.CiPipeline]
 *
 * Gets the user-visible title of the pipeline.
 *
 * Returns: (transfer full) (nullable): the pipeline title
 *
 * Since: 1.2
 */
char *
foundry_ci_pipeline_dup_title (FoundryCiPipeline *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_PIPELINE (self), NULL);

  return FOUNDRY_CI_PIPELINE_GET_CLASS (self)->dup_title (self);
}

/**
 * foundry_ci_pipeline_dup_provider:
 * @self: a [class@Foundry.CiPipeline]
 *
 * Gets the provider which created the pipeline.
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.CiProvider]
 *
 * Since: 1.2
 */
FoundryCiProvider *
foundry_ci_pipeline_dup_provider (FoundryCiPipeline *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_PIPELINE (self), NULL);

  return FOUNDRY_CI_PIPELINE_GET_CLASS (self)->dup_provider (self);
}

/**
 * foundry_ci_pipeline_list_jobs:
 * @self: a [class@Foundry.CiPipeline]
 *
 * Lists the jobs known to the pipeline.
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of
 *   [class@Foundry.CiJob]
 *
 * Since: 1.2
 */
GListModel *
foundry_ci_pipeline_list_jobs (FoundryCiPipeline *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_PIPELINE (self), NULL);

  return FOUNDRY_CI_PIPELINE_GET_CLASS (self)->list_jobs (self);
}

/**
 * foundry_ci_pipeline_find_job:
 * @self: a [class@Foundry.CiPipeline]
 * @id: a job identifier
 *
 * Finds a job in the pipeline by identifier.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.CiJob] or rejects with error
 *
 * Since: 1.2
 */
DexFuture *
foundry_ci_pipeline_find_job (FoundryCiPipeline *self,
                              const char        *id)
{
  g_autoptr(GListModel) jobs = NULL;
  guint n_items;

  dex_return_error_if_fail (FOUNDRY_IS_CI_PIPELINE (self));
  dex_return_error_if_fail (id != NULL);

  if (!(jobs = foundry_ci_pipeline_list_jobs (self)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "%s cannot list CI jobs",
                                  G_OBJECT_TYPE_NAME (self));

  n_items = g_list_model_get_n_items (jobs);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryCiJob) job = g_list_model_get_item (jobs, i);
      g_autofree char *job_id = foundry_ci_job_dup_id (job);

      if (g_strcmp0 (job_id, id) == 0)
        return dex_future_new_take_object (g_steal_pointer (&job));
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_FOUND,
                                "No CI job named `%s`",
                                id);
}
