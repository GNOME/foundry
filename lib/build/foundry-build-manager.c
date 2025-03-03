/* foundry-build-manager.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "eggactiongroup.h"

#include "foundry-build-manager.h"
#include "foundry-build-pipeline-private.h"
#include "foundry-config.h"
#include "foundry-config-manager.h"
#include "foundry-debug.h"
#include "foundry-device.h"
#include "foundry-device-manager.h"
#include "foundry-sdk.h"
#include "foundry-sdk-manager.h"
#include "foundry-service-private.h"

struct _FoundryBuildManager
{
  FoundryService  parent_instance;
  DexFuture      *pipeline;
};

struct _FoundryBuildManagerClass
{
  FoundryServiceClass parent_instance;
};

static void
foundry_build_manager_build_action (FoundryBuildManager *self,
                                    GVariant            *param)
{
  g_assert (FOUNDRY_IS_BUILD_MANAGER (self));

  g_printerr ("TODO: Build action\n");
}

EGG_DEFINE_ACTION_GROUP (FoundryBuildManager, foundry_build_manager, {
  { "build", foundry_build_manager_build_action },
})

G_DEFINE_QUARK (foundry_build_error, foundry_build_error)
G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryBuildManager, foundry_build_manager, FOUNDRY_TYPE_SERVICE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, foundry_build_manager_init_action_group))

static void
foundry_build_manager_class_init (FoundryBuildManagerClass *klass)
{
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  foundry_service_class_set_action_prefix (service_class, "build-manager");
}

static void
foundry_build_manager_init (FoundryBuildManager *self)
{
}

static DexFuture *
foundry_build_manager_load_pipeline_fiber (gpointer user_data)
{
  FoundryBuildManager *self = user_data;
  g_autoptr(FoundryConfigManager) config_manager = NULL;
  g_autoptr(FoundryDeviceManager) device_manager = NULL;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundrySdkManager) sdk_manager = NULL;
  g_autoptr(FoundryConfig) config = NULL;
  g_autoptr(FoundryDevice) device = NULL;
  g_autoptr(FoundrySdk) sdk = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_BUILD_MANAGER (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  dex_return_error_if_fail (FOUNDRY_IS_CONTEXT (context));

  if (foundry_context_is_shared (context))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Building is not supported in shared mode");

  config_manager = foundry_context_dup_config_manager (context);
  dex_return_error_if_fail (FOUNDRY_IS_CONFIG_MANAGER (config_manager));

  device_manager = foundry_context_dup_device_manager (context);
  dex_return_error_if_fail (FOUNDRY_IS_DEVICE_MANAGER (device_manager));

  sdk_manager = foundry_context_dup_sdk_manager (context);
  dex_return_error_if_fail (FOUNDRY_IS_SDK_MANAGER (sdk_manager));

  if (!dex_await (dex_future_all (foundry_service_when_ready (FOUNDRY_SERVICE (self)),
                                  foundry_service_when_ready (FOUNDRY_SERVICE (config_manager)),
                                  foundry_service_when_ready (FOUNDRY_SERVICE (device_manager)),
                                  foundry_service_when_ready (FOUNDRY_SERVICE (sdk_manager)),
                                  NULL),
                  &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(config = foundry_config_manager_dup_config (config_manager)))
    return dex_future_new_reject (FOUNDRY_BUILD_ERROR,
                                  FOUNDRY_BUILD_ERROR_INVALID_CONFIG,
                                  _("Project does not contain an active build configuration"));

  if (!(device = foundry_device_manager_dup_device (device_manager)))
    return dex_future_new_reject (FOUNDRY_BUILD_ERROR,
                                  FOUNDRY_BUILD_ERROR_INVALID_DEVICE,
                                  _("Project does not contain an active build device"));

  if (!(sdk = foundry_sdk_manager_dup_sdk (sdk_manager)))
    return dex_future_new_reject (FOUNDRY_BUILD_ERROR,
                                  FOUNDRY_BUILD_ERROR_INVALID_SDK,
                                  _("Project does not contain an active SDK"));

  if (!(pipeline = dex_await_object (foundry_build_pipeline_new (context, config, device, sdk, TRUE), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!dex_await (_foundry_build_pipeline_load (pipeline), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&pipeline));
}

/**
 * foundry_build_manager_load_pipeline:
 * @self: a #FoundryBuildManager
 *
 * Loads the pipeline as a future.
 *
 * If the pipeline is already being loaded, the future will be completed
 * as part of that request.
 *
 * If the pipeline is already loaded, the future returned will already
 * be resolved.
 *
 * Otherwise, a new request to load the pipeline is created and the future
 * will resolve upon completion.
 *
 * Returns: (transfer full): a #DexFuture that resolves to a #FoundryPipeline
 */
DexFuture *
foundry_build_manager_load_pipeline (FoundryBuildManager *self)
{
  g_return_val_if_fail (FOUNDRY_IS_BUILD_MANAGER (self), NULL);

  if (self->pipeline == NULL)
    self->pipeline = dex_scheduler_spawn (dex_scheduler_get_default (), 0,
                                          foundry_build_manager_load_pipeline_fiber,
                                          g_object_ref (self),
                                          g_object_unref);

  return dex_ref (DEX_FUTURE (self->pipeline));
}

void
foundry_build_manager_invalidate (FoundryBuildManager *self)
{
  g_return_if_fail (FOUNDRY_IS_BUILD_MANAGER (self));

  dex_clear (&self->pipeline);
}
