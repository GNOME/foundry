/* plugin-gitlab-ci-pipeline.c
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

#include "foundry-ci-provider.h"

#include "plugin-gitlab-ci-error-private.h"
#include "plugin-gitlab-ci-pipeline-private.h"

G_DEFINE_FINAL_TYPE (PluginGitlabCiPipeline, plugin_gitlab_ci_pipeline, FOUNDRY_TYPE_CI_PIPELINE)

static char *
plugin_gitlab_ci_pipeline_dup_id (FoundryCiPipeline *pipeline)
{
  return g_strdup (PLUGIN_GITLAB_CI_PIPELINE (pipeline)->digest);
}

static char *
plugin_gitlab_ci_pipeline_dup_title (FoundryCiPipeline *pipeline)
{
  return g_strdup (".gitlab-ci.yml");
}

static FoundryCiProvider *
plugin_gitlab_ci_pipeline_dup_provider (FoundryCiPipeline *pipeline)
{
  return g_object_ref (PLUGIN_GITLAB_CI_PIPELINE (pipeline)->provider);
}

static GListModel *
plugin_gitlab_ci_pipeline_list_jobs (FoundryCiPipeline *pipeline)
{
  PluginGitlabCiPipeline *self = PLUGIN_GITLAB_CI_PIPELINE (pipeline);
  GListStore *jobs;

  jobs = g_list_store_new (FOUNDRY_TYPE_CI_JOB);
  for (guint i = 0; i < self->jobs->len; i++)
    g_list_store_append (jobs, g_ptr_array_index (self->jobs, i));

  return G_LIST_MODEL (jobs);
}

static void
plugin_gitlab_ci_pipeline_finalize (GObject *object)
{
  PluginGitlabCiPipeline *self = PLUGIN_GITLAB_CI_PIPELINE (object);

  g_clear_object (&self->provider);
  g_clear_pointer (&self->context, plugin_gitlab_ci_context_unref);
  g_clear_pointer (&self->stages, g_ptr_array_unref);
  g_clear_pointer (&self->jobs_by_name, g_hash_table_unref);
  g_clear_pointer (&self->jobs, g_ptr_array_unref);
  g_clear_pointer (&self->digest, g_free);
  g_clear_pointer (&self->workflow_reason, g_free);

  G_OBJECT_CLASS (plugin_gitlab_ci_pipeline_parent_class)->finalize (object);
}

static void
plugin_gitlab_ci_pipeline_class_init (PluginGitlabCiPipelineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryCiPipelineClass *pipeline_class = FOUNDRY_CI_PIPELINE_CLASS (klass);

  object_class->finalize = plugin_gitlab_ci_pipeline_finalize;
  pipeline_class->dup_id = plugin_gitlab_ci_pipeline_dup_id;
  pipeline_class->dup_title = plugin_gitlab_ci_pipeline_dup_title;
  pipeline_class->dup_provider = plugin_gitlab_ci_pipeline_dup_provider;
  pipeline_class->list_jobs = plugin_gitlab_ci_pipeline_list_jobs;
}

static void
plugin_gitlab_ci_pipeline_init (PluginGitlabCiPipeline *self)
{
  self->stages = g_ptr_array_new_with_free_func (g_free);
  self->jobs = g_ptr_array_new_with_free_func (g_object_unref);
  self->jobs_by_name = g_hash_table_new (g_str_hash, g_str_equal);
  self->workflow_selected = TRUE;
}

PluginGitlabCiPipeline *
plugin_gitlab_ci_pipeline_new (FoundryCiProvider     *provider,
                               PluginGitlabCiContext *context)
{
  PluginGitlabCiPipeline *self;

  g_return_val_if_fail (FOUNDRY_IS_CI_PROVIDER (provider), NULL);
  g_return_val_if_fail (context != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_GITLAB_CI_PIPELINE, NULL);
  self->provider = g_object_ref (provider);
  self->context = plugin_gitlab_ci_context_ref (context);

  return self;
}

PluginGitlabCiContext *
plugin_gitlab_ci_pipeline_dup_context (PluginGitlabCiPipeline *self)
{
  g_return_val_if_fail (PLUGIN_IS_GITLAB_CI_PIPELINE (self), NULL);

  return plugin_gitlab_ci_context_ref (self->context);
}

void
plugin_gitlab_ci_pipeline_add_job (PluginGitlabCiPipeline *self,
                                   PluginGitlabCiJob      *job)
{
  g_return_if_fail (PLUGIN_IS_GITLAB_CI_PIPELINE (self));
  g_return_if_fail (PLUGIN_IS_GITLAB_CI_JOB (job));
  g_return_if_fail (job->name != NULL);
  g_return_if_fail (!g_hash_table_contains (self->jobs_by_name, job->name));

  g_ptr_array_add (self->jobs, g_object_ref (job));
  g_hash_table_insert (self->jobs_by_name, job->name, job);
}

PluginGitlabCiJob *
plugin_gitlab_ci_pipeline_lookup_job (PluginGitlabCiPipeline *self,
                                      const char             *name)
{
  g_return_val_if_fail (PLUGIN_IS_GITLAB_CI_PIPELINE (self), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_hash_table_lookup (self->jobs_by_name, name);
}

static int
stage_index (PluginGitlabCiPipeline *self,
             const char             *stage)
{
  g_assert (PLUGIN_IS_GITLAB_CI_PIPELINE (self));
  g_assert (stage != NULL);

  for (guint i = 0; i < self->stages->len; i++)
    {
      if (g_str_equal (g_ptr_array_index (self->stages, i), stage))
        return i;
    }

  return -1;
}

static gboolean
visit_job (PluginGitlabCiPipeline  *self,
           PluginGitlabCiJob       *job,
           GHashTable              *visiting,
           GHashTable              *visited,
           GError                 **error)
{
  g_assert (PLUGIN_IS_GITLAB_CI_PIPELINE (self));
  g_assert (PLUGIN_IS_GITLAB_CI_JOB (job));
  g_assert (visiting != NULL);
  g_assert (visited != NULL);

  if (g_hash_table_contains (visited, job->name))
    return TRUE;

  if (g_hash_table_contains (visiting, job->name))
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                   "dependency cycle contains job '%s'",
                   job->name);
      return FALSE;
    }

  g_hash_table_add (visiting, job->name);
  for (guint i = 0; i < job->needs->len; i++)
    {
      PluginGitlabCiNeed *need = g_ptr_array_index (job->needs, i);
      PluginGitlabCiJob *dependency = plugin_gitlab_ci_pipeline_lookup_job (self, need->job);

      if (dependency == NULL)
        {
          g_set_error (error,
                       PLUGIN_GITLAB_CI_ERROR,
                       PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                       "job '%s' needs unknown job '%s'",
                       job->name,
                       need->job);
          return FALSE;
        }

      if (stage_index (self, dependency->stage) > stage_index (self, job->stage))
        {
          g_set_error (error,
                       PLUGIN_GITLAB_CI_ERROR,
                       PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                       "job '%s' needs later-stage job '%s'",
                       job->name,
                       need->job);
          return FALSE;
        }

      if (!visit_job (self, dependency, visiting, visited, error))
        return FALSE;
    }

  g_hash_table_remove (visiting, job->name);
  g_hash_table_add (visited, job->name);

  return TRUE;
}

gboolean
plugin_gitlab_ci_pipeline_validate (PluginGitlabCiPipeline  *self,
                                    GError                 **error)
{
  g_autoptr(GHashTable) visiting = NULL;
  g_autoptr(GHashTable) visited = NULL;

  g_return_val_if_fail (PLUGIN_IS_GITLAB_CI_PIPELINE (self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  visiting = g_hash_table_new (g_str_hash, g_str_equal);
  visited = g_hash_table_new (g_str_hash, g_str_equal);

  for (guint i = 0; i < self->jobs->len; i++)
    {
      PluginGitlabCiJob *job = g_ptr_array_index (self->jobs, i);

      if (stage_index (self, job->stage) < 0)
        {
          g_set_error (error,
                       PLUGIN_GITLAB_CI_ERROR,
                       PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                       "job '%s' uses unknown stage '%s'",
                       job->name,
                       job->stage);
          return FALSE;
        }

      if (!visit_job (self, job, visiting, visited, error))
        return FALSE;
    }

  return TRUE;
}
