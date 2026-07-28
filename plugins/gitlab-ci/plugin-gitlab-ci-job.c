/* plugin-gitlab-ci-job.c
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

#include "foundry-ci-pipeline.h"
#include "foundry-ci-provider.h"

#include "plugin-gitlab-ci-job-private.h"

G_DEFINE_FINAL_TYPE (PluginGitlabCiJob, plugin_gitlab_ci_job, FOUNDRY_TYPE_CI_JOB)

PluginGitlabCiVariable *
plugin_gitlab_ci_variable_new (const char *name,
                               const char *value,
                               gboolean    secret,
                               gboolean    file)
{
  PluginGitlabCiVariable *self;

  g_return_val_if_fail (name != NULL, NULL);

  self = g_new0 (PluginGitlabCiVariable, 1);
  self->name = g_strdup (name);
  self->value = g_strdup (value != NULL ? value : "");
  self->secret = secret;
  self->file = file;

  return self;
}

void
plugin_gitlab_ci_variable_free (PluginGitlabCiVariable *self)
{
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->value, g_free);
  g_free (self);
}

PluginGitlabCiNeed *
plugin_gitlab_ci_need_new (const char *job,
                           gboolean    artifacts)
{
  PluginGitlabCiNeed *self;

  g_return_val_if_fail (job != NULL, NULL);

  self = g_new0 (PluginGitlabCiNeed, 1);
  self->job = g_strdup (job);
  self->artifacts = artifacts;

  return self;
}

void
plugin_gitlab_ci_need_free (PluginGitlabCiNeed *self)
{
  g_clear_pointer (&self->job, g_free);
  g_free (self);
}

const char *
plugin_gitlab_ci_job_status_to_string (PluginGitlabCiJobStatus status)
{
  switch (status)
    {
    case PLUGIN_GITLAB_CI_JOB_STATUS_SELECTED:
      return "selected";

    case PLUGIN_GITLAB_CI_JOB_STATUS_SKIPPED:
      return "skipped";

    case PLUGIN_GITLAB_CI_JOB_STATUS_MANUAL:
      return "manual";

    case PLUGIN_GITLAB_CI_JOB_STATUS_UNSUPPORTED:
      return "unsupported";

    default:
      return "unknown";
    }
}

static FoundryCiJobDisposition
plugin_gitlab_ci_job_get_disposition (FoundryCiJob *job)
{
  PluginGitlabCiJob *self = PLUGIN_GITLAB_CI_JOB (job);

  switch (self->status)
    {
    case PLUGIN_GITLAB_CI_JOB_STATUS_SELECTED:
      return FOUNDRY_CI_JOB_DISPOSITION_SELECTED;

    case PLUGIN_GITLAB_CI_JOB_STATUS_SKIPPED:
      return FOUNDRY_CI_JOB_DISPOSITION_SKIPPED;

    case PLUGIN_GITLAB_CI_JOB_STATUS_MANUAL:
      return FOUNDRY_CI_JOB_DISPOSITION_MANUAL;

    case PLUGIN_GITLAB_CI_JOB_STATUS_UNSUPPORTED:
      return FOUNDRY_CI_JOB_DISPOSITION_UNSUPPORTED;

    default:
      g_assert_not_reached ();
    }
}

static char *
plugin_gitlab_ci_job_dup_id (FoundryCiJob *job)
{
  return g_strdup (PLUGIN_GITLAB_CI_JOB (job)->name);
}

static char *
plugin_gitlab_ci_job_dup_title (FoundryCiJob *job)
{
  return g_strdup (PLUGIN_GITLAB_CI_JOB (job)->name);
}

static char *
plugin_gitlab_ci_job_dup_stage (FoundryCiJob *job)
{
  return g_strdup (PLUGIN_GITLAB_CI_JOB (job)->stage);
}

static char *
plugin_gitlab_ci_job_dup_image (FoundryCiJob *job)
{
  return g_strdup (PLUGIN_GITLAB_CI_JOB (job)->image);
}

static char *
plugin_gitlab_ci_job_dup_reason (FoundryCiJob *job)
{
  return g_strdup (PLUGIN_GITLAB_CI_JOB (job)->reason);
}

static gboolean
plugin_gitlab_ci_job_get_can_run (FoundryCiJob *job)
{
  return PLUGIN_GITLAB_CI_JOB (job)->status == PLUGIN_GITLAB_CI_JOB_STATUS_SELECTED;
}

static gboolean
plugin_gitlab_ci_job_get_can_shell (FoundryCiJob *job)
{
  return PLUGIN_GITLAB_CI_JOB (job)->status == PLUGIN_GITLAB_CI_JOB_STATUS_SELECTED;
}

static FoundryCiPipeline *
plugin_gitlab_ci_job_dup_pipeline (FoundryCiJob *job)
{
  PluginGitlabCiJob *self = PLUGIN_GITLAB_CI_JOB (job);

  return self->pipeline ? g_object_ref (self->pipeline) : NULL;
}

static FoundryCiProvider *
plugin_gitlab_ci_job_dup_provider (FoundryCiJob *job)
{
  return g_object_ref (PLUGIN_GITLAB_CI_JOB (job)->provider);
}

static void
plugin_gitlab_ci_job_finalize (GObject *object)
{
  PluginGitlabCiJob *self = PLUGIN_GITLAB_CI_JOB (object);

  g_clear_weak_pointer (&self->pipeline);
  g_clear_object (&self->provider);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->stage, g_free);
  g_clear_pointer (&self->image, g_free);
  g_clear_pointer (&self->image_user, g_free);
  g_clear_pointer (&self->before_script, g_ptr_array_unref);
  g_clear_pointer (&self->script, g_ptr_array_unref);
  g_clear_pointer (&self->after_script, g_ptr_array_unref);
  g_clear_pointer (&self->needs, g_ptr_array_unref);
  g_clear_pointer (&self->dependencies, g_ptr_array_unref);
  g_clear_pointer (&self->artifact_paths, g_ptr_array_unref);
  g_clear_pointer (&self->variables, g_hash_table_unref);
  g_clear_pointer (&self->unsupported, g_ptr_array_unref);
  g_clear_pointer (&self->artifacts_when, g_free);
  g_clear_pointer (&self->when, g_free);
  g_clear_pointer (&self->timeout, g_free);
  g_clear_pointer (&self->reason, g_free);

  G_OBJECT_CLASS (plugin_gitlab_ci_job_parent_class)->finalize (object);
}

static void
plugin_gitlab_ci_job_class_init (PluginGitlabCiJobClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryCiJobClass *job_class = FOUNDRY_CI_JOB_CLASS (klass);

  object_class->finalize = plugin_gitlab_ci_job_finalize;
  job_class->dup_id = plugin_gitlab_ci_job_dup_id;
  job_class->dup_title = plugin_gitlab_ci_job_dup_title;
  job_class->dup_stage = plugin_gitlab_ci_job_dup_stage;
  job_class->dup_image = plugin_gitlab_ci_job_dup_image;
  job_class->dup_reason = plugin_gitlab_ci_job_dup_reason;
  job_class->get_disposition = plugin_gitlab_ci_job_get_disposition;
  job_class->get_can_run = plugin_gitlab_ci_job_get_can_run;
  job_class->get_can_shell = plugin_gitlab_ci_job_get_can_shell;
  job_class->dup_pipeline = plugin_gitlab_ci_job_dup_pipeline;
  job_class->dup_provider = plugin_gitlab_ci_job_dup_provider;
}

static void
plugin_gitlab_ci_job_init (PluginGitlabCiJob *self)
{
  self->after_script = g_ptr_array_new_with_free_func (g_free);
  self->artifact_paths = g_ptr_array_new_with_free_func (g_free);
  self->before_script = g_ptr_array_new_with_free_func (g_free);
  self->dependencies = g_ptr_array_new_with_free_func (g_free);
  self->needs = g_ptr_array_new_with_free_func ((GDestroyNotify)plugin_gitlab_ci_need_free);
  self->script = g_ptr_array_new_with_free_func (g_free);
  self->unsupported = g_ptr_array_new_with_free_func (g_free);
  self->variables = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                           (GDestroyNotify)plugin_gitlab_ci_variable_free);
}

PluginGitlabCiJob *
plugin_gitlab_ci_job_new (FoundryCiProvider *provider,
                          FoundryCiPipeline *pipeline)
{
  PluginGitlabCiJob *self;

  g_return_val_if_fail (FOUNDRY_IS_CI_PROVIDER (provider), NULL);
  g_return_val_if_fail (FOUNDRY_IS_CI_PIPELINE (pipeline), NULL);

  self = g_object_new (PLUGIN_TYPE_GITLAB_CI_JOB, NULL);
  g_set_weak_pointer (&self->pipeline, pipeline);
  self->provider = g_object_ref (provider);

  return self;
}
