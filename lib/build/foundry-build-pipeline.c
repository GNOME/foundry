/* foundry-build-pipeline.c
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

#include "foundry-build-pipeline.h"
#include "foundry-build-progress.h"
#include "foundry-config.h"
#include "foundry-contextual.h"
#include "foundry-device.h"
#include "foundry-sdk.h"

struct _FoundryBuildPipeline
{
  FoundryContextual  parent_instance;
  FoundryDevice     *device;
  FoundrySdk        *sdk;
};

enum {
  PROP_0,
  PROP_CONFIG,
  PROP_DEVICE,
  PROP_SDK,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryBuildPipeline, foundry_build_pipeline, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static void
foundry_build_pipeline_constructed (GObject *object)
{
  G_OBJECT_CLASS (foundry_build_pipeline_parent_class)->constructed (object);
}

static void
foundry_build_pipeline_finalize (GObject *object)
{
  FoundryBuildPipeline *self = (FoundryBuildPipeline *)object;

  g_clear_object (&self->device);
  g_clear_object (&self->sdk);

  G_OBJECT_CLASS (foundry_build_pipeline_parent_class)->finalize (object);
}

static void
foundry_build_pipeline_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  FoundryBuildPipeline *self = FOUNDRY_BUILD_PIPELINE (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_take_object (value, foundry_build_pipeline_dup_device (self));
      break;

    case PROP_SDK:
      g_value_take_object (value, foundry_build_pipeline_dup_sdk (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_build_pipeline_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  FoundryBuildPipeline *self = FOUNDRY_BUILD_PIPELINE (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    case PROP_SDK:
      self->sdk = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_build_pipeline_class_init (FoundryBuildPipelineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = foundry_build_pipeline_constructed;
  object_class->finalize = foundry_build_pipeline_finalize;
  object_class->get_property = foundry_build_pipeline_get_property;
  object_class->set_property = foundry_build_pipeline_set_property;

  properties[PROP_CONFIG] =
    g_param_spec_object ("config", NULL, NULL,
                         FOUNDRY_TYPE_CONFIG,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         FOUNDRY_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SDK] =
    g_param_spec_object ("sdk", NULL, NULL,
                         FOUNDRY_TYPE_SDK,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_build_pipeline_init (FoundryBuildPipeline *self)
{
}

DexFuture *
foundry_build_pipeline_new (FoundryContext *context,
                            FoundryConfig  *config,
                            FoundryDevice  *device,
                            FoundrySdk     *sdk)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (FOUNDRY_IS_CONFIG (config), NULL);
  g_return_val_if_fail (FOUNDRY_IS_DEVICE (device), NULL);
  g_return_val_if_fail (FOUNDRY_IS_SDK (sdk), NULL);

  return dex_future_new_take_object (g_object_new (FOUNDRY_TYPE_BUILD_PIPELINE,
                                                   "context", context,
                                                   "config", config,
                                                   "device", device,
                                                   "sdk", sdk,
                                                   NULL));
}

/**
 * foundry_build_pipeline_build:
 * @self: a #FoundryBuildPipeline
 *
 * Build the build_pipeline.
 *
 * Returns: (transfer full): a #FoundryBuildProgress
 */
FoundryBuildProgress *
foundry_build_pipeline_build (FoundryBuildPipeline      *self,
                              FoundryBuildPipelinePhase  phase)
{
  g_autoptr(FoundryContext) context = NULL;

  g_return_val_if_fail (FOUNDRY_IS_BUILD_PIPELINE (self), NULL);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  return g_object_new (FOUNDRY_TYPE_BUILD_PROGRESS,
                       "context", context,
                       NULL);
}

/**
 * foundry_build_pipeline_dup_device:
 * @self: a #FoundryBuildPipeline
 *
 * Gets the device used for the pipeline.
 *
 * Returns: (transfer full): a #FoundryDevice
 */
FoundryDevice *
foundry_build_pipeline_dup_device (FoundryBuildPipeline *self)
{
  g_return_val_if_fail (FOUNDRY_IS_BUILD_PIPELINE (self), NULL);
  g_return_val_if_fail (FOUNDRY_IS_DEVICE (self->device), NULL);

  return g_object_ref (self->device);
}

/**
 * foundry_build_pipeline_dup_sdk:
 * @self: a #FoundryBuildPipeline
 *
 * Gets the SDK to use for the platform.
 *
 * Returns: (transfer full): a #FoundrySdk
 */
FoundrySdk *
foundry_build_pipeline_dup_sdk (FoundryBuildPipeline *self)
{
  g_return_val_if_fail (FOUNDRY_IS_BUILD_PIPELINE (self), NULL);
  g_return_val_if_fail (FOUNDRY_IS_SDK (self->sdk), NULL);

  return g_object_ref (self->sdk);
}
