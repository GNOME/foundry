/* test-ci.c
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

#include <foundry.h>

#include "test-util.h"

typedef struct _TestCiProvider      TestCiProvider;
typedef struct _TestCiProviderClass TestCiProviderClass;
typedef struct _TestCiPipeline      TestCiPipeline;
typedef struct _TestCiPipelineClass TestCiPipelineClass;
typedef struct _TestCiJob           TestCiJob;
typedef struct _TestCiJobClass      TestCiJobClass;
typedef struct _TestCiRun           TestCiRun;
typedef struct _TestCiRunClass      TestCiRunClass;

struct _TestCiProvider
{
  FoundryCiProvider parent_instance;
};

struct _TestCiProviderClass
{
  FoundryCiProviderClass parent_class;
};

struct _TestCiPipeline
{
  FoundryCiPipeline  parent_instance;
  FoundryCiProvider *provider;
  GListStore        *jobs;
};

struct _TestCiPipelineClass
{
  FoundryCiPipelineClass parent_class;
};

struct _TestCiJob
{
  FoundryCiJob      parent_instance;
  FoundryCiProvider *provider;
  FoundryCiPipeline *pipeline;
};

struct _TestCiJobClass
{
  FoundryCiJobClass parent_class;
};

struct _TestCiRun
{
  FoundryCiRun      parent_instance;
  DexPromise       *completion;
  GListStore       *artifacts;
  FoundryCiRunState state;
  double            progress;
  gboolean          cancelled;
};

struct _TestCiRunClass
{
  FoundryCiRunClass parent_class;
};

GType test_ci_provider_get_type (void);
GType test_ci_pipeline_get_type (void);
GType test_ci_job_get_type (void);
GType test_ci_run_get_type (void);

G_DEFINE_TYPE (TestCiProvider, test_ci_provider, FOUNDRY_TYPE_CI_PROVIDER)
G_DEFINE_TYPE (TestCiPipeline, test_ci_pipeline, FOUNDRY_TYPE_CI_PIPELINE)
G_DEFINE_TYPE (TestCiJob, test_ci_job, FOUNDRY_TYPE_CI_JOB)
G_DEFINE_TYPE (TestCiRun, test_ci_run, FOUNDRY_TYPE_CI_RUN)

static DexFuture *
test_ci_provider_run (FoundryCiProvider   *provider,
                      FoundryCiPipeline   *pipeline,
                      const char * const  *job_ids,
                      FoundryCiRunOptions *options)
{
  g_autoptr(FoundryContext) context = NULL;

  g_assert (FOUNDRY_IS_CI_PROVIDER (provider));
  g_assert (FOUNDRY_IS_CI_PIPELINE (pipeline));
  g_assert (FOUNDRY_IS_CI_RUN_OPTIONS (options));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (provider));

  return dex_future_new_take_object (
    g_object_new (test_ci_run_get_type (),
                  "context", context,
                  NULL));
}

static DexFuture *
test_ci_provider_run_shell (FoundryCiProvider   *provider,
                            FoundryCiJob        *job,
                            FoundryCiRunOptions *options)
{
  g_autoptr(FoundryContext) context = NULL;

  g_assert (FOUNDRY_IS_CI_PROVIDER (provider));
  g_assert (FOUNDRY_IS_CI_JOB (job));
  g_assert (FOUNDRY_IS_CI_RUN_OPTIONS (options));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (provider));

  return dex_future_new_take_object (
    g_object_new (test_ci_run_get_type (),
                  "context", context,
                  NULL));
}

static void
test_ci_job_pipeline_destroyed_cb (gpointer data,
                                   GObject *where_pipeline_was)
{
  TestCiJob *self = data;

  self->pipeline = NULL;
  g_object_notify (G_OBJECT (self), "pipeline");
  g_object_unref (self);
}

static void
test_ci_provider_class_init (TestCiProviderClass *klass)
{
  FoundryCiProviderClass *provider_class = FOUNDRY_CI_PROVIDER_CLASS (klass);

  provider_class->run = test_ci_provider_run;
  provider_class->run_shell = test_ci_provider_run_shell;
}

static void
test_ci_provider_init (TestCiProvider *self)
{
}

static char *
test_ci_pipeline_dup_id (FoundryCiPipeline *pipeline)
{
  return g_strdup ("pipeline");
}

static char *
test_ci_pipeline_dup_title (FoundryCiPipeline *pipeline)
{
  return g_strdup ("Pipeline");
}

static FoundryCiProvider *
test_ci_pipeline_dup_provider (FoundryCiPipeline *pipeline)
{
  return g_object_ref (((TestCiPipeline *)pipeline)->provider);
}

static GListModel *
test_ci_pipeline_list_jobs (FoundryCiPipeline *pipeline)
{
  return G_LIST_MODEL (g_object_ref (((TestCiPipeline *)pipeline)->jobs));
}

static void
test_ci_pipeline_finalize (GObject *object)
{
  TestCiPipeline *self = (TestCiPipeline *)object;

  g_clear_object (&self->provider);
  g_clear_object (&self->jobs);

  G_OBJECT_CLASS (test_ci_pipeline_parent_class)->finalize (object);
}

static void
test_ci_pipeline_class_init (TestCiPipelineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryCiPipelineClass *pipeline_class = FOUNDRY_CI_PIPELINE_CLASS (klass);

  object_class->finalize = test_ci_pipeline_finalize;
  pipeline_class->dup_id = test_ci_pipeline_dup_id;
  pipeline_class->dup_title = test_ci_pipeline_dup_title;
  pipeline_class->dup_provider = test_ci_pipeline_dup_provider;
  pipeline_class->list_jobs = test_ci_pipeline_list_jobs;
}

static void
test_ci_pipeline_init (TestCiPipeline *self)
{
  self->jobs = g_list_store_new (FOUNDRY_TYPE_CI_JOB);
}

static char *
test_ci_job_dup_id (FoundryCiJob *job)
{
  return g_strdup ("build");
}

static char *
test_ci_job_dup_title (FoundryCiJob *job)
{
  return g_strdup ("Build");
}

static char *
test_ci_job_dup_stage (FoundryCiJob *job)
{
  return g_strdup ("build");
}

static char *
test_ci_job_dup_image (FoundryCiJob *job)
{
  return g_strdup ("example/image");
}

static char *
test_ci_job_dup_reason (FoundryCiJob *job)
{
  return NULL;
}

static FoundryCiJobDisposition
test_ci_job_get_disposition (FoundryCiJob *job)
{
  return FOUNDRY_CI_JOB_DISPOSITION_SELECTED;
}

static gboolean
test_ci_job_get_can_run (FoundryCiJob *job)
{
  return TRUE;
}

static gboolean
test_ci_job_get_can_shell (FoundryCiJob *job)
{
  return TRUE;
}

static FoundryCiPipeline *
test_ci_job_dup_pipeline (FoundryCiJob *job)
{
  TestCiJob *self = (TestCiJob *)job;

  return self->pipeline != NULL ? g_object_ref (self->pipeline) : NULL;
}

static FoundryCiProvider *
test_ci_job_dup_provider (FoundryCiJob *job)
{
  return g_object_ref (((TestCiJob *)job)->provider);
}

static void
test_ci_job_finalize (GObject *object)
{
  TestCiJob *self = (TestCiJob *)object;

  g_clear_object (&self->provider);

  G_OBJECT_CLASS (test_ci_job_parent_class)->finalize (object);
}

static void
test_ci_job_class_init (TestCiJobClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryCiJobClass *job_class = FOUNDRY_CI_JOB_CLASS (klass);

  object_class->finalize = test_ci_job_finalize;
  job_class->dup_id = test_ci_job_dup_id;
  job_class->dup_title = test_ci_job_dup_title;
  job_class->dup_stage = test_ci_job_dup_stage;
  job_class->dup_image = test_ci_job_dup_image;
  job_class->dup_reason = test_ci_job_dup_reason;
  job_class->get_disposition = test_ci_job_get_disposition;
  job_class->get_can_run = test_ci_job_get_can_run;
  job_class->get_can_shell = test_ci_job_get_can_shell;
  job_class->dup_pipeline = test_ci_job_dup_pipeline;
  job_class->dup_provider = test_ci_job_dup_provider;
}

static void
test_ci_job_init (TestCiJob *self)
{
}

static DexFuture *
test_ci_run_await (FoundryCiRun *run)
{
  return dex_ref (((TestCiRun *)run)->completion);
}

static void
test_ci_run_cancel (FoundryCiRun *run)
{
  TestCiRun *self = (TestCiRun *)run;

  self->cancelled = TRUE;

  if (dex_future_is_pending (DEX_FUTURE (self->completion)))
    {
      self->state = FOUNDRY_CI_RUN_STATE_CANCELLED;
      g_object_notify (G_OBJECT (self), "state");
      dex_promise_reject (self->completion,
                          g_error_new_literal (G_IO_ERROR,
                                               G_IO_ERROR_CANCELLED,
                                               "CI run cancelled"));
    }
}

static FoundryCiRunState
test_ci_run_get_state (FoundryCiRun *run)
{
  return ((TestCiRun *)run)->state;
}

static double
test_ci_run_get_progress (FoundryCiRun *run)
{
  return ((TestCiRun *)run)->progress;
}

static int
test_ci_run_get_exit_status (FoundryCiRun *run)
{
  return -1;
}

static char *
test_ci_run_dup_output_dir (FoundryCiRun *run)
{
  return g_strdup ("/tmp/ci-output");
}

static GListModel *
test_ci_run_list_artifacts (FoundryCiRun *run)
{
  return G_LIST_MODEL (g_object_ref (((TestCiRun *)run)->artifacts));
}

static void
test_ci_run_finalize (GObject *object)
{
  TestCiRun *self = (TestCiRun *)object;

  dex_clear (&self->completion);
  g_clear_object (&self->artifacts);

  G_OBJECT_CLASS (test_ci_run_parent_class)->finalize (object);
}

static void
test_ci_run_class_init (TestCiRunClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryCiRunClass *run_class = FOUNDRY_CI_RUN_CLASS (klass);

  object_class->finalize = test_ci_run_finalize;
  run_class->await = test_ci_run_await;
  run_class->cancel = test_ci_run_cancel;
  run_class->get_state = test_ci_run_get_state;
  run_class->get_progress = test_ci_run_get_progress;
  run_class->get_exit_status = test_ci_run_get_exit_status;
  run_class->dup_output_dir = test_ci_run_dup_output_dir;
  run_class->list_artifacts = test_ci_run_list_artifacts;
}

static void
test_ci_run_init (TestCiRun *self)
{
  self->completion = dex_promise_new ();
  self->artifacts = g_list_store_new (FOUNDRY_TYPE_CI_ARTIFACT);
  self->state = FOUNDRY_CI_RUN_STATE_RUNNING;
  self->progress = 0.5;
}

static void
test_ci_run_set_progress (TestCiRun *self,
                          double     progress)
{
  g_assert (self != NULL);

  if (self->progress != progress)
    {
      self->progress = progress;
      g_object_notify (G_OBJECT (self), "progress");
    }
}

static void
test_ci_run_complete (TestCiRun *self)
{
  g_assert (self != NULL);

  if (dex_future_is_pending (DEX_FUTURE (self->completion)))
    {
      self->state = FOUNDRY_CI_RUN_STATE_PASSED;
      g_object_notify (G_OBJECT (self), "state");
      dex_promise_resolve_int (self->completion, 0);
    }
}

static FoundryCiPipeline *
test_ci_pipeline_new (FoundryCiProvider *provider)
{
  g_autoptr(FoundryCiJob) job = NULL;
  TestCiPipeline *pipeline;
  TestCiJob *test_job;

  pipeline = g_object_new (test_ci_pipeline_get_type (), NULL);
  pipeline->provider = g_object_ref (provider);

  test_job = g_object_new (test_ci_job_get_type (), NULL);
  test_job->pipeline = FOUNDRY_CI_PIPELINE (pipeline);
  test_job->provider = g_object_ref (provider);
  g_object_weak_ref (G_OBJECT (pipeline),
                     test_ci_job_pipeline_destroyed_cb,
                     g_object_ref (test_job));
  job = FOUNDRY_CI_JOB (test_job);
  g_list_store_append (pipeline->jobs, job);

  return FOUNDRY_CI_PIPELINE (pipeline);
}

static void
pipeline_notify_cb (FoundryCiJob *job,
                    GParamSpec   *pspec,
                    guint        *count)
{
  (*count)++;
}

static void
test_ci_pipeline_lifetime (void)
{
  g_autoptr(FoundryCiProvider) provider = NULL;
  g_autoptr(FoundryCiPipeline) pipeline = NULL;
  g_autoptr(FoundryCiPipeline) parent = NULL;
  g_autoptr(FoundryCiProvider) job_provider = NULL;
  g_autoptr(FoundryCiJob) job = NULL;
  g_autoptr(GListModel) jobs = NULL;
  g_autofree char *id = NULL;
  g_autofree char *title = NULL;
  g_autofree char *stage = NULL;
  FoundryCiPipeline *weak_pipeline;
  gboolean can_run = FALSE;
  guint pipeline_notifies = 0;

  provider = g_object_new (test_ci_provider_get_type (), NULL);
  pipeline = test_ci_pipeline_new (provider);

  jobs = foundry_ci_pipeline_list_jobs (pipeline);
  g_assert_cmpuint (g_list_model_get_n_items (jobs), ==, 1);
  job = g_list_model_get_item (jobs, 0);
  g_signal_connect (job,
                    "notify::pipeline",
                    G_CALLBACK (pipeline_notify_cb),
                    &pipeline_notifies);
  g_object_get (job,
                "id", &id,
                "title", &title,
                "stage", &stage,
                "can-run", &can_run,
                NULL);
  g_assert_cmpstr (id, ==, "build");
  g_assert_cmpstr (title, ==, "Build");
  g_assert_cmpstr (stage, ==, "build");
  g_assert_true (can_run);

  parent = foundry_ci_job_dup_pipeline (job);
  g_assert_true (parent == pipeline);
  job_provider = foundry_ci_job_dup_provider (job);
  g_assert_true (job_provider == provider);
  g_clear_object (&parent);
  g_clear_object (&jobs);

  weak_pipeline = pipeline;
  g_object_add_weak_pointer (G_OBJECT (pipeline), (gpointer *)&weak_pipeline);
  g_clear_object (&pipeline);

  g_assert_null (weak_pipeline);
  g_assert_cmpuint (pipeline_notifies, ==, 1);
  parent = foundry_ci_job_dup_pipeline (job);
  g_assert_null (parent);
}

static void
test_ci_run_options (void)
{
  g_autoptr(FoundryCiRunOptions) options = NULL;
  g_autofree char *output_dir = NULL;

  options = foundry_ci_run_options_new ();
  foundry_ci_run_options_set_fail_fast (options, TRUE);
  foundry_ci_run_options_set_max_jobs (options, 3);
  foundry_ci_run_options_set_offline (options, TRUE);
  foundry_ci_run_options_set_output_dir (options, "/tmp/ci-output");
  foundry_ci_run_options_set_save_workspace (options, TRUE);
  foundry_ci_run_options_set_fds (options, 3, 4, 5);

  g_assert_true (foundry_ci_run_options_get_fail_fast (options));
  g_assert_cmpuint (foundry_ci_run_options_get_max_jobs (options), ==, 3);
  g_assert_true (foundry_ci_run_options_get_offline (options));
  g_assert_true (foundry_ci_run_options_get_save_state (options));
  g_assert_true (foundry_ci_run_options_get_save_workspace (options));
  g_assert_cmpint (foundry_ci_run_options_get_stdin_fd (options), ==, 3);
  g_assert_cmpint (foundry_ci_run_options_get_stdout_fd (options), ==, 4);
  g_assert_cmpint (foundry_ci_run_options_get_stderr_fd (options), ==, 5);

  output_dir = foundry_ci_run_options_dup_output_dir (options);
  g_assert_cmpstr (output_dir, ==, "/tmp/ci-output");
}

static void
wait_for_n_operations (GListModel *operations,
                       guint       expected)
{
  gint64 deadline;

  g_assert (G_IS_LIST_MODEL (operations));

  deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND * 5;

  while (g_list_model_get_n_items (operations) != expected)
    {
      g_assert_cmpint (g_get_monotonic_time (), <, deadline);
      dex_await (dex_timeout_new_msec (1), NULL);
    }
}

static void
test_ci_provider_run_operation_fiber (void)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryCiProvider) provider = NULL;
  g_autoptr(FoundryCiPipeline) pipeline = NULL;
  g_autoptr(FoundryCiRunOptions) options = NULL;
  g_autoptr(FoundryCiRun) run = NULL;
  g_autoptr(FoundryCiRun) shell_run = NULL;
  g_autoptr(FoundryCiJob) job = NULL;
  g_autoptr(FoundryOperationManager) operation_manager = NULL;
  g_autoptr(FoundryOperation) operation = NULL;
  g_autoptr(GListModel) jobs = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *title = NULL;
  g_autofree char *subtitle = NULL;

  g_assert_true (dex_await (foundry_init (), &error));
  g_assert_no_error (error);

  context = g_object_new (FOUNDRY_TYPE_CONTEXT, NULL);
  provider = g_object_new (test_ci_provider_get_type (),
                           "context", context,
                           NULL);
  pipeline = test_ci_pipeline_new (provider);
  options = foundry_ci_run_options_new ();
  operation_manager = foundry_context_dup_operation_manager (context);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (operation_manager)), ==, 0);

  run = dex_await_object (foundry_ci_provider_run (provider, pipeline, NULL, options), &error);
  g_assert_no_error (error);
  g_assert_nonnull (run);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (operation_manager)), ==, 1);

  operation = g_list_model_get_item (G_LIST_MODEL (operation_manager), 0);
  title = foundry_operation_dup_title (operation);
  subtitle = foundry_operation_dup_subtitle (operation);
  g_assert_cmpstr (title, ==, "Run CI pipeline");
  g_assert_cmpstr (subtitle, ==, "Pipeline");
  g_assert_cmpfloat (foundry_operation_get_progress (operation), ==, 0.5);

  test_ci_run_set_progress ((TestCiRun *)run, 0.75);
  g_assert_cmpfloat (foundry_operation_get_progress (operation), ==, 0.75);

  test_ci_run_complete ((TestCiRun *)run);
  wait_for_n_operations (G_LIST_MODEL (operation_manager), 0);
  g_clear_object (&operation);
  g_clear_object (&run);

  run = dex_await_object (foundry_ci_provider_run (provider, pipeline, NULL, options), &error);
  g_assert_no_error (error);
  g_assert_nonnull (run);
  operation = g_list_model_get_item (G_LIST_MODEL (operation_manager), 0);
  foundry_operation_cancel (operation);
  g_assert_true (((TestCiRun *)run)->cancelled);
  wait_for_n_operations (G_LIST_MODEL (operation_manager), 0);
  g_clear_object (&operation);
  g_clear_object (&run);

  jobs = foundry_ci_pipeline_list_jobs (pipeline);
  job = g_list_model_get_item (jobs, 0);
  shell_run = dex_await_object (foundry_ci_provider_run_shell (provider, job, options), &error);
  g_assert_no_error (error);
  g_assert_nonnull (shell_run);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (operation_manager)), ==, 0);
  foundry_ci_run_cancel (shell_run);
}

static void
test_ci_provider_run_operation (void)
{
  test_from_fiber (test_ci_provider_run_operation_fiber);
}

static void
test_ci_run_vfuncs (void)
{
  g_autoptr(FoundryCiRun) run = NULL;
  g_autoptr(GListModel) artifacts = NULL;
  g_autoptr(DexFuture) completion = NULL;
  g_autofree char *output_dir = NULL;
  g_autofree char *property_output_dir = NULL;
  FoundryCiRunState state;
  double progress;
  int exit_status;

  run = g_object_new (test_ci_run_get_type (), NULL);

  completion = foundry_ci_run_await (run);
  g_assert_true (dex_future_is_pending (completion));

  g_assert_cmpint (foundry_ci_run_get_state (run), ==, FOUNDRY_CI_RUN_STATE_RUNNING);
  g_assert_cmpfloat (foundry_ci_run_get_progress (run), ==, 0.5);
  g_assert_cmpint (foundry_ci_run_get_exit_status (run), ==, -1);

  output_dir = foundry_ci_run_dup_output_dir (run);
  g_assert_cmpstr (output_dir, ==, "/tmp/ci-output");

  artifacts = foundry_ci_run_list_artifacts (run);
  g_assert_cmpuint (g_list_model_get_n_items (artifacts), ==, 0);

  g_object_get (run,
                "exit-status", &exit_status,
                "output-dir", &property_output_dir,
                "progress", &progress,
                "state", &state,
                NULL);
  g_assert_cmpint (exit_status, ==, -1);
  g_assert_cmpstr (property_output_dir, ==, "/tmp/ci-output");
  g_assert_cmpfloat (progress, ==, 0.5);
  g_assert_cmpint (state, ==, FOUNDRY_CI_RUN_STATE_RUNNING);

  foundry_ci_run_cancel (run);
  g_assert_true (((TestCiRun *)run)->cancelled);
}

static void
assert_read_only_property (GObjectClass *object_class,
                           const char   *name)
{
  GParamSpec *pspec;

  pspec = g_object_class_find_property (object_class, name);
  g_assert_nonnull (pspec);
  g_assert_true ((pspec->flags & G_PARAM_READABLE) != 0);
  g_assert_true ((pspec->flags & G_PARAM_WRITABLE) == 0);
}

static void
test_ci_read_only_properties (void)
{
  GObjectClass *job_class = g_type_class_get (FOUNDRY_TYPE_CI_JOB);
  GObjectClass *pipeline_class = g_type_class_get (FOUNDRY_TYPE_CI_PIPELINE);
  GObjectClass *run_class = g_type_class_get (FOUNDRY_TYPE_CI_RUN);
  const char *job_properties[] = {
    "can-run",
    "can-shell",
    "disposition",
    "id",
    "image",
    "pipeline",
    "provider",
    "reason",
    "stage",
    "title",
  };
  const char *pipeline_properties[] = {
    "id",
    "provider",
    "title",
  };
  const char *run_properties[] = {
    "exit-status",
    "output-dir",
    "progress",
    "state",
  };

  for (guint i = 0; i < G_N_ELEMENTS (job_properties); i++)
    assert_read_only_property (job_class, job_properties[i]);
  for (guint i = 0; i < G_N_ELEMENTS (pipeline_properties); i++)
    assert_read_only_property (pipeline_class, pipeline_properties[i]);
  for (guint i = 0; i < G_N_ELEMENTS (run_properties); i++)
    assert_read_only_property (run_class, run_properties[i]);
}

int
main (int   argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Foundry/CI/pipeline-lifetime", test_ci_pipeline_lifetime);
  g_test_add_func ("/Foundry/CI/read-only-properties", test_ci_read_only_properties);
  g_test_add_func ("/Foundry/CI/provider-run-operation", test_ci_provider_run_operation);
  g_test_add_func ("/Foundry/CI/run-options", test_ci_run_options);
  g_test_add_func ("/Foundry/CI/run-vfuncs", test_ci_run_vfuncs);
  return g_test_run ();
}
