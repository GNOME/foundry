/* plugin-no-sdk.c
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

#include "foundry-build-pipeline.h"
#include "foundry-search-path.h"

#include "plugin-no-sdk.h"

struct _PluginNoSdk
{
  FoundrySdk parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginNoSdk, plugin_no_sdk, FOUNDRY_TYPE_SDK)

static gboolean
plugin_no_sdk_prepare_cb (FoundryProcessLauncher  *launcher,
                          const char * const      *argv,
                          const char * const      *env,
                          const char              *cwd,
                          FoundryUnixFDMap        *unix_fd_map,
                          gpointer                 user_data,
                          GError                 **error)
{
  FoundryBuildPipeline *pipeline = user_data;
  g_autofree char *new_path = NULL;
  g_autofree char *pipeline_prepend = NULL;
  g_autofree char *pipeline_append = NULL;
  const char *path = NULL;

  g_assert (FOUNDRY_IS_PROCESS_LAUNCHER (launcher));

  if (!foundry_process_launcher_merge_unix_fd_map (launcher, unix_fd_map, error))
    return FALSE;

  foundry_process_launcher_set_cwd (launcher, cwd);

  /* Handle PATH specially to apply pipeline prepend/append paths */
  if (env != NULL)
    {
      for (guint i = 0; env[i]; i++)
        {
          if (g_str_has_prefix (env[i], "PATH="))
            {
              path = env[i] + 5; /* Skip "PATH=" */
              break;
            }
        }
    }

  if (pipeline != NULL)
    {
      pipeline_prepend = foundry_build_pipeline_dup_prepend_path (pipeline);
      pipeline_append = foundry_build_pipeline_dup_append_path (pipeline);
    }

  if (path != NULL || pipeline_prepend != NULL || pipeline_append != NULL)
    {
      g_autofree char *tmp = NULL;

      if (path == NULL)
        path = g_getenv ("PATH");

      tmp = foundry_search_path_prepend (path, pipeline_prepend);
      new_path = foundry_search_path_append (tmp, pipeline_append);
      foundry_process_launcher_setenv (launcher, "PATH", new_path);
    }

  /* Set other environment variables */
  if (env != NULL)
    {
      for (guint i = 0; env[i]; i++)
        {
          if (!g_str_has_prefix (env[i], "PATH="))
            {
              const char *eq = strchr (env[i], '=');

              if (eq != NULL)
                {
                  g_autofree char *key = g_strndup (env[i], eq - env[i]);
                  foundry_process_launcher_setenv (launcher, key, eq + 1);
                }
            }
        }
    }

  foundry_process_launcher_append_args (launcher, argv);

  return TRUE;
}

static DexFuture *
plugin_no_sdk_prepare_to_build (FoundrySdk                *sdk,
                                FoundryBuildPipeline      *pipeline,
                                FoundryProcessLauncher    *launcher,
                                FoundryBuildPipelinePhase  phase)
{
  g_assert (PLUGIN_IS_NO_SDK (sdk));
  g_assert (!pipeline || FOUNDRY_IS_BUILD_PIPELINE (pipeline));
  g_assert (FOUNDRY_IS_PROCESS_LAUNCHER (launcher));

  foundry_process_launcher_push (launcher,
                                 plugin_no_sdk_prepare_cb,
                                 pipeline ? g_object_ref (pipeline) : NULL,
                                 g_object_unref);

  return dex_future_new_true ();
}

static DexFuture *
plugin_no_sdk_prepare_to_run (FoundrySdk             *sdk,
                              FoundryBuildPipeline   *pipeline,
                              FoundryProcessLauncher *launcher)
{
  g_assert (PLUGIN_IS_NO_SDK (sdk));
  g_assert (!pipeline || FOUNDRY_IS_BUILD_PIPELINE (pipeline));
  g_assert (FOUNDRY_IS_PROCESS_LAUNCHER (launcher));

  foundry_process_launcher_push (launcher,
                                 plugin_no_sdk_prepare_cb,
                                 pipeline ? g_object_ref (pipeline) : NULL,
                                 g_object_unref);

  return dex_future_new_true ();
}

static DexFuture *
plugin_no_sdk_translate_path (FoundrySdk           *sdk,
                              FoundryBuildPipeline *pipeline,
                              const char           *path)
{
  return dex_future_new_take_object (g_file_new_for_path (path));
}

static void
plugin_no_sdk_class_init (PluginNoSdkClass *klass)
{
  FoundrySdkClass *sdk_class = FOUNDRY_SDK_CLASS (klass);

  sdk_class->prepare_to_build = plugin_no_sdk_prepare_to_build;
  sdk_class->prepare_to_run = plugin_no_sdk_prepare_to_run;
  sdk_class->translate_path = plugin_no_sdk_translate_path;
}

static void
plugin_no_sdk_init (PluginNoSdk *self)
{
}

FoundrySdk *
plugin_no_sdk_new (FoundryContext *context)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);

  return g_object_new (PLUGIN_TYPE_NO_SDK,
                       "context", context,
                       "id", "no",
                       "arch", foundry_get_default_arch (),
                       "name", _("No SDK"),
                       "installed", TRUE,
                       "kind", "host",
                       NULL);
}
