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

struct _PluginFlatpakBuildAddin
{
  FoundryBuildAddin parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginFlatpakBuildAddin, plugin_flatpak_build_addin, FOUNDRY_TYPE_BUILD_ADDIN)

static DexFuture *
plugin_flatpak_build_addin_load (FoundryBuildAddin *addin)
{
  PluginFlatpakBuildAddin *self = (PluginFlatpakBuildAddin *)addin;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryConfig) config = NULL;

  g_assert (PLUGIN_IS_FLATPAK_BUILD_ADDIN (self));

  pipeline = foundry_build_addin_dup_pipeline (addin);
  config = foundry_build_pipeline_dup_config (pipeline);

  if (PLUGIN_IS_FLATPAK_MANIFEST (config))
    {
    }

  return dex_future_new_true ();
}

static DexFuture *
plugin_flatpak_build_addin_unload (FoundryBuildAddin *addin)
{
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
