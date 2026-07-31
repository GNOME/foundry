/* foundry-ci-manager.c
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

#include <libpeas.h>

#include "foundry-ci-manager-private.h"
#include "foundry-ci-job.h"
#include "foundry-ci-pipeline.h"
#include "foundry-ci-provider-private.h"
#include "foundry-directory-reaper.h"
#include "foundry-settings.h"
#include "foundry-service-private.h"
#include "foundry-util-private.h"

/**
 * FoundryCiManager:
 *
 * Project service which aggregates installed continuous integration
 * providers.
 *
 * The manager loads [class@Foundry.CiProvider] extensions and routes runs
 * back to the provider which created a pipeline or job.
 *
 * Since: 1.2
 */

struct _FoundryCiManager
{
  FoundryService    parent_instance;
  PeasExtensionSet *providers;
};

struct _FoundryCiManagerClass
{
  FoundryServiceClass parent_class;
};

enum {
  INVALIDATED,
  N_SIGNALS
};

G_DEFINE_FINAL_TYPE (FoundryCiManager, foundry_ci_manager, FOUNDRY_TYPE_SERVICE)

static guint signals[N_SIGNALS];

DexFuture *
_foundry_ci_manager_prune_outputs (FoundryCiManager *self,
                                   GTimeSpan         min_age)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryDirectoryReaper) reaper = NULL;
  g_autoptr(GFile) output_directory = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_CI_MANAGER (self));
  dex_return_error_if_fail (min_age >= 0);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  output_directory = foundry_context_cache_file (context, "ci-output", NULL);
  reaper = foundry_directory_reaper_new ();
  foundry_directory_reaper_add_directory (reaper, output_directory, min_age);

  return foundry_directory_reaper_execute (reaper);
}

static void
foundry_ci_manager_prune_old_outputs (FoundryCiManager *self)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundrySettings) settings = NULL;
  guint retention_days;

  g_assert (FOUNDRY_IS_CI_MANAGER (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  settings = foundry_context_load_settings (context, "org.gnome.foundry.ci", NULL);
  retention_days = foundry_settings_get_uint (settings, "retention-days");

  if (retention_days > 0)
    dex_future_disown (_foundry_ci_manager_prune_outputs (self, (GTimeSpan)retention_days * G_TIME_SPAN_DAY));
}

static void
foundry_ci_manager_provider_invalidated_cb (FoundryCiManager  *self,
                                            FoundryCiProvider *provider)
{
  g_assert (FOUNDRY_IS_CI_MANAGER (self));
  g_assert (FOUNDRY_IS_CI_PROVIDER (provider));
  g_signal_emit (self, signals[INVALIDATED], 0);
}

static void
foundry_ci_manager_provider_added (PeasExtensionSet *set,
                                   PeasPluginInfo   *plugin_info,
                                   GObject          *extension,
                                   gpointer          user_data)
{
  FoundryCiProvider *provider = FOUNDRY_CI_PROVIDER (extension);
  FoundryCiManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_CI_MANAGER (self));

  g_signal_connect_object (provider,
                           "invalidated",
                           G_CALLBACK (foundry_ci_manager_provider_invalidated_cb),
                           self,
                           G_CONNECT_SWAPPED);
  dex_future_disown (_foundry_ci_provider_load (provider));
  g_signal_emit (self, signals[INVALIDATED], 0);
}

static void
foundry_ci_manager_provider_removed (PeasExtensionSet *set,
                                     PeasPluginInfo   *plugin_info,
                                     GObject          *extension,
                                     gpointer          user_data)
{
  FoundryCiProvider *provider = FOUNDRY_CI_PROVIDER (extension);
  FoundryCiManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_CI_MANAGER (self));

  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (foundry_ci_manager_provider_invalidated_cb),
                                        self);
  dex_future_disown (_foundry_ci_provider_unload (provider));
  g_signal_emit (self, signals[INVALIDATED], 0);
}

static DexFuture *
foundry_ci_manager_start_fiber (gpointer user_data)
{
  FoundryCiManager *self = user_data;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_CI_MANAGER (self));

  g_signal_connect_object (self->providers,
                           "extension-added",
                           G_CALLBACK (foundry_ci_manager_provider_added),
                           self,
                           0);
  g_signal_connect_object (self->providers,
                           "extension-removed",
                           G_CALLBACK (foundry_ci_manager_provider_removed),
                           self,
                           0);

  futures = g_ptr_array_new_with_free_func (dex_unref);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->providers));
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryCiProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->providers), i);

      g_signal_connect_object (provider,
                               "invalidated",
                               G_CALLBACK (foundry_ci_manager_provider_invalidated_cb),
                               self,
                               G_CONNECT_SWAPPED);
      g_ptr_array_add (futures, _foundry_ci_provider_load (provider));
    }

  if (futures->len > 0)
    dex_await (foundry_future_all (futures), NULL);

  return dex_future_new_true ();
}

static DexFuture *
foundry_ci_manager_start (FoundryService *service)
{
  FoundryCiManager *self = (FoundryCiManager *)service;

  return dex_scheduler_spawn (NULL, 0,
                              foundry_ci_manager_start_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static DexFuture *
foundry_ci_manager_stop (FoundryService *service)
{
  FoundryCiManager *self = (FoundryCiManager *)service;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_signal_handlers_disconnect_by_data (self->providers, self);

  futures = g_ptr_array_new_with_free_func (dex_unref);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->providers));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryCiProvider) provider =
        g_list_model_get_item (G_LIST_MODEL (self->providers), i);

      g_signal_handlers_disconnect_by_data (provider, self);
      g_ptr_array_add (futures, _foundry_ci_provider_unload (provider));
    }

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static void
foundry_ci_manager_constructed (GObject *object)
{
  FoundryCiManager *self = FOUNDRY_CI_MANAGER (object);
  g_autoptr(FoundryContext) context = NULL;

  G_OBJECT_CLASS (foundry_ci_manager_parent_class)->constructed (object);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  self->providers = peas_extension_set_new (peas_engine_get_default (),
                                            FOUNDRY_TYPE_CI_PROVIDER,
                                            "context", context,
                                            NULL);
}

static void
foundry_ci_manager_finalize (GObject *object)
{
  FoundryCiManager *self = FOUNDRY_CI_MANAGER (object);

  g_clear_object (&self->providers);

  G_OBJECT_CLASS (foundry_ci_manager_parent_class)->finalize (object);
}

static void
foundry_ci_manager_class_init (FoundryCiManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  object_class->constructed = foundry_ci_manager_constructed;
  object_class->finalize = foundry_ci_manager_finalize;
  service_class->start = foundry_ci_manager_start;
  service_class->stop = foundry_ci_manager_stop;

  /**
   * FoundryCiManager::invalidated:
   *
   * Emitted when the available CI pipelines may have changed.
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
foundry_ci_manager_init (FoundryCiManager *self)
{
}

static DexFuture *
foundry_ci_manager_list_pipelines_fiber (gpointer user_data)
{
  FoundryCiManager *self = user_data;
  g_autoptr(GListStore) pipelines = NULL;
  g_autoptr(GError) error = NULL;
  guint n_providers;

  g_assert (FOUNDRY_IS_CI_MANAGER (self));

  pipelines = g_list_store_new (FOUNDRY_TYPE_CI_PIPELINE);
  n_providers = g_list_model_get_n_items (G_LIST_MODEL (self->providers));

  for (guint i = 0; i < n_providers; i++)
    {
      g_autoptr(FoundryCiProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->providers), i);
      g_autoptr(GListModel) model = NULL;
      guint n_items;

      if (!(model = dex_await_object (foundry_ci_provider_list_pipelines (provider), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      n_items = g_list_model_get_n_items (model);
      for (guint j = 0; j < n_items; j++)
        {
          g_autoptr(FoundryCiPipeline) pipeline = g_list_model_get_item (model, j);
          g_list_store_append (pipelines, pipeline);
        }
    }

  return dex_future_new_take_object (g_steal_pointer (&pipelines));
}

/**
 * foundry_ci_manager_list_pipelines:
 * @self: a [class@Foundry.CiManager]
 *
 * Lists pipelines from all loaded CI providers.
 *
 * This is active discovery and may access remote resources. UI consumers
 * should call this only in response to an explicit user action, such as
 * opening a continuous integration panel.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.CiPipeline] or rejects with error
 *
 * Since: 1.2
 */
DexFuture *
foundry_ci_manager_list_pipelines (FoundryCiManager *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_CI_MANAGER (self));

  return dex_scheduler_spawn (NULL, 0,
                              foundry_ci_manager_list_pipelines_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

/**
 * foundry_ci_manager_run:
 * @self: a [class@Foundry.CiManager]
 * @pipeline: a pipeline to run
 * @job_ids: (nullable) (array zero-terminated=1): job identifiers, or %NULL
 * @options: (nullable): options for the run
 *
 * Starts selected jobs using the provider which created @pipeline.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.CiRun] or rejects with error
 *
 * Since: 1.2
 */
DexFuture *
foundry_ci_manager_run (FoundryCiManager    *self,
                        FoundryCiPipeline   *pipeline,
                        const char * const  *job_ids,
                        FoundryCiRunOptions *options)
{
  g_autoptr(FoundryCiProvider) provider = NULL;
  g_autoptr(FoundryCiRunOptions) default_options = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_CI_MANAGER (self));
  dex_return_error_if_fail (FOUNDRY_IS_CI_PIPELINE (pipeline));
  dex_return_error_if_fail (options == NULL || FOUNDRY_IS_CI_RUN_OPTIONS (options));

  if (options == NULL)
    options = default_options = foundry_ci_run_options_new ();

  if (!(provider = foundry_ci_pipeline_dup_provider (pipeline)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "%s does not have a CI provider",
                                  G_OBJECT_TYPE_NAME (pipeline));

  foundry_ci_manager_prune_old_outputs (self);

  return foundry_ci_provider_run (provider, pipeline, job_ids, options);
}

/**
 * foundry_ci_manager_run_shell:
 * @self: a [class@Foundry.CiManager]
 * @job: a job whose environment should be opened
 * @options: (nullable): options for the shell
 *
 * Starts a shell using the provider which created @job.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.CiRun] or rejects with error
 *
 * Since: 1.2
 */
DexFuture *
foundry_ci_manager_run_shell (FoundryCiManager    *self,
                              FoundryCiJob        *job,
                              FoundryCiRunOptions *options)
{
  g_autoptr(FoundryCiProvider) provider = NULL;
  g_autoptr(FoundryCiRunOptions) default_options = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_CI_MANAGER (self));
  dex_return_error_if_fail (FOUNDRY_IS_CI_JOB (job));
  dex_return_error_if_fail (options == NULL || FOUNDRY_IS_CI_RUN_OPTIONS (options));

  if (options == NULL)
    options = default_options = foundry_ci_run_options_new ();

  if (!(provider = foundry_ci_job_dup_provider (job)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "%s does not have a CI provider",
                                  G_OBJECT_TYPE_NAME (job));

  foundry_ci_manager_prune_old_outputs (self);

  return foundry_ci_provider_run_shell (provider, job, options);
}
