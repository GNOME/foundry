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
#include "foundry-build-pipeline.h"
#include "foundry-config.h"
#include "foundry-config-manager.h"
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
foundry_build_manager_run_action (FoundryBuildManager *self,
                                  GVariant            *param)
{
  g_assert (FOUNDRY_IS_BUILD_MANAGER (self));

}

EGG_DEFINE_ACTION_GROUP (FoundryBuildManager, foundry_build_manager, {
  { "run", foundry_build_manager_run_action },
})

G_DEFINE_QUARK (foundry_build_error, foundry_build_error)
G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryBuildManager, foundry_build_manager, FOUNDRY_TYPE_SERVICE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, foundry_build_manager_init_action_group))

static void
foundry_build_manager_finalize (GObject *object)
{
  G_OBJECT_CLASS (foundry_build_manager_parent_class)->finalize (object);
}

static void
foundry_build_manager_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_build_manager_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_build_manager_class_init (FoundryBuildManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_build_manager_finalize;
  object_class->get_property = foundry_build_manager_get_property;
  object_class->set_property = foundry_build_manager_set_property;
}

static void
foundry_build_manager_init (FoundryBuildManager *self)
{
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
    {
      g_autoptr(FoundryContext) context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
      g_autoptr(FoundryConfigManager) config_manager = foundry_context_dup_config_manager (context);
      g_autoptr(FoundryDeviceManager) device_manager = foundry_context_dup_device_manager (context);
      g_autoptr(FoundrySdkManager) sdk_manager = foundry_context_dup_sdk_manager (context);
      g_autoptr(FoundryConfig) config = foundry_config_manager_dup_config (config_manager);
      g_autoptr(FoundryDevice) device = foundry_device_manager_dup_device (device_manager);
      g_autoptr(FoundrySdk) sdk = foundry_sdk_manager_dup_sdk (sdk_manager);

      if (config == NULL)
        return dex_future_new_reject (FOUNDRY_BUILD_ERROR,
                                      FOUNDRY_BUILD_ERROR_INVALID_CONFIG,
                                      _("Project does not contain an active build configuration"));

      if (device == NULL)
        return dex_future_new_reject (FOUNDRY_BUILD_ERROR,
                                      FOUNDRY_BUILD_ERROR_INVALID_DEVICE,
                                      _("Project does not contain an active build device"));

      if (sdk == NULL)
        return dex_future_new_reject (FOUNDRY_BUILD_ERROR,
                                      FOUNDRY_BUILD_ERROR_INVALID_SDK,
                                      _("Project does not contain an active SDK"));

      self->pipeline = foundry_build_pipeline_new (context, config, device, sdk);
    }

  return dex_ref (DEX_FUTURE (self->pipeline));
}

void
foundry_build_manager_invalidate (FoundryBuildManager *self)
{
  g_return_if_fail (FOUNDRY_IS_BUILD_MANAGER (self));

  dex_clear (&self->pipeline);
}
