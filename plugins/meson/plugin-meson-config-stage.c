/* plugin-meson-build-stage.c
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

#include "plugin-meson-config-stage.h"

struct _PluginMesonConfigStage
{
  FoundryBuildStage parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginMesonConfigStage, plugin_meson_config_stage, PLUGIN_TYPE_MESON_BASE_STAGE)

typedef struct _Run
{
  PluginMesonConfigStage *self;
  FoundryBuildProgress   *progress;
  FoundryBuildPipeline   *pipeline;
} Run;

static void
run_free (Run *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->progress);
  g_clear_object (&state->pipeline);
  g_free (state);
}

static DexFuture *
plugin_meson_config_stage_run_fiber (gpointer data)
{
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(DexCancellable) cancellable = NULL;
  g_autoptr(FoundryConfig) config = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_dir = NULL;
  g_auto(GStrv) config_opts = NULL;
  g_autofree char *builddir = NULL;
  g_autofree char *meson = NULL;
  Run *state = data;

  g_assert (state != NULL);
  g_assert (PLUGIN_IS_MESON_CONFIG_STAGE (state->self));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (state->progress));
  g_assert (FOUNDRY_IS_BUILD_PIPELINE (state->pipeline));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (state->self));
  project_dir = foundry_context_dup_project_directory (context);
  builddir = plugin_meson_base_stage_dup_builddir (PLUGIN_MESON_BASE_STAGE (state->self));
  meson = plugin_meson_base_stage_dup_meson (PLUGIN_MESON_BASE_STAGE (state->self));
  cancellable = foundry_build_progress_dup_cancellable (state->progress);
  config = foundry_build_pipeline_dup_config (state->pipeline);

  launcher = foundry_process_launcher_new ();

  if (!dex_await (foundry_build_pipeline_prepare (state->pipeline, launcher, FOUNDRY_BUILD_PIPELINE_PHASE_CONFIGURE), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  foundry_process_launcher_set_cwd (launcher, builddir);
  foundry_process_launcher_append_argv (launcher, meson);
  foundry_process_launcher_append_argv (launcher, "setup");
  foundry_process_launcher_append_argv (launcher, builddir);
  foundry_process_launcher_append_argv (launcher, g_file_peek_path (project_dir));

  if ((config_opts = foundry_config_dup_config_opts (config)))
    foundry_process_launcher_append_args (launcher, (const char * const *)config_opts);

  foundry_build_progress_setup_pty (state->progress, launcher);

  if (!(subprocess = foundry_process_launcher_spawn (launcher, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return foundry_subprocess_wait_check (subprocess, cancellable);
}

static DexFuture *
plugin_meson_config_stage_build (FoundryBuildStage    *config_stage,
                                 FoundryBuildProgress *progress)
{
  Run *state;

  g_assert (PLUGIN_IS_MESON_CONFIG_STAGE (config_stage));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (progress));

  state = g_new0 (Run, 1);
  state->self = g_object_ref (PLUGIN_MESON_CONFIG_STAGE (config_stage));
  state->progress = g_object_ref (progress);
  state->pipeline = foundry_build_stage_dup_pipeline (config_stage);

  return dex_scheduler_spawn (NULL, 0,
                              plugin_meson_config_stage_run_fiber,
                              state,
                              (GDestroyNotify) run_free);
}

static DexFuture *
plugin_meson_config_stage_query_fiber (gpointer data)
{
  PluginMesonConfigStage *self = data;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autofree char *builddir = NULL;
  g_autofree char *coredata_dat = NULL;

  g_assert (PLUGIN_IS_MESON_CONFIG_STAGE (self));

  pipeline = foundry_build_stage_dup_pipeline (FOUNDRY_BUILD_STAGE (self));
  builddir = foundry_build_pipeline_dup_builddir (pipeline);
  coredata_dat = g_build_filename (builddir, "meson-private", "coredata.dat", NULL);

  if (dex_await_boolean (foundry_file_test (coredata_dat, G_FILE_TEST_EXISTS), NULL))
    foundry_build_stage_set_completed (FOUNDRY_BUILD_STAGE (self), TRUE);

  return dex_future_new_true ();
}

static DexFuture *
plugin_meson_config_stage_query (FoundryBuildStage *build_stage)
{
  return dex_scheduler_spawn (NULL, 0,
                              plugin_meson_config_stage_query_fiber,
                              g_object_ref (build_stage),
                              g_object_unref);
}

static DexFuture *
plugin_meson_config_stage_purge (FoundryBuildStage    *build_stage,
                                 FoundryBuildProgress *progress)
{
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryDirectoryReaper) reaper = NULL;
  g_autoptr(GFile) coredata_dat = NULL;
  g_autofree char *builddir = NULL;

  g_assert (PLUGIN_IS_MESON_CONFIG_STAGE (build_stage));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (progress));

  pipeline = foundry_build_stage_dup_pipeline (build_stage);
  builddir = foundry_build_pipeline_dup_builddir (pipeline);
  coredata_dat = g_file_new_build_filename (builddir, "meson-private", "coredata.dat", NULL);

  reaper = foundry_directory_reaper_new ();
  foundry_directory_reaper_add_file (reaper, coredata_dat, 0);

  return foundry_directory_reaper_execute (reaper);
}

static FoundryBuildPipelinePhase
plugin_meson_config_stage_get_phase (FoundryBuildStage *config_stage)
{
  return FOUNDRY_BUILD_PIPELINE_PHASE_CONFIGURE;
}

static void
plugin_meson_config_stage_class_init (PluginMesonConfigStageClass *klass)
{
  FoundryBuildStageClass *config_stage_class = FOUNDRY_BUILD_STAGE_CLASS (klass);

  config_stage_class->build = plugin_meson_config_stage_build;
  config_stage_class->get_phase = plugin_meson_config_stage_get_phase;
  config_stage_class->query = plugin_meson_config_stage_query;
  config_stage_class->purge = plugin_meson_config_stage_purge;
}

static void
plugin_meson_config_stage_init (PluginMesonConfigStage *self)
{
}
