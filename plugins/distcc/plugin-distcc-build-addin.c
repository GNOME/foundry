/* plugin-distcc-build-addin.c
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
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include "plugin-distcc-build-addin.h"

struct _PluginDistccBuildAddin
{
  FoundryBuildAddin parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginDistccBuildAddin, plugin_distcc_build_addin, FOUNDRY_TYPE_BUILD_ADDIN)

static DexFuture *
plugin_distcc_build_addin_load_fiber (gpointer user_data)
{
  PluginDistccBuildAddin *self = user_data;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundrySettings) distcc_settings = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autofree char *wrapper_path = NULL;
  g_autofree char *hosts_str = NULL;
  g_auto(GStrv) hosts = NULL;
  gboolean enabled;

  g_assert (PLUGIN_IS_DISTCC_BUILD_ADDIN (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  pipeline = foundry_build_addin_dup_pipeline (FOUNDRY_BUILD_ADDIN (self));
  distcc_settings = foundry_context_load_settings (context, "org.gnome.foundry.distcc", NULL);
  enabled = foundry_settings_get_boolean (distcc_settings, "enabled");

  if (!enabled)
    return dex_future_new_true ();

  hosts = foundry_settings_get_strv (distcc_settings, "hosts");
  wrapper_path = foundry_settings_get_string (distcc_settings, "wrapper-path");

  if (foundry_str_empty0 (wrapper_path))
    {
      g_autofree char *distcc_path = dex_await_string (foundry_build_pipeline_contains_program (pipeline, "distcc"), NULL);

      if (distcc_path != NULL)
        g_set_str (&wrapper_path, distcc_path);
    }

  if (foundry_str_empty0 (wrapper_path))
    {
      FOUNDRY_CONTEXTUAL_MESSAGE (self, "%s: enabled but distcc not found", "distcc");
      return dex_future_new_true ();
    }

  if (hosts != NULL && hosts[0] != NULL)
    {
      hosts_str = g_strjoinv (",", hosts);
      foundry_build_pipeline_setenv (pipeline, "DISTCC_HOSTS", hosts_str);
    }

  foundry_build_pipeline_setenv (pipeline, "CCACHE_PREFIX", wrapper_path);

  return dex_future_new_true ();
}

static DexFuture *
plugin_distcc_build_addin_load (FoundryBuildAddin *addin)
{
  g_assert (PLUGIN_IS_DISTCC_BUILD_ADDIN (addin));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_distcc_build_addin_load_fiber,
                              g_object_ref (addin),
                              g_object_unref);
}

static DexFuture *
plugin_distcc_build_addin_unload (FoundryBuildAddin *addin)
{
  return dex_future_new_true ();
}

static void
plugin_distcc_build_addin_class_init (PluginDistccBuildAddinClass *klass)
{
  FoundryBuildAddinClass *build_addin_class = FOUNDRY_BUILD_ADDIN_CLASS (klass);

  build_addin_class->load = plugin_distcc_build_addin_load;
  build_addin_class->unload = plugin_distcc_build_addin_unload;
}

static void
plugin_distcc_build_addin_init (PluginDistccBuildAddin *self)
{
}
