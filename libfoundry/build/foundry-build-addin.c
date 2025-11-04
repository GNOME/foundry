/* foundry-build-addin.c
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

#include "foundry-build-addin-private.h"
#include "foundry-build-pipeline.h"
#include "foundry-file.h"
#include "foundry-util.h"

/**
 * FoundryBuildAddin:
 *
 * Customize build pipelines from plugins.
 *
 * This class provides plugins the ability to modify how a build pipeline
 * works by registering stages in the pipeline.
 */

typedef struct
{
  GWeakRef        pipeline_wr;
  PeasPluginInfo *plugin_info;
} FoundryBuildAddinPrivate;

enum {
  PROP_0,
  PROP_PIPELINE,
  PROP_PLUGIN_INFO,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryBuildAddin, foundry_build_addin, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_build_addin_real_discover_build_system_fiber (gpointer user_data)
{
  FoundryBuildAddin *self = user_data;
  FoundryBuildAddinPrivate *priv = foundry_build_addin_get_instance_private (self);

  g_assert (FOUNDRY_IS_BUILD_ADDIN (self));

  if (priv->plugin_info != NULL)
    {
      const char *x_buildsystem_pattern = peas_plugin_info_get_external_data (priv->plugin_info, "BuildSystem-Pattern");

      if (x_buildsystem_pattern != NULL)
        {
          g_autoptr(FoundryContext) context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
          g_autoptr(GFile) project_dir = foundry_context_dup_project_directory (context);
          g_autoptr(GPtrArray) files = NULL;
          g_autoptr(GRegex) regex = NULL;
          g_autoptr(GError) error = NULL;

          if (!(regex = g_regex_new (x_buildsystem_pattern, G_REGEX_OPTIMIZE, 0, &error)))
            {
              g_warning ("Failed to compile regex `%s` from plugin %s",
                         x_buildsystem_pattern,
                         peas_plugin_info_get_module_name (priv->plugin_info));
              return dex_future_new_for_error (g_steal_pointer (&error));
            }

          files = dex_await_boxed (foundry_file_find_regex_with_depth (project_dir, regex, 1), NULL);

          if (files && files->len > 0)
            return dex_future_new_for_string (peas_plugin_info_get_module_name (priv->plugin_info));
        }
    }

  return foundry_future_new_not_supported ();
}

static DexFuture *
foundry_build_addin_real_discover_build_system (FoundryBuildAddin *self)
{
  g_assert (FOUNDRY_IS_BUILD_ADDIN (self));

  return dex_scheduler_spawn (NULL, 0,
                              foundry_build_addin_real_discover_build_system_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static void
foundry_build_addin_finalize (GObject *object)
{
  FoundryBuildAddin *self = (FoundryBuildAddin *)object;
  FoundryBuildAddinPrivate *priv = foundry_build_addin_get_instance_private (self);

  g_weak_ref_clear (&priv->pipeline_wr);

  G_OBJECT_CLASS (foundry_build_addin_parent_class)->finalize (object);
}

static void
foundry_build_addin_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundryBuildAddin *self = FOUNDRY_BUILD_ADDIN (object);

  switch (prop_id)
    {
    case PROP_PIPELINE:
      g_value_take_object (value, foundry_build_addin_dup_pipeline (self));
      break;

    case PROP_PLUGIN_INFO:
      g_value_take_object (value, foundry_build_addin_dup_plugin_info (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_build_addin_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  FoundryBuildAddin *self = FOUNDRY_BUILD_ADDIN (object);
  FoundryBuildAddinPrivate *priv = foundry_build_addin_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PIPELINE:
      g_weak_ref_set (&priv->pipeline_wr, g_value_get_object (value));
      break;

    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_build_addin_class_init (FoundryBuildAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_build_addin_finalize;
  object_class->get_property = foundry_build_addin_get_property;
  object_class->set_property = foundry_build_addin_set_property;

  klass->discover_build_system = foundry_build_addin_real_discover_build_system;

  properties[PROP_PIPELINE] =
    g_param_spec_object ("pipeline", NULL, NULL,
                         FOUNDRY_TYPE_BUILD_PIPELINE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_build_addin_init (FoundryBuildAddin *self)
{
  FoundryBuildAddinPrivate *priv = foundry_build_addin_get_instance_private (self);

  g_weak_ref_init (&priv->pipeline_wr, NULL);
}

/**
 * foundry_build_addin_dup_pipeline:
 * @self: a [class@Foundry.BuildAddin]
 *
 * Gets the pipeline for which the addin belongs.
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.BuildPipeline
 *   or %NULL if the pipeline has been destroyed.
 */
FoundryBuildPipeline *
foundry_build_addin_dup_pipeline (FoundryBuildAddin *self)
{
  FoundryBuildAddinPrivate *priv = foundry_build_addin_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_BUILD_ADDIN (self), NULL);

  return g_weak_ref_get (&priv->pipeline_wr);
}

/**
 * foundry_build_addin_dup_plugin_info:
 * @self: a [class@Foundry.BuildAddin]
 *
 * Returns: (transfer full) (nullable): a [class@Peas.PluginInfo]
 */
PeasPluginInfo *
foundry_build_addin_dup_plugin_info (FoundryBuildAddin *self)
{
  FoundryBuildAddinPrivate *priv = foundry_build_addin_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_BUILD_ADDIN (self), NULL);

  return priv->plugin_info ? g_object_ref (priv->plugin_info) : NULL;
}

DexFuture *
_foundry_build_addin_load (FoundryBuildAddin *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_ADDIN (self));

  return FOUNDRY_BUILD_ADDIN_GET_CLASS (self)->load (self);
}

DexFuture *
_foundry_build_addin_unload (FoundryBuildAddin *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_ADDIN (self));

  return FOUNDRY_BUILD_ADDIN_GET_CLASS (self)->unload (self);
}

/**
 * foundry_build_addin_discover_build_system:
 * @self: a [class@Foundry.BuildAddin]
 *
 * Discover the build system used by the project.
 *
 * This may be called _before_ [vfunc@Foundry.BuildAddin.load] is called so
 * that addins may discover what build system should be used. If the user
 * has already specified a build system then this will not be called during
 * pipeline initialization.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   string name of the build system or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_build_addin_discover_build_system (FoundryBuildAddin *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_ADDIN (self));

  return FOUNDRY_BUILD_ADDIN_GET_CLASS (self)->discover_build_system (self);
}
