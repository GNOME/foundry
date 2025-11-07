/* plugin-just-build-addin.c
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

#include <glib/gi18n-lib.h>

#include "plugin-just-build-addin.h"
#include "plugin-just-build-stage.h"

struct _PluginJustBuildAddin
{
  FoundryBuildAddin  parent_instance;
  FoundryBuildStage *build;
};

G_DEFINE_FINAL_TYPE (PluginJustBuildAddin, plugin_just_build_addin, FOUNDRY_TYPE_BUILD_ADDIN)

static DexFuture *
plugin_just_build_addin_load (FoundryBuildAddin *build_addin)
{
  PluginJustBuildAddin *self = (PluginJustBuildAddin *)build_addin;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autofree char *build_system = NULL;

  g_assert (PLUGIN_IS_JUST_BUILD_ADDIN (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  pipeline = foundry_build_addin_dup_pipeline (build_addin);
  build_system = foundry_build_pipeline_dup_build_system (pipeline);

  if (foundry_str_equal0 (build_system, "just"))
    {
      g_autofree char *just = g_strdup ("just"); /* TODO: Find just */
      g_autofree char *builddir = foundry_build_pipeline_dup_builddir (pipeline);

      self->build = g_object_new (PLUGIN_TYPE_JUST_BUILD_STAGE,
                                  "context", context,
                                  "just", just,
                                  "kind", "just",
                                  "title", _("Build Just Project"),
                                  NULL);
      foundry_build_pipeline_add_stage (pipeline, self->build);
    }

  return dex_future_new_true ();
}

static DexFuture *
plugin_just_build_addin_unload (FoundryBuildAddin *build_addin)
{
  PluginJustBuildAddin *self = (PluginJustBuildAddin *)build_addin;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;

  g_assert (PLUGIN_IS_JUST_BUILD_ADDIN (self));

  pipeline = foundry_build_addin_dup_pipeline (build_addin);

  foundry_clear_build_stage (&self->build, pipeline);

  return dex_future_new_true ();
}

static void
plugin_just_build_addin_class_init (PluginJustBuildAddinClass *klass)
{
  FoundryBuildAddinClass *build_addin_class = FOUNDRY_BUILD_ADDIN_CLASS (klass);

  build_addin_class->load = plugin_just_build_addin_load;
  build_addin_class->unload = plugin_just_build_addin_unload;
}

static void
plugin_just_build_addin_init (PluginJustBuildAddin *self)
{
}
