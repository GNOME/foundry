/* plugin-gitlab-ci-provider.c
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

#include "foundry-ci-artifact.h"
#include "foundry-ci-job.h"
#include "foundry-ci-pipeline.h"
#include "foundry-ci-run.h"
#include "foundry-context.h"

#include "plugin-gitlab-ci-pipeline-private.h"
#include "plugin-gitlab-ci-provider.h"
#include "plugin-gitlab-ci-run-private.h"
#include "plugin-gitlab-ci-compiler-private.h"
#include "plugin-gitlab-ci-config-loader-private.h"
#include "plugin-gitlab-ci-context-private.h"
#include "plugin-gitlab-ci-runner-private.h"

struct _PluginGitlabCiProvider
{
  FoundryCiProvider  parent_instance;
  GFileMonitor      *monitor;
};

static void provider_options_clear (PluginGitlabCiRunOptions *options);

DEX_DEFINE_CLOSURE_TYPE (ListRequest, list_request,
                         DEX_DEFINE_CLOSURE_OBJECT (PluginGitlabCiProvider, provider),
                         DEX_DEFINE_CLOSURE_OBJECT (FoundryContext, context))

DEX_DEFINE_CLOSURE_TYPE (RunRequest, run_request,
                         DEX_DEFINE_CLOSURE_OBJECT (PluginGitlabCiRun, run),
                         DEX_DEFINE_CLOSURE_POINTER (PluginGitlabCiContext *, context, plugin_gitlab_ci_context_unref),
                         DEX_DEFINE_CLOSURE_OBJECT (PluginGitlabCiPipeline, pipeline),
                         DEX_DEFINE_CLOSURE_VALUE_WITH_CLEAR (PluginGitlabCiRunOptions, options, provider_options_clear))

G_DEFINE_FINAL_TYPE (PluginGitlabCiProvider, plugin_gitlab_ci_provider, FOUNDRY_TYPE_CI_PROVIDER)

static void
provider_options_clear (PluginGitlabCiRunOptions *options)
{
  g_assert (options != NULL);

  g_clear_pointer (&options->output_dir, g_free);
  g_clear_pointer (&options->result_output_dir, g_free);
  g_clear_pointer (&options->job_names, g_strfreev);
}

static void
provider_options_init (PluginGitlabCiRunOptions *options)
{
  g_assert (options != NULL);

  options->jobs = MAX (1, g_get_num_processors ());
  options->stdin_fd = -1;
  options->stdout_fd = -1;
  options->stderr_fd = -1;
}

static void
plugin_gitlab_ci_provider_progress_cb (double   progress,
                                       gpointer user_data)
{
  PluginGitlabCiRun *run = user_data;

  g_assert (PLUGIN_IS_GITLAB_CI_RUN (run));

  plugin_gitlab_ci_run_set_progress (run, progress);
}

static DexFuture *
plugin_gitlab_ci_provider_list_pipelines_fiber (gpointer user_data)
{
  ListRequest *request = user_data;
  g_autoptr(PluginGitlabCiContext) context = NULL;
  g_autoptr(PluginGitlabCiConfigLoader) loader = NULL;
  g_autoptr(JsonNode) config = NULL;
  g_autoptr(PluginGitlabCiPipeline) pipeline = NULL;
  g_autoptr(GListStore) pipelines = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (request != NULL);
  g_assert (PLUGIN_IS_GITLAB_CI_PROVIDER (request->provider));

  if (!(context = dex_await_boxed (plugin_gitlab_ci_context_new (request->context), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  loader = plugin_gitlab_ci_config_loader_new (request->context, context, FALSE);
  if (!(config = dex_await_boxed (plugin_gitlab_ci_config_loader_load (loader), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(pipeline = plugin_gitlab_ci_compiler_compile (FOUNDRY_CI_PROVIDER (request->provider), context, config, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  pipelines = g_list_store_new (FOUNDRY_TYPE_CI_PIPELINE);
  g_list_store_append (pipelines, pipeline);

  return dex_future_new_take_object (g_steal_pointer (&pipelines));
}

static DexFuture *
plugin_gitlab_ci_provider_list_pipelines (FoundryCiProvider *provider)
{
  PluginGitlabCiProvider *self = PLUGIN_GITLAB_CI_PROVIDER (provider);
  g_autoptr(FoundryContext) context = NULL;
  ListRequest *request;

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  request = list_request_new ();
  request->provider = g_object_ref (self);
  request->context = g_object_ref (context);

  return dex_scheduler_spawn (NULL,
                              0,
                              plugin_gitlab_ci_provider_list_pipelines_fiber,
                              request,
                              (GDestroyNotify)list_request_free);
}

static void
apply_run_options (PluginGitlabCiRunOptions *destination,
                   FoundryCiRunOptions      *source)
{
  g_assert (destination != NULL);
  g_assert (FOUNDRY_IS_CI_RUN_OPTIONS (source));

  destination->jobs = foundry_ci_run_options_get_max_jobs (source);
  destination->fail_fast = foundry_ci_run_options_get_fail_fast (source);
  destination->offline = foundry_ci_run_options_get_offline (source);
  destination->save_state = foundry_ci_run_options_get_save_state (source);
  destination->save_workspace = foundry_ci_run_options_get_save_workspace (source);
  destination->stdin_fd = foundry_ci_run_options_get_stdin_fd (source);
  destination->stdout_fd = foundry_ci_run_options_get_stdout_fd (source);
  destination->stderr_fd = foundry_ci_run_options_get_stderr_fd (source);
  destination->output_dir = foundry_ci_run_options_dup_output_dir (source);
}

static DexFuture *
plugin_gitlab_ci_provider_run_finish_fiber (gpointer user_data)
{
  RunRequest *request = user_data;
  g_autoptr(DexCancellable) cancellable = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) output = NULL;
  g_autoptr(GFile) artifacts_dir = NULL;
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(FoundryCiArtifact) artifact = NULL;
  const char *attributes = (G_FILE_ATTRIBUTE_STANDARD_NAME ","
                            G_FILE_ATTRIBUTE_STANDARD_TYPE);
  GFileQueryInfoFlags flags = G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS;
  int exit_status;

  g_assert (request != NULL);
  g_assert (PLUGIN_IS_GITLAB_CI_RUN (request->run));

  plugin_gitlab_ci_run_set_state (request->run, FOUNDRY_CI_RUN_STATE_RUNNING);
  cancellable = plugin_gitlab_ci_run_dup_cancellable (request->run);
  exit_status = dex_await_int (plugin_gitlab_ci_runner_run (request->context, request->pipeline, &request->options, cancellable), &error);

  if (error != NULL)
    {
      plugin_gitlab_ci_run_fail (request->run, g_steal_pointer (&error));
      return dex_future_new_false ();
    }

  if (request->options.result_output_dir != NULL)
    {
      output = g_file_new_for_path (request->options.result_output_dir);
      plugin_gitlab_ci_run_set_output_dir (request->run, request->options.result_output_dir);
      artifact = foundry_ci_artifact_new ("Output Bundle", output, FOUNDRY_CI_ARTIFACT_KIND_DIRECTORY);
      plugin_gitlab_ci_run_add_artifact (request->run, artifact);
      g_clear_object (&artifact);

      artifacts_dir = g_file_get_child (output, "artifacts");
      enumerator = dex_await_object (dex_file_enumerate_children (artifacts_dir,
                                                                  attributes,
                                                                  flags,
                                                                  G_PRIORITY_DEFAULT),
                                     &error);

      if (enumerator != NULL)
        {
          gpointer infos_ptr;

          while ((infos_ptr = dex_await_boxed (dex_file_enumerator_next_files (enumerator, 100, G_PRIORITY_DEFAULT), &error)))
            {
              g_autolist(GFileInfo) infos = infos_ptr;

              for (const GList *iter = infos; iter; iter = iter->next)
                {
                  GFileInfo *info = iter->data;
                  const char *name = g_file_info_get_name (info);
                  g_autoptr(GFile) file = NULL;

                  if (name == NULL)
                    continue;

                  file = g_file_enumerator_get_child (enumerator, info);
                  artifact = foundry_ci_artifact_new (name, file,
                                                      g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY
                                                        ? FOUNDRY_CI_ARTIFACT_KIND_DIRECTORY
                                                        : FOUNDRY_CI_ARTIFACT_KIND_FILE);
                  plugin_gitlab_ci_run_add_artifact (request->run, artifact);
                  g_clear_object (&artifact);
                }
            }
        }

      if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        g_debug ("Failed to enumerate CI artifacts: %s", error->message);

      g_clear_error (&error);
    }

  plugin_gitlab_ci_run_complete (request->run, exit_status);

  return dex_future_new_for_int (exit_status);
}

static DexFuture *
plugin_gitlab_ci_provider_run (FoundryCiProvider   *provider,
                               FoundryCiPipeline   *pipeline,
                               const char * const  *job_ids,
                               FoundryCiRunOptions *run_options)
{
  PluginGitlabCiProvider *self = PLUGIN_GITLAB_CI_PROVIDER (provider);
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(PluginGitlabCiRun) run = NULL;
  g_autoptr(GFile) default_output = NULL;
  RunRequest *request;

  g_assert (PLUGIN_IS_GITLAB_CI_PIPELINE (pipeline));
  g_assert (FOUNDRY_IS_CI_RUN_OPTIONS (run_options));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  run = plugin_gitlab_ci_run_new (context);
  request = run_request_new ();
  request->run = g_object_ref (run);
  request->context = plugin_gitlab_ci_pipeline_dup_context (PLUGIN_GITLAB_CI_PIPELINE (pipeline));
  request->pipeline = g_object_ref (PLUGIN_GITLAB_CI_PIPELINE (pipeline));
  provider_options_init (&request->options);
  request->options.job_names = g_strdupv ((char **)job_ids);
  request->options.progress_func = plugin_gitlab_ci_provider_progress_cb;
  request->options.progress_data = run;
  apply_run_options (&request->options, run_options);

  if (request->options.output_dir == NULL)
    {
      default_output = foundry_context_cache_file (context, "ci-output", NULL);
      request->options.output_dir = g_file_get_path (default_output);
    }

  if (request->options.output_dir == NULL)
    {
      run_request_free (request);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_NOT_SUPPORTED,
                                    "GitLab CI requires a local output directory");
    }

  plugin_gitlab_ci_run_set_output_dir (run, request->options.output_dir);
  plugin_gitlab_ci_run_set_state (run, FOUNDRY_CI_RUN_STATE_PREPARING);

  dex_future_disown (dex_scheduler_spawn (NULL,
                                          0,
                                          plugin_gitlab_ci_provider_run_finish_fiber,
                                          request,
                                          (GDestroyNotify)run_request_free));

  return dex_future_new_take_object (g_steal_pointer (&run));
}

static DexFuture *
plugin_gitlab_ci_provider_run_shell (FoundryCiProvider   *provider,
                                     FoundryCiJob        *job,
                                     FoundryCiRunOptions *run_options)
{
  PluginGitlabCiProvider *self = PLUGIN_GITLAB_CI_PROVIDER (provider);
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryCiPipeline) pipeline = NULL;
  g_autoptr(PluginGitlabCiRun) run = NULL;
  g_autofree char *job_id = NULL;
  RunRequest *request;

  g_assert (FOUNDRY_IS_CI_JOB (job));
  g_assert (FOUNDRY_IS_CI_RUN_OPTIONS (run_options));

  if (!foundry_ci_job_get_can_shell (job))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "This CI job cannot be opened in a shell");

  pipeline = foundry_ci_job_dup_pipeline (job);
  if (!PLUGIN_IS_GITLAB_CI_PIPELINE (pipeline))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "CI job does not belong to the GitLab provider");

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  job_id = foundry_ci_job_dup_id (job);
  run = plugin_gitlab_ci_run_new (context);
  request = run_request_new ();
  request->run = g_object_ref (run);
  request->context = plugin_gitlab_ci_pipeline_dup_context (PLUGIN_GITLAB_CI_PIPELINE (pipeline));
  request->pipeline = g_object_ref (PLUGIN_GITLAB_CI_PIPELINE (pipeline));
  provider_options_init (&request->options);
  request->options.shell = TRUE;
  request->options.job_names = g_new0 (char *, 2);
  request->options.job_names[0] = g_strdup (job_id);
  request->options.progress_func = plugin_gitlab_ci_provider_progress_cb;
  request->options.progress_data = run;
  apply_run_options (&request->options, run_options);
  g_clear_pointer (&request->options.output_dir, g_free);
  plugin_gitlab_ci_run_set_state (run, FOUNDRY_CI_RUN_STATE_PREPARING);

  dex_future_disown (dex_scheduler_spawn (NULL,
                                          0,
                                          plugin_gitlab_ci_provider_run_finish_fiber,
                                          request,
                                          (GDestroyNotify)run_request_free));

  return dex_future_new_take_object (g_steal_pointer (&run));
}

static void
plugin_gitlab_ci_provider_changed_cb (PluginGitlabCiProvider *self,
                                      GFile                  *file,
                                      GFile                  *other_file,
                                      GFileMonitorEvent       event,
                                      GFileMonitor           *monitor)
{
  g_assert (PLUGIN_IS_GITLAB_CI_PROVIDER (self));

  foundry_ci_provider_invalidate (FOUNDRY_CI_PROVIDER (self));
}

static DexFuture *
plugin_gitlab_ci_provider_load (FoundryCiProvider *provider)
{
  PluginGitlabCiProvider *self = PLUGIN_GITLAB_CI_PROVIDER (provider);
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GFile) project_directory = NULL;
  g_autoptr(GFile) config_file = NULL;

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  project_directory = foundry_context_dup_project_directory (context);
  config_file = g_file_get_child (project_directory, ".gitlab-ci.yml");
  self->monitor = g_file_monitor_file (config_file, G_FILE_MONITOR_NONE, NULL, NULL);

  if (self->monitor != NULL)
    g_signal_connect_object (self->monitor,
                             "changed",
                             G_CALLBACK (plugin_gitlab_ci_provider_changed_cb),
                             self,
                             G_CONNECT_SWAPPED);

  return dex_future_new_true ();
}

static DexFuture *
plugin_gitlab_ci_provider_unload (FoundryCiProvider *provider)
{
  PluginGitlabCiProvider *self = PLUGIN_GITLAB_CI_PROVIDER (provider);

  if (self->monitor != NULL)
    g_file_monitor_cancel (self->monitor);

  g_clear_object (&self->monitor);

  return dex_future_new_true ();
}

static void
plugin_gitlab_ci_provider_finalize (GObject *object)
{
  PluginGitlabCiProvider *self = PLUGIN_GITLAB_CI_PROVIDER (object);

  g_clear_object (&self->monitor);

  G_OBJECT_CLASS (plugin_gitlab_ci_provider_parent_class)->finalize (object);
}

static void
plugin_gitlab_ci_provider_class_init (PluginGitlabCiProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryCiProviderClass *provider_class = FOUNDRY_CI_PROVIDER_CLASS (klass);

  object_class->finalize = plugin_gitlab_ci_provider_finalize;
  provider_class->load = plugin_gitlab_ci_provider_load;
  provider_class->unload = plugin_gitlab_ci_provider_unload;
  provider_class->list_pipelines = plugin_gitlab_ci_provider_list_pipelines;
  provider_class->run = plugin_gitlab_ci_provider_run;
  provider_class->run_shell = plugin_gitlab_ci_provider_run_shell;
}

static void
plugin_gitlab_ci_provider_init (PluginGitlabCiProvider *self)
{
}
