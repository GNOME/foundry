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

static char *
find_distcc_wrapper (FoundryBuildPipeline *pipeline)
{
  g_autoptr(FoundrySdk) sdk = NULL;
  g_autofree char *distcc_path = NULL;
  g_autofree char *distcc_dir = NULL;
  g_autofree char *wrapper_dir = NULL;
  g_autofree char *cc_wrapper = NULL;
  g_autofree char *cxx_wrapper = NULL;
  g_autoptr(GFile) cc_wrapper_file = NULL;
  g_autoptr(GFile) cxx_wrapper_file = NULL;
  gboolean cc_exists = FALSE;
  gboolean cxx_exists = FALSE;

  g_assert (FOUNDRY_IS_BUILD_PIPELINE (pipeline));

  if (!(distcc_path = dex_await_string (foundry_build_pipeline_contains_program (pipeline, "distcc"), NULL)))
    return NULL;

  if (!(sdk = foundry_build_pipeline_dup_sdk (pipeline)))
    return NULL;

  /* Find the wrapper directory - typically /usr/lib/distcc/bin or similar */
  distcc_dir = g_path_get_dirname (distcc_path);
  wrapper_dir = g_build_filename (distcc_dir, "..", "lib", "distcc", "bin", NULL);
  cc_wrapper = g_build_filename (wrapper_dir, "cc", NULL);
  cxx_wrapper = g_build_filename (wrapper_dir, "c++", NULL);

  /* Translate paths to accessible locations */
  if ((cc_wrapper_file = dex_await_object (foundry_sdk_translate_path (sdk, pipeline, cc_wrapper), NULL)))
    cc_exists = dex_await_boolean (dex_file_query_exists (cc_wrapper_file), NULL);

  if ((cxx_wrapper_file = dex_await_object (foundry_sdk_translate_path (sdk, pipeline, cxx_wrapper), NULL)))
    cxx_exists = dex_await_boolean (dex_file_query_exists (cxx_wrapper_file), NULL);

  /* Check if wrappers exist or try alternative location */
  if (!cc_exists || !cxx_exists)
    {
      g_free (wrapper_dir);
      g_free (cc_wrapper);
      g_free (cxx_wrapper);

      wrapper_dir = g_build_filename (distcc_dir, "..", "libexec", "distcc", NULL);
      cc_wrapper = g_build_filename (wrapper_dir, "cc", NULL);
      cxx_wrapper = g_build_filename (wrapper_dir, "c++", NULL);

      g_clear_object (&cc_wrapper_file);
      g_clear_object (&cxx_wrapper_file);

      cc_wrapper_file = dex_await_object (foundry_sdk_translate_path (sdk, pipeline, cc_wrapper), NULL);
      cxx_wrapper_file = dex_await_object (foundry_sdk_translate_path (sdk, pipeline, cxx_wrapper), NULL);

      cc_exists = FALSE;
      cxx_exists = FALSE;

      if (cc_wrapper_file != NULL)
        cc_exists = dex_await_boolean (dex_file_query_exists (cc_wrapper_file), NULL);

      if (cxx_wrapper_file != NULL)
        cxx_exists = dex_await_boolean (dex_file_query_exists (cxx_wrapper_file), NULL);
    }

  if (cc_exists && cxx_exists)
    return g_steal_pointer (&wrapper_dir);

  return NULL;
}

static DexFuture *
plugin_distcc_build_addin_load_fiber (gpointer user_data)
{
  PluginDistccBuildAddin *self = user_data;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundrySettings) distcc_settings = NULL;
  gboolean enabled;
  g_auto(GStrv) hosts = NULL;
  g_autofree char *wrapper_path = NULL;
  g_autofree char *wrapper_dir = NULL;
  g_autofree char *cc_wrapper = NULL;
  g_autofree char *cxx_wrapper = NULL;
  g_autofree char *hosts_str = NULL;

  g_assert (PLUGIN_IS_DISTCC_BUILD_ADDIN (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  pipeline = foundry_build_addin_dup_pipeline (FOUNDRY_BUILD_ADDIN (self));
  distcc_settings = foundry_context_load_settings (context, "app.devsuite.foundry.distcc", NULL);

  /* Check if distcc is enabled */
  enabled = foundry_settings_get_boolean (distcc_settings, "enabled");

  if (!enabled)
    return dex_future_new_true ();

  /* Get configuration */
  hosts = foundry_settings_get_strv (distcc_settings, "hosts");
  wrapper_path = foundry_settings_get_string (distcc_settings, "wrapper-path");

  /* Try to find distcc wrappers if not configured */
  if (foundry_str_empty0 (wrapper_path))
    wrapper_dir = find_distcc_wrapper (pipeline);
  else
    wrapper_dir = g_steal_pointer (&wrapper_path);

  if (wrapper_dir == NULL)
    {
      FOUNDRY_CONTEXTUAL_MESSAGE (self, "%s: enabled but no wrappers found", "distcc");
      return dex_future_new_true ();
    }

  /* Prepend wrapper directory to PATH so distcc wrappers are found first */
  foundry_build_pipeline_prepend_path (pipeline, wrapper_dir);

  /* Set DISTCC_HOSTS if provided */
  if (hosts != NULL && hosts[0] != NULL)
    {
      hosts_str = g_strjoinv (",", hosts);
      foundry_build_pipeline_setenv (pipeline, "DISTCC_HOSTS", hosts_str);
    }

  /* Configure for ccache compatibility */
  foundry_build_pipeline_setenv (pipeline, "CCACHE_PREFIX", "distcc");

  /* Set compiler environment variables */
  {
    g_autoptr(FoundrySdk) sdk = NULL;
    g_autoptr(GFile) cc_wrapper_file = NULL;
    g_autoptr(GFile) cxx_wrapper_file = NULL;
    gboolean cc_exists = FALSE;
    gboolean cxx_exists = FALSE;

    sdk = foundry_build_pipeline_dup_sdk (pipeline);

    cc_wrapper = g_build_filename (wrapper_dir, "cc", NULL);
    cc_wrapper_file = dex_await_object (foundry_sdk_translate_path (sdk, pipeline, cc_wrapper), NULL);

    cxx_wrapper = g_build_filename (wrapper_dir, "c++", NULL);
    cxx_wrapper_file = dex_await_object (foundry_sdk_translate_path (sdk, pipeline, cxx_wrapper), NULL);

    if (cc_wrapper_file != NULL)
      cc_exists = dex_await_boolean (dex_file_query_exists (cc_wrapper_file), NULL);

    if (cxx_wrapper_file != NULL)
      cxx_exists = dex_await_boolean (dex_file_query_exists (cxx_wrapper_file), NULL);

    if (cc_exists)
      foundry_build_pipeline_setenv (pipeline, "CC", cc_wrapper);

    if (cxx_exists)
      foundry_build_pipeline_setenv (pipeline, "CXX", cxx_wrapper);
  }

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
