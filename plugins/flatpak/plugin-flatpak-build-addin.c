/* plugin-flatpak-build-addin.c
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

#include "plugin-flatpak-build-addin.h"
#include "plugin-flatpak-manifest.h"
#include "plugin-flatpak-prepare-stage.h"
#include "plugin-flatpak-util.h"

struct _PluginFlatpakBuildAddin
{
  FoundryBuildAddin  parent_instance;
  FoundryBuildStage *prepare;
};

G_DEFINE_FINAL_TYPE (PluginFlatpakBuildAddin, plugin_flatpak_build_addin, FOUNDRY_TYPE_BUILD_ADDIN)

static DexFuture *
plugin_flatpak_build_addin_load (FoundryBuildAddin *addin)
{
  PluginFlatpakBuildAddin *self = (PluginFlatpakBuildAddin *)addin;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryConfig) config = NULL;

  g_assert (PLUGIN_IS_FLATPAK_BUILD_ADDIN (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (addin));
  pipeline = foundry_build_addin_dup_pipeline (addin);
  config = foundry_build_pipeline_dup_config (pipeline);

  if (PLUGIN_IS_FLATPAK_MANIFEST (config))
    {
      g_autofree char *repo_dir = plugin_flatpak_get_repo_dir (context);
      g_autofree char *staging_dir = plugin_flatpak_get_staging_dir (pipeline);

      self->prepare = plugin_flatpak_prepare_stage_new (repo_dir, staging_dir);
      foundry_build_pipeline_add_stage (pipeline, self->prepare);
    }

  return dex_future_new_true ();
}

static DexFuture *
plugin_flatpak_build_addin_unload (FoundryBuildAddin *addin)
{
  PluginFlatpakBuildAddin *self = (PluginFlatpakBuildAddin *)addin;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(GPtrArray) stages = NULL;

  g_assert (PLUGIN_IS_FLATPAK_BUILD_ADDIN (self));

  pipeline = foundry_build_addin_dup_pipeline (addin);
  stages = g_ptr_array_new_with_free_func (g_object_unref);

  if (self->prepare != NULL)
    g_ptr_array_add (stages, g_steal_pointer (&self->prepare));

  for (guint i = 0; i < stages->len; i++)
    foundry_build_pipeline_remove_stage (pipeline, g_ptr_array_index (stages, i));

  return dex_future_new_true ();
}

static void
plugin_flatpak_build_addin_class_init (PluginFlatpakBuildAddinClass *klass)
{
  FoundryBuildAddinClass *build_addin_class = FOUNDRY_BUILD_ADDIN_CLASS (klass);

  build_addin_class->load = plugin_flatpak_build_addin_load;
  build_addin_class->unload = plugin_flatpak_build_addin_unload;
}

static void
plugin_flatpak_build_addin_init (PluginFlatpakBuildAddin *self)
{
}
