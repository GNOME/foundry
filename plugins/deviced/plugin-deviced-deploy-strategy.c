/* plugin-deviced-deploy-strategy.c
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

#include "plugin-deviced-deploy-strategy.h"
#include "plugin-deviced-device.h"

#include "plugin-deviced-dex.h"

#include "../flatpak/plugin-flatpak-bundle-stage.h"
#include "../flatpak/plugin-flatpak-manifest.h"

struct _PluginDevicedDeployStrategy
{
  FoundryDeployStrategy parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginDevicedDeployStrategy, plugin_deviced_deploy_strategy, FOUNDRY_TYPE_DEPLOY_STRATEGY)

static DexFuture *
plugin_deviced_deploy_strategy_supported (FoundryDeployStrategy *deploy_strategy)
{
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryDevice) device = NULL;
  g_autoptr(FoundryConfig) config = NULL;

  g_assert (PLUGIN_IS_DEVICED_DEPLOY_STRATEGY (deploy_strategy));

  pipeline = foundry_deploy_strategy_dup_pipeline (deploy_strategy);
  device = foundry_build_pipeline_dup_device (pipeline);
  config = foundry_build_pipeline_dup_config (pipeline);

  if (!PLUGIN_IS_DEVICED_DEVICE (device) || !PLUGIN_IS_FLATPAK_MANIFEST (config))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return dex_future_new_for_int (1000);
}

static void
progress_cb (goffset  current,
             goffset  total,
             gpointer user_data)
{
  /* TODO: */
}

static DexFuture *
plugin_deviced_deploy_strategy_deploy_fiber (gpointer data)
{
  FoundryPair *pair = data;
  PluginDevicedDeployStrategy *self = PLUGIN_DEVICED_DEPLOY_STRATEGY (pair->first);
  FoundryBuildProgress *progress = FOUNDRY_BUILD_PROGRESS (pair->second);
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryDevice) device = NULL;
  g_autoptr(FoundryConfig) config = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) bundle = NULL;
  g_autofree char *app_id = NULL;
  guint n_stages;

  g_assert (pair != NULL);
  g_assert (PLUGIN_IS_DEVICED_DEPLOY_STRATEGY (self));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (progress));

  pipeline = foundry_deploy_strategy_dup_pipeline (FOUNDRY_DEPLOY_STRATEGY (self));
  config = foundry_build_pipeline_dup_config (pipeline);
  device = foundry_build_pipeline_dup_device (pipeline);

  g_assert (FOUNDRY_IS_BUILD_PIPELINE (pipeline));
  g_assert (PLUGIN_IS_DEVICED_DEVICE (device));

  n_stages = g_list_model_get_n_items (G_LIST_MODEL (pipeline));

  for (guint i = 0; i < n_stages; i++)
    {
      g_autoptr(FoundryBuildStage) stage = g_list_model_get_item (G_LIST_MODEL (pipeline), i);

      if (PLUGIN_IS_FLATPAK_BUNDLE_STAGE (stage))
        {
          bundle = plugin_flatpak_bundle_stage_dup_bundle (PLUGIN_FLATPAK_BUNDLE_STAGE (stage));
          break;
        }
    }

  if (!dex_await (foundry_build_progress_await (progress), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  app_id = plugin_flatpak_manifest_dup_id (PLUGIN_FLATPAK_MANIFEST (config));

  if (!dex_await (plugin_deviced_device_install_bundle (PLUGIN_DEVICED_DEVICE (device),
                                                        g_file_peek_path (bundle),
                                                        progress_cb,
                                                        g_object_ref (progress),
                                                        g_object_unref),
                  &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
plugin_deviced_deploy_strategy_deploy (FoundryDeployStrategy *deploy_strategy,
                                       int                    pty_fd,
                                       DexCancellable        *cancellable)
{
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryBuildProgress) progress = NULL;
  g_autoptr(FoundryDevice) device = NULL;
  g_autoptr(FoundryConfig) config = NULL;

  g_assert (PLUGIN_IS_DEVICED_DEPLOY_STRATEGY (deploy_strategy));
  g_assert (pty_fd >= -1);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  pipeline = foundry_deploy_strategy_dup_pipeline (deploy_strategy);
  device = foundry_build_pipeline_dup_device (pipeline);
  config = foundry_build_pipeline_dup_config (pipeline);

  dex_return_error_if_fail (PLUGIN_IS_DEVICED_DEVICE (device));
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_MANIFEST (config));

  progress = foundry_build_pipeline_build (pipeline,
                                           FOUNDRY_BUILD_PIPELINE_PHASE_EXPORT,
                                           pty_fd,
                                           cancellable);

  return dex_scheduler_spawn (NULL, 0,
                              plugin_deviced_deploy_strategy_deploy_fiber,
                              foundry_pair_new (deploy_strategy, progress),
                              (GDestroyNotify)foundry_pair_free);
}

static DexFuture *
plugin_deviced_deploy_strategy_prepare (FoundryDeployStrategy  *deploy_strategy,
                                        FoundryProcessLauncher *launcher,
                                        FoundryBuildPipeline   *pipeline,
                                        int                     pty_fd,
                                        DexCancellable         *cancellable)
{
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "deviced running not yet supported");
}

static void
plugin_deviced_deploy_strategy_class_init (PluginDevicedDeployStrategyClass *klass)
{
  FoundryDeployStrategyClass *deploy_strategy_class = FOUNDRY_DEPLOY_STRATEGY_CLASS (klass);

  deploy_strategy_class->supported = plugin_deviced_deploy_strategy_supported;
  deploy_strategy_class->deploy = plugin_deviced_deploy_strategy_deploy;
  deploy_strategy_class->prepare = plugin_deviced_deploy_strategy_prepare;
}

static void
plugin_deviced_deploy_strategy_init (PluginDevicedDeployStrategy *self)
{
}
