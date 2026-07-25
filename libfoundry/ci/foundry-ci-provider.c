/* foundry-ci-provider.c
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

#include <glib/gi18n-lib.h>

#include "foundry-ci-job.h"
#include "foundry-ci-pipeline.h"
#include "foundry-ci-provider-private.h"
#include "foundry-ci-run.h"
#include "foundry-context.h"
#include "foundry-operation-manager.h"
#include "foundry-operation.h"

/**
 * FoundryCiProvider:
 *
 * Extension point for continuous integration implementations.
 *
 * Providers discover pipelines for a project and create asynchronous
 * [class@Foundry.CiRun] objects for jobs and interactive job shells.
 * Provider implementations should perform blocking work in fibers on a
 * worker scheduler and use libdex asynchronous I/O helpers where available.
 *
 * Loading a provider must remain passive. Providers should defer pipeline
 * discovery and remote access until [method@Foundry.CiProvider.list_pipelines]
 * is called by an explicit consumer action.
 *
 * Since: 1.2
 */

enum {
  INVALIDATED,
  N_SIGNALS
};

DEX_DEFINE_CLOSURE_TYPE (RunOperation, run_operation,
                         DEX_DEFINE_CLOSURE_OBJECT (FoundryCiRun, run),
                         DEX_DEFINE_CLOSURE_OBJECT (FoundryOperation, operation),
                         DEX_DEFINE_CLOSURE_OBJECT (GSignalGroup, run_signals),
                         DEX_DEFINE_CLOSURE_OBJECT (GSignalGroup, operation_signals))

DEX_DEFINE_CLOSURE_TYPE (RunOperationInfo, run_operation_info,
                         DEX_DEFINE_CLOSURE_OBJECT (FoundryContext, context),
                         DEX_DEFINE_CLOSURE_POINTER (char *, title, g_free),
                         DEX_DEFINE_CLOSURE_POINTER (char *, subtitle, g_free))

G_DEFINE_ABSTRACT_TYPE (FoundryCiProvider, foundry_ci_provider, FOUNDRY_TYPE_CONTEXTUAL)

static guint signals[N_SIGNALS];

static void
run_operation_progress_cb (FoundryCiRun     *run,
                           GParamSpec       *pspec,
                           FoundryOperation *operation)
{
  g_assert (FOUNDRY_IS_CI_RUN (run));
  g_assert (FOUNDRY_IS_OPERATION (operation));

  foundry_operation_set_progress (operation, foundry_ci_run_get_progress (run));
}

static void
run_operation_cancelled_cb (FoundryOperation *operation,
                            GParamSpec       *pspec,
                            FoundryCiRun     *run)
{
  g_assert (FOUNDRY_IS_OPERATION (operation));
  g_assert (FOUNDRY_IS_CI_RUN (run));

  if (foundry_operation_is_cancelled (operation))
    foundry_ci_run_cancel (run);
}

static DexFuture *
run_operation_completed_cb (DexFuture *completed,
                            gpointer   user_data)
{
  RunOperation *state = user_data;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_CI_RUN (state->run));
  g_assert (FOUNDRY_IS_OPERATION (state->operation));

  g_signal_group_set_target (state->run_signals, NULL);
  g_signal_group_set_target (state->operation_signals, NULL);

  if (foundry_ci_run_get_state (state->run) == FOUNDRY_CI_RUN_STATE_CANCELLED)
    foundry_operation_cancel (state->operation);
  else
    foundry_operation_complete (state->operation);

  return dex_future_new_true ();
}

static void
foundry_ci_provider_track_run (FoundryContext *context,
                               FoundryCiRun   *run,
                               const char     *title,
                               const char     *subtitle)
{
  g_autoptr(FoundryOperationManager) operation_manager = NULL;
  g_autoptr(FoundryOperation) operation = NULL;
  RunOperation *state;

  g_assert (FOUNDRY_IS_CONTEXT (context));
  g_assert (FOUNDRY_IS_CI_RUN (run));
  g_assert (title != NULL);

  if (!(operation_manager = foundry_context_dup_operation_manager (context)))
    return;

  operation = foundry_operation_manager_begin (operation_manager, title);
  foundry_operation_set_subtitle (operation, subtitle);
  foundry_operation_set_progress (operation, foundry_ci_run_get_progress (run));

  state = run_operation_new ();
  state->run = g_object_ref (run);
  state->operation = g_object_ref (operation);
  state->run_signals = g_signal_group_new (FOUNDRY_TYPE_CI_RUN);
  state->operation_signals = g_signal_group_new (FOUNDRY_TYPE_OPERATION);

  g_signal_group_connect_object (state->run_signals,
                                 "notify::progress",
                                 G_CALLBACK (run_operation_progress_cb),
                                 operation,
                                 0);
  g_signal_group_connect_object (state->operation_signals,
                                 "notify::cancelled",
                                 G_CALLBACK (run_operation_cancelled_cb),
                                 run,
                                 0);
  g_signal_group_set_target (state->run_signals, run);
  g_signal_group_set_target (state->operation_signals, operation);

  dex_future_disown (dex_future_finally (foundry_ci_run_await (run),
                                         run_operation_completed_cb,
                                         state,
                                         (GDestroyNotify) run_operation_free));
}

static DexFuture *
foundry_ci_provider_run_cb (DexFuture *completed,
                            gpointer   user_data)
{
  RunOperationInfo *info = user_data;
  g_autoptr(FoundryCiRun) run = NULL;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (info != NULL);

  value = dex_future_get_value (completed, NULL);
  if (value == NULL ||
      !G_VALUE_HOLDS_OBJECT (value) ||
      !FOUNDRY_IS_CI_RUN (g_value_get_object (value)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "CI provider did not return a FoundryCiRun");

  run = g_value_dup_object (value);
  foundry_ci_provider_track_run (info->context, run, info->title, info->subtitle);

  return dex_future_new_take_object (g_steal_pointer (&run));
}

static DexFuture *
foundry_ci_provider_real_load (FoundryCiProvider *self)
{
  return dex_future_new_true ();
}

static DexFuture *
foundry_ci_provider_real_unload (FoundryCiProvider *self)
{
  return dex_future_new_true ();
}

static void
foundry_ci_provider_class_init (FoundryCiProviderClass *klass)
{
  klass->load = foundry_ci_provider_real_load;
  klass->unload = foundry_ci_provider_real_unload;

  /**
   * FoundryCiProvider::invalidated:
   *
   * Emitted when the provider's available pipelines have changed.
   *
   * Since: 1.2
   */
  signals[INVALIDATED] =
    g_signal_new ("invalidated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
foundry_ci_provider_init (FoundryCiProvider *self)
{
}

DexFuture *
_foundry_ci_provider_load (FoundryCiProvider *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_CI_PROVIDER (self));
  return FOUNDRY_CI_PROVIDER_GET_CLASS (self)->load (self);
}

DexFuture *
_foundry_ci_provider_unload (FoundryCiProvider *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_CI_PROVIDER (self));
  return FOUNDRY_CI_PROVIDER_GET_CLASS (self)->unload (self);
}

/**
 * foundry_ci_provider_list_pipelines:
 * @self: a [class@Foundry.CiProvider]
 *
 * Lists the pipelines available for the current project.
 *
 * This is active discovery and may access remote resources. UI consumers
 * should call this only in response to an explicit user action.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.CiPipeline] or rejects with error
 *
 * Since: 1.2
 */
DexFuture *
foundry_ci_provider_list_pipelines (FoundryCiProvider *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_CI_PROVIDER (self));

  if (FOUNDRY_CI_PROVIDER_GET_CLASS (self)->list_pipelines != NULL)
    return FOUNDRY_CI_PROVIDER_GET_CLASS (self)->list_pipelines (self);

  return dex_future_new_take_object (g_list_store_new (FOUNDRY_TYPE_CI_PIPELINE));
}

/**
 * foundry_ci_provider_run:
 * @self: a [class@Foundry.CiProvider]
 * @pipeline: a pipeline created by @self
 * @job_ids: (nullable) (array zero-terminated=1): job identifiers, or %NULL
 * @options: options for the run
 *
 * Starts a local pipeline run. If @job_ids is %NULL or empty, the provider
 * runs the jobs selected by its pipeline rules. The run is exposed through
 * the context's [class@Foundry.OperationManager] until it completes or is
 * cancelled.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.CiRun] or rejects with error
 *
 * Since: 1.2
 */
DexFuture *
foundry_ci_provider_run (FoundryCiProvider   *self,
                         FoundryCiPipeline   *pipeline,
                         const char * const  *job_ids,
                         FoundryCiRunOptions *options)
{
  dex_return_error_if_fail (FOUNDRY_IS_CI_PROVIDER (self));
  dex_return_error_if_fail (FOUNDRY_IS_CI_PIPELINE (pipeline));
  dex_return_error_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (options));

  if (FOUNDRY_CI_PROVIDER_GET_CLASS (self)->run != NULL)
    {
      RunOperationInfo *info = run_operation_info_new ();

      info->context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
      info->title = g_strdup (job_ids != NULL && job_ids[0] != NULL
                              ? _("Run CI jobs")
                              : _("Run CI pipeline"));
      info->subtitle = foundry_ci_pipeline_dup_title (pipeline);

      return dex_future_then (FOUNDRY_CI_PROVIDER_GET_CLASS (self)->run (self, pipeline, job_ids, options),
                              foundry_ci_provider_run_cb,
                              g_steal_pointer (&info),
                              (GDestroyNotify) run_operation_info_free);
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "%s cannot run CI pipelines",
                                G_OBJECT_TYPE_NAME (self));
}

/**
 * foundry_ci_provider_run_shell:
 * @self: a [class@Foundry.CiProvider]
 * @job: a job created by @self
 * @options: options for the shell
 *
 * Starts an interactive shell using the job's local execution environment.
 * The standard file descriptors in @options are borrowed for the duration
 * of the run.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.CiRun] or rejects with error
 *
 * Since: 1.2
 */
DexFuture *
foundry_ci_provider_run_shell (FoundryCiProvider   *self,
                               FoundryCiJob        *job,
                               FoundryCiRunOptions *options)
{
  dex_return_error_if_fail (FOUNDRY_IS_CI_PROVIDER (self));
  dex_return_error_if_fail (FOUNDRY_IS_CI_JOB (job));
  dex_return_error_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (options));

  if (FOUNDRY_CI_PROVIDER_GET_CLASS (self)->run_shell != NULL)
    return FOUNDRY_CI_PROVIDER_GET_CLASS (self)->run_shell (self, job, options);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "`%s` cannot open CI job shells",
                                G_OBJECT_TYPE_NAME (self));
}

/**
 * foundry_ci_provider_invalidate:
 * @self: a [class@Foundry.CiProvider]
 *
 * Notifies the CI manager that the provider's pipelines have changed.
 *
 * Since: 1.2
 */
void
foundry_ci_provider_invalidate (FoundryCiProvider *self)
{
  g_return_if_fail (FOUNDRY_IS_CI_PROVIDER (self));

  g_signal_emit (self, signals[INVALIDATED], 0);
}
