/* plugin-just-build-stage.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "plugin-just-build-stage.h"

struct _PluginJustBuildStage
{
  FoundryBuildStage parent_instance;
  char *just;
};

G_DEFINE_FINAL_TYPE (PluginJustBuildStage, plugin_just_build_stage, FOUNDRY_TYPE_BUILD_STAGE)

enum {
  PROP_0,
  PROP_JUST,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static DexFuture *
plugin_just_build_stage_run_fiber (PluginJustBuildStage *self,
                                   FoundryBuildProgress *progress,
                                   FoundryBuildPipeline *pipeline,
                                   const char           *command)
{
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(DexCancellable) cancellable = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *builddir = NULL;
  g_autofree char *just = NULL;

  g_assert (PLUGIN_IS_JUST_BUILD_STAGE (self));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (progress));
  g_assert (FOUNDRY_IS_BUILD_PIPELINE (pipeline));
  g_assert (command != NULL);

  if (!(builddir = foundry_build_pipeline_dup_builddir (pipeline)))
    return foundry_future_new_disposed ();

  if (!g_set_str (&just, self->just))
    just = g_strdup ("just");

  cancellable = foundry_build_progress_dup_cancellable (progress);
  launcher = foundry_process_launcher_new ();

  if (!dex_await (foundry_build_pipeline_prepare (pipeline, launcher, FOUNDRY_BUILD_PIPELINE_PHASE_BUILD), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  foundry_process_launcher_set_cwd (launcher, builddir);
  foundry_process_launcher_append_argv (launcher, just);
  /* TODO: This just uses "clean" or "build" and expects them to be
   *       in the Justfile. That isn't quite right and we may need to
   *       do some introspection on the better thing to do. For example,
   *       look for the [default] from summary.
   *
   *       But as it were, we also want to allow for building custom
   *       build targets directly eventually.
   */
  foundry_process_launcher_append_argv (launcher, command);

  foundry_build_progress_setup_pty (progress, launcher);

  if (!(subprocess = foundry_process_launcher_spawn (launcher, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return foundry_subprocess_wait_check (subprocess, cancellable);
}

static DexFuture *
plugin_just_build_stage_build (FoundryBuildStage    *build_stage,
                               FoundryBuildProgress *progress)
{
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;

  g_assert (PLUGIN_IS_JUST_BUILD_STAGE (build_stage));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (progress));

  pipeline = foundry_build_stage_dup_pipeline (build_stage);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_just_build_stage_run_fiber),
                                  4,
                                  FOUNDRY_TYPE_BUILD_STAGE, build_stage,
                                  FOUNDRY_TYPE_BUILD_PROGRESS, progress,
                                  FOUNDRY_TYPE_BUILD_PIPELINE, pipeline,
                                  G_TYPE_STRING, "build");
}

static DexFuture *
plugin_just_build_stage_clean (FoundryBuildStage    *build_stage,
                               FoundryBuildProgress *progress)
{
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;

  g_assert (PLUGIN_IS_JUST_BUILD_STAGE (build_stage));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (progress));

  pipeline = foundry_build_stage_dup_pipeline (build_stage);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_just_build_stage_run_fiber),
                                  4,
                                  FOUNDRY_TYPE_BUILD_STAGE, build_stage,
                                  FOUNDRY_TYPE_BUILD_PROGRESS, progress,
                                  FOUNDRY_TYPE_BUILD_PIPELINE, pipeline,
                                  G_TYPE_STRING, "clean");
}

static DexFuture *
plugin_just_build_stage_query (FoundryBuildStage *build_stage)
{
  foundry_build_stage_set_completed (build_stage, FALSE);
  return dex_future_new_true ();
}

static FoundryBuildPipelinePhase
plugin_just_build_stage_get_phase (FoundryBuildStage *stage)
{
  return FOUNDRY_BUILD_PIPELINE_PHASE_BUILD;
}

static void
plugin_just_build_stage_finalize (GObject *object)
{
  PluginJustBuildStage *self = (PluginJustBuildStage *)object;

  g_clear_pointer (&self->just, g_free);

  G_OBJECT_CLASS (plugin_just_build_stage_parent_class)->finalize (object);
}

static void
plugin_just_build_stage_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  PluginJustBuildStage *self = PLUGIN_JUST_BUILD_STAGE (object);

  switch (prop_id)
    {
    case PROP_JUST:
      g_value_set_string (value, self->just);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_just_build_stage_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  PluginJustBuildStage *self = PLUGIN_JUST_BUILD_STAGE (object);

  switch (prop_id)
    {
    case PROP_JUST:
      if (g_set_str (&self->just, g_value_get_string (value)))
        g_object_notify_by_pspec (object, pspec);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_just_build_stage_class_init (PluginJustBuildStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryBuildStageClass *build_stage_class = FOUNDRY_BUILD_STAGE_CLASS (klass);

  object_class->finalize = plugin_just_build_stage_finalize;
  object_class->get_property = plugin_just_build_stage_get_property;
  object_class->set_property = plugin_just_build_stage_set_property;

  build_stage_class->build = plugin_just_build_stage_build;
  build_stage_class->clean = plugin_just_build_stage_clean;
  build_stage_class->query = plugin_just_build_stage_query;
  build_stage_class->get_phase = plugin_just_build_stage_get_phase;

  properties[PROP_JUST] =
    g_param_spec_string ("just", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_just_build_stage_init (PluginJustBuildStage *self)
{
  self->just = g_strdup ("just");
}
