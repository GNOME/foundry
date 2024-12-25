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

#include <libpeas.h>

#include "foundry-build-addin-private.h"
#include "foundry-build-pipeline-private.h"
#include "foundry-build-progress.h"
#include "foundry-build-stage-private.h"
#include "foundry-config.h"
#include "foundry-contextual.h"
#include "foundry-debug.h"
#include "foundry-device.h"
#include "foundry-sdk.h"
#include "foundry-triplet.h"
#include "foundry-util.h"

struct _FoundryBuildPipeline
{
  FoundryContextual    parent_instance;
  FoundryConfig       *config;
  FoundryDevice       *device;
  FoundrySdk          *sdk;
  PeasExtensionSet    *addins;
  GListStore          *stages;
};

enum {
  PROP_0,
  PROP_ARCH,
  PROP_CONFIG,
  PROP_DEVICE,
  PROP_SDK,
  N_PROPS
};

static GType
foundry_build_pipeline_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_BUILD_STAGE;
}

static guint
foundry_build_pipeline_get_n_items (GListModel *model)
{
  return g_list_model_get_n_items (G_LIST_MODEL (FOUNDRY_BUILD_PIPELINE (model)->stages));
}

static gpointer
foundry_build_pipeline_get_item (GListModel *model,
                                 guint       position)
{
  return g_list_model_get_item (G_LIST_MODEL (FOUNDRY_BUILD_PIPELINE (model)->stages), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_build_pipeline_get_item_type;
  iface->get_n_items = foundry_build_pipeline_get_n_items;
  iface->get_item = foundry_build_pipeline_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryBuildPipeline, foundry_build_pipeline, FOUNDRY_TYPE_CONTEXTUAL,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties[N_PROPS];

static void
foundry_build_pipeline_addin_added_cb (PeasExtensionSet *set,
                                       PeasPluginInfo   *plugin_info,
                                       GObject          *addin,
                                       gpointer          user_data)
{
  FoundryBuildPipeline *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_BUILD_ADDIN (addin));
  g_assert (FOUNDRY_IS_BUILD_PIPELINE (self));

  g_debug ("Adding FoundryBuildAddin of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (_foundry_build_addin_load (FOUNDRY_BUILD_ADDIN (addin)));
}

static void
foundry_build_pipeline_addin_removed_cb (PeasExtensionSet *set,
                                         PeasPluginInfo   *plugin_info,
                                         GObject          *addin,
                                         gpointer          user_data)
{
  FoundryBuildPipeline *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_BUILD_ADDIN (addin));
  g_assert (FOUNDRY_IS_BUILD_PIPELINE (self));

  g_debug ("Removing FoundryBuildAddin of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (_foundry_build_addin_unload (FOUNDRY_BUILD_ADDIN (addin)));
}

DexFuture *
_foundry_build_pipeline_load (FoundryBuildPipeline *self)
{
  g_autoptr(GPtrArray) futures = NULL;
  DexFuture *future;
  guint n_items;

  FOUNDRY_ENTRY;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_BUILD_PIPELINE (self));

  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (foundry_build_pipeline_addin_added_cb),
                           self,
                           0);
  g_signal_connect_object (self->addins,
                           "extension-removed",
                           G_CALLBACK (foundry_build_pipeline_addin_removed_cb),
                           self,
                           0);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryBuildAddin) addin = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures, _foundry_build_addin_load (addin));
    }

  if (futures->len > 0)
    future = foundry_future_all (futures);
  else
    future = dex_future_new_true ();

  FOUNDRY_RETURN (g_steal_pointer (&future));
}

static void
foundry_build_pipeline_constructed (GObject *object)
{
  FoundryBuildPipeline *self = (FoundryBuildPipeline *)object;
  g_autoptr(FoundryContext) context = NULL;

  G_OBJECT_CLASS (foundry_build_pipeline_parent_class)->constructed (object);

  if (!(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))))
    g_return_if_reached ();

  self->addins = peas_extension_set_new (NULL,
                                         FOUNDRY_TYPE_BUILD_ADDIN,
                                         "context", context,
                                         "pipeline", self,
                                         NULL);
}

static void
foundry_build_pipeline_dispose (GObject *object)
{
  FoundryBuildPipeline *self = (FoundryBuildPipeline *)object;

  g_clear_object (&self->addins);

  g_list_store_remove_all (self->stages);

  G_OBJECT_CLASS (foundry_build_pipeline_parent_class)->dispose (object);
}

static void
foundry_build_pipeline_finalize (GObject *object)
{
  FoundryBuildPipeline *self = (FoundryBuildPipeline *)object;

  g_clear_object (&self->stages);
  g_clear_object (&self->config);
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
    case PROP_ARCH:
      g_value_take_string (value, foundry_build_pipeline_dup_arch (self));
      break;

    case PROP_CONFIG:
      g_value_take_object (value, foundry_build_pipeline_dup_config (self));
      break;

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
    case PROP_CONFIG:
      self->config = g_value_dup_object (value);
      break;

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
  object_class->dispose = foundry_build_pipeline_dispose;
  object_class->finalize = foundry_build_pipeline_finalize;
  object_class->get_property = foundry_build_pipeline_get_property;
  object_class->set_property = foundry_build_pipeline_set_property;

  properties[PROP_ARCH] =
    g_param_spec_string ("arch", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

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
  self->stages = g_list_store_new (FOUNDRY_TYPE_BUILD_STAGE);

  g_signal_connect_object (self->stages,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);
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
 * foundry_build_pipeline_dup_config:
 * @self: a #FoundryBuildPipeline
 *
 * Gets the CONFIG to use for the platform.
 *
 * Returns: (transfer full): a #FoundryConfig
 */
FoundryConfig *
foundry_build_pipeline_dup_config (FoundryBuildPipeline *self)
{
  g_return_val_if_fail (FOUNDRY_IS_BUILD_PIPELINE (self), NULL);
  g_return_val_if_fail (FOUNDRY_IS_CONFIG (self->config), NULL);

  return g_object_ref (self->config);
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

static int
foundry_build_pipeline_compare_stage (gconstpointer a,
                                      gconstpointer b,
                                      gpointer      data)
{
  FoundryBuildStage *stage_a = (FoundryBuildStage *)a;
  FoundryBuildStage *stage_b = (FoundryBuildStage *)b;
  FoundryBuildPipelinePhase phase_a;
  FoundryBuildPipelinePhase phase_b;
  FoundryBuildPipelinePhase whence_a;
  FoundryBuildPipelinePhase whence_b;
  guint prio_a;
  guint prio_b;

  if (stage_a == stage_b)
    return 0;

  phase_a = FOUNDRY_BUILD_PIPELINE_PHASE_MASK (foundry_build_stage_get_phase (stage_a));
  phase_b = FOUNDRY_BUILD_PIPELINE_PHASE_MASK (foundry_build_stage_get_phase (stage_b));

  if (phase_a < phase_b)
    return -1;

  if (phase_a > phase_b)
    return 1;

  whence_a = FOUNDRY_BUILD_PIPELINE_PHASE_WHENCE_MASK (foundry_build_stage_get_phase (stage_a));
  whence_b = FOUNDRY_BUILD_PIPELINE_PHASE_WHENCE_MASK (foundry_build_stage_get_phase (stage_b));

  if (whence_a != whence_b)
    {
      if (whence_a == FOUNDRY_BUILD_PIPELINE_PHASE_BEFORE)
        return -1;

      if (whence_b == FOUNDRY_BUILD_PIPELINE_PHASE_BEFORE)
        return 1;

      if (whence_a == 0)
        return -1;

      if (whence_b == 0)
        return 1;

      g_assert_not_reached ();
    }

  prio_a = foundry_build_stage_get_priority (stage_a);
  prio_b = foundry_build_stage_get_priority (stage_b);

  if (prio_a < prio_b)
    return -1;

  if (prio_a > prio_b)
    return 1;

  return 0;
}

void
foundry_build_pipeline_add_stage (FoundryBuildPipeline *self,
                                  FoundryBuildStage    *stage)
{
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;

  g_return_if_fail (FOUNDRY_IS_BUILD_PIPELINE (self));
  g_return_if_fail (FOUNDRY_IS_BUILD_STAGE (stage));

  if ((pipeline = foundry_build_stage_dup_pipeline (stage)))
    {
      g_critical ("%s at %p is already added to pipeline %p",
                  G_OBJECT_TYPE_NAME (self), self, pipeline);
      return;
    }

  _foundry_build_stage_set_pipeline (stage, self);

  g_list_store_insert_sorted (self->stages,
                              stage,
                              foundry_build_pipeline_compare_stage,
                              NULL);
}

void
foundry_build_pipeline_remove_stage (FoundryBuildPipeline *self,
                                     FoundryBuildStage    *stage)
{
  GListModel *model;
  guint n_items;

  g_return_if_fail (FOUNDRY_IS_BUILD_PIPELINE (self));
  g_return_if_fail (FOUNDRY_IS_BUILD_STAGE (stage));

  model = G_LIST_MODEL (self->stages);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryBuildStage) element = g_list_model_get_item (model, i);

      if (element == stage)
        {
          g_list_store_remove (self->stages, i);
          break;
        }
    }

  _foundry_build_stage_set_pipeline (stage, NULL);
}

char *
foundry_build_pipeline_dup_arch (FoundryBuildPipeline *self)
{
  g_autoptr(FoundryTriplet) triplet = NULL;

  g_return_val_if_fail (FOUNDRY_IS_BUILD_PIPELINE (self), NULL);

  if (!(triplet = foundry_device_dup_triplet (self->device)))
    return NULL;

  return g_strdup (foundry_triplet_get_arch (triplet));
}

G_DEFINE_FLAGS_TYPE (FoundryBuildPipelinePhase, foundry_build_pipeline_phase,
                     G_DEFINE_ENUM_VALUE (FOUNDRY_BUILD_PIPELINE_PHASE_NONE, "none"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_BUILD_PIPELINE_PHASE_PREPARE, "prepare"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_BUILD_PIPELINE_PHASE_DOWNLOADS, "downloads"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_BUILD_PIPELINE_PHASE_DEPENDENCIES, "dependencies"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_BUILD_PIPELINE_PHASE_AUTOGEN, "autogen"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_BUILD_PIPELINE_PHASE_CONFIGURE, "configure"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_BUILD_PIPELINE_PHASE_BUILD, "build"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_BUILD_PIPELINE_PHASE_INSTALL, "install"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_BUILD_PIPELINE_PHASE_COMMIT, "commit"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_BUILD_PIPELINE_PHASE_EXPORT, "export"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_BUILD_PIPELINE_PHASE_FINAL, "final"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_BUILD_PIPELINE_PHASE_BEFORE, "before"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_BUILD_PIPELINE_PHASE_AFTER, "after"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_BUILD_PIPELINE_PHASE_FINISHED, "finished"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_BUILD_PIPELINE_PHASE_FAILED, "failed"))
