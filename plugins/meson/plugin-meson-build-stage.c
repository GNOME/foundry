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

#include "plugin-meson-build-stage.h"

struct _PluginMesonBuildStage
{
  FoundryBuildStage parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginMesonBuildStage, plugin_meson_build_stage, PLUGIN_TYPE_MESON_BASE_STAGE)

typedef struct _Run
{
  PluginMesonBuildStage *self;
  FoundryBuildProgress  *progress;
  FoundryBuildPipeline  *pipeline;
  char                  *command;
} Run;

static void
run_free (Run *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->progress);
  g_clear_object (&state->pipeline);
  g_clear_pointer (&state->command, g_free);
  g_free (state);
}

static DexFuture *
plugin_meson_build_stage_run_fiber (gpointer data)
{
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(DexCancellable) cancellable = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *builddir = NULL;
  g_autofree char *ninja = NULL;
  Run *state = data;

  g_assert (state != NULL);
  g_assert (PLUGIN_IS_MESON_BUILD_STAGE (state->self));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (state->progress));
  g_assert (FOUNDRY_IS_BUILD_PIPELINE (state->pipeline));
  g_assert (state->command != NULL);

  builddir = plugin_meson_base_stage_dup_builddir (PLUGIN_MESON_BASE_STAGE (state->self));
  ninja = plugin_meson_base_stage_dup_ninja (PLUGIN_MESON_BASE_STAGE (state->self));
  cancellable = foundry_build_progress_dup_cancellable (state->progress);

  launcher = foundry_process_launcher_new ();

  if (!dex_await (foundry_build_pipeline_prepare (state->pipeline, launcher, FOUNDRY_BUILD_PIPELINE_PHASE_BUILD), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  foundry_process_launcher_set_cwd (launcher, builddir);
  foundry_process_launcher_append_argv (launcher, ninja);
  foundry_process_launcher_append_argv (launcher, state->command);

  foundry_build_progress_setup_pty (state->progress, launcher);

  if (!(subprocess = foundry_process_launcher_spawn (launcher, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return foundry_subprocess_wait_check (subprocess, cancellable);
}

static DexFuture *
plugin_meson_build_stage_build (FoundryBuildStage    *build_stage,
                                FoundryBuildProgress *progress)
{
  Run *state;

  g_assert (PLUGIN_IS_MESON_BUILD_STAGE (build_stage));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (progress));

  state = g_new0 (Run, 1);
  state->self = g_object_ref (PLUGIN_MESON_BUILD_STAGE (build_stage));
  state->progress = g_object_ref (progress);
  state->pipeline = foundry_build_stage_dup_pipeline (build_stage);
  state->command = g_strdup ("all");

  return dex_scheduler_spawn (NULL, 0,
                              plugin_meson_build_stage_run_fiber,
                              state,
                              (GDestroyNotify) run_free);
}

static DexFuture *
plugin_meson_build_stage_clean (FoundryBuildStage    *build_stage,
                                FoundryBuildProgress *progress)
{
  Run *state;

  g_assert (PLUGIN_IS_MESON_BUILD_STAGE (build_stage));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (progress));

  state = g_new0 (Run, 1);
  state->self = g_object_ref (PLUGIN_MESON_BUILD_STAGE (build_stage));
  state->progress = g_object_ref (progress);
  state->pipeline = foundry_build_stage_dup_pipeline (build_stage);
  state->command = g_strdup ("clean");

  return dex_scheduler_spawn (NULL, 0,
                              plugin_meson_build_stage_run_fiber,
                              state,
                              (GDestroyNotify) run_free);
}

static DexFuture *
plugin_meson_build_stage_query (FoundryBuildStage *build_stage)
{
  foundry_build_stage_set_completed (build_stage, FALSE);
  return dex_future_new_true ();
}

static FoundryBuildPipelinePhase
plugin_meson_build_stage_get_phase (FoundryBuildStage *build_stage)
{
  return FOUNDRY_BUILD_PIPELINE_PHASE_BUILD;
}

static void
plugin_meson_build_stage_class_init (PluginMesonBuildStageClass *klass)
{
  FoundryBuildStageClass *build_stage_class = FOUNDRY_BUILD_STAGE_CLASS (klass);

  build_stage_class->build = plugin_meson_build_stage_build;
  build_stage_class->clean = plugin_meson_build_stage_clean;
  build_stage_class->get_phase = plugin_meson_build_stage_get_phase;
  build_stage_class->query = plugin_meson_build_stage_query;
}

static void
plugin_meson_build_stage_init (PluginMesonBuildStage *self)
{
}
