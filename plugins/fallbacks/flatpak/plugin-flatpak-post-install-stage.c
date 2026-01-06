/* plugin-flatpak-post-install-stage.c
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

#include "plugin-flatpak-config.h"
#include "plugin-flatpak-post-install-stage.h"

struct _PluginFlatpakPostInstallStage
{
  FoundryBuildStage parent_instance;
  PluginFlatpakConfig *config;
};

enum {
  PROP_0,
  PROP_CONFIG,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (PluginFlatpakPostInstallStage, plugin_flatpak_post_install_stage, FOUNDRY_TYPE_BUILD_STAGE)

static DexFuture *
plugin_flatpak_post_install_stage_build_fiber (gpointer data)
{
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryFlatpakModule) primary_module = NULL;
  g_autoptr(DexCancellable) cancellable = NULL;
  PluginFlatpakPostInstallStage *self;
  FoundryBuildProgress *progress;
  FoundryPair *pair = data;
  g_auto(GStrv) post_install = NULL;
  g_autofree char *srcdir = NULL;

  g_assert (pair != NULL);
  g_assert (PLUGIN_IS_FLATPAK_POST_INSTALL_STAGE (pair->first));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (pair->second));

  self = PLUGIN_FLATPAK_POST_INSTALL_STAGE (pair->first);
  progress = FOUNDRY_BUILD_PROGRESS (pair->second);
  pipeline = foundry_build_stage_dup_pipeline (FOUNDRY_BUILD_STAGE (self));
  cancellable = foundry_build_progress_dup_cancellable (progress);
  srcdir = foundry_build_pipeline_dup_srcdir (pipeline);

  primary_module = plugin_flatpak_config_dup_primary_module (self->config);

  if (primary_module != NULL)
    g_object_get (primary_module, "post-install", &post_install, NULL);

  if (post_install == NULL || post_install[0] == NULL)
    return dex_future_new_true ();

  for (guint i = 0; post_install[i]; i++)
    {
      g_autoptr(FoundryProcessLauncher) launcher = NULL;
      g_autoptr(GSubprocess) subprocess = NULL;
      g_autoptr(GError) error = NULL;

      launcher = foundry_process_launcher_new ();
      foundry_process_launcher_set_cwd (launcher, srcdir);

      /* Prepare the pipeline for execution */
      if (!dex_await (foundry_build_pipeline_prepare (pipeline, launcher, FOUNDRY_BUILD_PIPELINE_PHASE_INSTALL), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      /* Push shell to run the command with sh -c */
      foundry_process_launcher_push_shell (launcher, 0);

      /* Append the command as a single argument to sh -c */
      foundry_process_launcher_append_argv (launcher, post_install[i]);

      /* Setup the PTY so output ends up in the right place */
      foundry_build_progress_setup_pty (progress, launcher);

      /* Spawn the process within the pipeline */
      if (!(subprocess = foundry_process_launcher_spawn (launcher, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      /* Await completion of subprocess but possibly force-exit it on cancellation */
      if (!dex_await (foundry_subprocess_wait_check (subprocess, cancellable), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

static DexFuture *
plugin_flatpak_post_install_stage_build (FoundryBuildStage    *build_stage,
                                          FoundryBuildProgress *progress)
{
  PluginFlatpakPostInstallStage *self = (PluginFlatpakPostInstallStage *)build_stage;

  g_assert (PLUGIN_IS_FLATPAK_POST_INSTALL_STAGE (build_stage));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (progress));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_flatpak_post_install_stage_build_fiber,
                              foundry_pair_new (build_stage, progress),
                              (GDestroyNotify) foundry_pair_free);
}

static FoundryBuildPipelinePhase
plugin_flatpak_post_install_stage_get_phase (FoundryBuildStage *build_stage)
{
  return FOUNDRY_BUILD_PIPELINE_PHASE_INSTALL | FOUNDRY_BUILD_PIPELINE_PHASE_AFTER;
}

static DexFuture *
plugin_flatpak_post_install_stage_query (FoundryBuildStage *build_stage)
{
  g_assert (PLUGIN_IS_FLATPAK_POST_INSTALL_STAGE (build_stage));

  foundry_build_stage_set_completed (build_stage, FALSE);

  return dex_future_new_true ();
}

static void
plugin_flatpak_post_install_stage_finalize (GObject *object)
{
  PluginFlatpakPostInstallStage *self = (PluginFlatpakPostInstallStage *)object;

  g_clear_object (&self->config);

  G_OBJECT_CLASS (plugin_flatpak_post_install_stage_parent_class)->finalize (object);
}

static void
plugin_flatpak_post_install_stage_get_property (GObject    *object,
                                                 guint       prop_id,
                                                 GValue     *value,
                                                 GParamSpec *pspec)
{
  PluginFlatpakPostInstallStage *self = PLUGIN_FLATPAK_POST_INSTALL_STAGE (object);

  switch (prop_id)
    {
    case PROP_CONFIG:
      g_value_set_object (value, self->config);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_post_install_stage_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec)
{
  PluginFlatpakPostInstallStage *self = PLUGIN_FLATPAK_POST_INSTALL_STAGE (object);

  switch (prop_id)
    {
    case PROP_CONFIG:
      g_set_object (&self->config, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_post_install_stage_class_init (PluginFlatpakPostInstallStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryBuildStageClass *build_stage_class = FOUNDRY_BUILD_STAGE_CLASS (klass);

  object_class->finalize = plugin_flatpak_post_install_stage_finalize;
  object_class->get_property = plugin_flatpak_post_install_stage_get_property;
  object_class->set_property = plugin_flatpak_post_install_stage_set_property;

  build_stage_class->get_phase = plugin_flatpak_post_install_stage_get_phase;
  build_stage_class->build = plugin_flatpak_post_install_stage_build;
  build_stage_class->query = plugin_flatpak_post_install_stage_query;

  properties[PROP_CONFIG] =
    g_param_spec_object ("config", NULL, NULL,
                         PLUGIN_TYPE_FLATPAK_CONFIG,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_flatpak_post_install_stage_init (PluginFlatpakPostInstallStage *self)
{
}

FoundryBuildStage *
plugin_flatpak_post_install_stage_new (FoundryContext     *context,
                                       PluginFlatpakConfig *config)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_CONFIG (config), NULL);

  return g_object_new (PLUGIN_TYPE_FLATPAK_POST_INSTALL_STAGE,
                       "context", context,
                       "config", config,
                       "kind", "flatpak",
                       "title", _("Post-Install Commands"),
                       NULL);
}
