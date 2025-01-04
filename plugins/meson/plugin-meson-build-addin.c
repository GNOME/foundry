/* plugin-meson-build-addin.c
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

#include <glib/gi18n-lib.h>

#include "plugin-meson-build-addin.h"
#include "plugin-meson-build-stage.h"

struct _PluginMesonBuildAddin
{
  FoundryBuildAddin  parent_instance;
  FoundryBuildStage *build;
};

G_DEFINE_FINAL_TYPE (PluginMesonBuildAddin, plugin_meson_build_addin, FOUNDRY_TYPE_BUILD_ADDIN)

static DexFuture *
plugin_meson_build_addin_load (FoundryBuildAddin *build_addin)
{
  PluginMesonBuildAddin *self = (PluginMesonBuildAddin *)build_addin;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autofree char *build_system = NULL;

  g_assert (PLUGIN_IS_MESON_BUILD_ADDIN (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  build_system = foundry_context_dup_build_system (context);
  pipeline = foundry_build_addin_dup_pipeline (build_addin);

  if (foundry_str_equal0 (build_system, "meson"))
    {
      g_autofree char *meson = g_strdup ("meson"); /* TODO: Find meson */
      g_autofree char *ninja = g_strdup ("ninja"); /* TODO: Find ninja */
      g_autofree char *builddir = foundry_build_pipeline_dup_builddir (pipeline);

      /* TODO: Add build stages */

      self->build = g_object_new (PLUGIN_TYPE_MESON_BUILD_STAGE,
                                  "builddir", builddir,
                                  "context", context,
                                  "meson", meson,
                                  "ninja", ninja,
                                  "kind", "meson",
                                  "title", _("Build Meson Project"),
                                  NULL);
      foundry_build_pipeline_add_stage (pipeline, self->build);
    }

  return dex_future_new_true ();
}

static DexFuture *
plugin_meson_build_addin_unload (FoundryBuildAddin *build_addin)
{
  PluginMesonBuildAddin *self = (PluginMesonBuildAddin *)build_addin;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(GPtrArray) stages = NULL;

  g_assert (PLUGIN_IS_MESON_BUILD_ADDIN (self));

  pipeline = foundry_build_addin_dup_pipeline (build_addin);
  stages = g_ptr_array_new_with_free_func (g_object_unref);

  if (self->build != NULL)
    g_ptr_array_add (stages, g_steal_pointer (&self->build));

  for (guint i = 0; i < stages->len; i++)
    foundry_build_pipeline_remove_stage (pipeline, g_ptr_array_index (stages, i));

  return dex_future_new_true ();
}

static void
plugin_meson_build_addin_finalize (GObject *object)
{
  G_OBJECT_CLASS (plugin_meson_build_addin_parent_class)->finalize (object);
}

static void
plugin_meson_build_addin_class_init (PluginMesonBuildAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryBuildAddinClass *build_addin_class = FOUNDRY_BUILD_ADDIN_CLASS (klass);

  object_class->finalize = plugin_meson_build_addin_finalize;

  build_addin_class->load = plugin_meson_build_addin_load;
  build_addin_class->unload = plugin_meson_build_addin_unload;
}

static void
plugin_meson_build_addin_init (PluginMesonBuildAddin *self)
{
}
