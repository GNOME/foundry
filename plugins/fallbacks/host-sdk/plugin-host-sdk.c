/* plugin-host-sdk.c
 *
 * Copyright 2024-2025 Christian Hergert <chergert@redhat.com>
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
#include "foundry-shell.h"
#include "foundry-util-private.h"

#include "plugin-host-sdk.h"

struct _PluginHostSdk
{
  FoundrySdk  parent_instance;
  char       *systemd_run_path;
  guint       in_flatpak : 1;
};

typedef struct _HostSdkPrepare
{
  PluginHostSdk        *self;
  FoundryBuildPipeline *pipeline;
} HostSdkPrepare;

G_DEFINE_FINAL_TYPE (PluginHostSdk, plugin_host_sdk, FOUNDRY_TYPE_SDK)

static void
host_sdk_prepare_free (HostSdkPrepare *prepare)
{
  g_clear_object (&prepare->self);
  g_clear_object (&prepare->pipeline);
  g_free (prepare);
}

static gboolean
plugin_host_sdk_systemd_run_handler (FoundryProcessLauncher  *launcher,
                                     const char * const      *argv,
                                     const char * const      *env,
                                     const char              *cwd,
                                     FoundryUnixFDMap        *unix_fd_map,
                                     gpointer                 user_data,
                                     GError                 **error)
{
  HostSdkPrepare *prepare = user_data;
  g_autofree char *uuid = NULL;
  g_autofree char *new_path = NULL;
  g_autofree char *pipeline_prepend = NULL;
  g_autofree char *pipeline_append = NULL;
  const char *path = NULL;

  g_assert (FOUNDRY_IS_PROCESS_LAUNCHER (launcher));
  g_assert (prepare != NULL);
  g_assert (PLUGIN_IS_HOST_SDK (prepare->self));

  if (!foundry_process_launcher_merge_unix_fd_map (launcher, unix_fd_map, error))
    return FALSE;

  foundry_process_launcher_set_cwd (launcher, cwd);

  foundry_process_launcher_append_argv (launcher, prepare->self->systemd_run_path);
  foundry_process_launcher_append_argv (launcher, "--user");
  foundry_process_launcher_append_argv (launcher, "--scope");
  foundry_process_launcher_append_argv (launcher, "--collect");
  foundry_process_launcher_append_argv (launcher, "--quiet");
  foundry_process_launcher_append_argv (launcher, "--same-dir");

  uuid = g_uuid_string_random ();
  foundry_process_launcher_append_formatted (launcher, "--unit=foundry-%s.scope", uuid);

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

  if (prepare->pipeline != NULL)
    {
      pipeline_prepend = foundry_build_pipeline_dup_prepend_path (prepare->pipeline);
      pipeline_append = foundry_build_pipeline_dup_append_path (prepare->pipeline);
    }

  if (path != NULL || pipeline_prepend != NULL || pipeline_append != NULL)
    {
      g_autofree char *tmp = NULL;

      if (path == NULL)
        path = foundry_shell_get_default_path ();

      tmp = foundry_search_path_prepend (path, pipeline_prepend);
      new_path = foundry_search_path_append (tmp, pipeline_append);
      foundry_process_launcher_append_formatted (launcher, "--setenv=PATH=%s", new_path);
    }

  if (env != NULL)
    {
      for (guint i = 0; env[i]; i++)
        {
          if (!g_str_has_prefix (env[i], "PATH="))
            foundry_process_launcher_append_formatted (launcher, "--setenv=%s", env[i]);
        }
    }

  foundry_process_launcher_append_args (launcher, argv);

  return TRUE;
}

static DexFuture *
plugin_host_sdk_prepare_to_build (FoundrySdk                *sdk,
                                  FoundryBuildPipeline      *pipeline,
                                  FoundryProcessLauncher    *launcher,
                                  FoundryBuildPipelinePhase  phase)
{
  PluginHostSdk *self = (PluginHostSdk *)sdk;
  HostSdkPrepare *prepare = NULL;

  g_assert (PLUGIN_IS_HOST_SDK (self));
  g_assert (!pipeline || FOUNDRY_IS_BUILD_PIPELINE (pipeline));
  g_assert (FOUNDRY_IS_PROCESS_LAUNCHER (launcher));

  foundry_process_launcher_push_host (launcher);

  if (self->systemd_run_path != NULL)
    {
      prepare = g_new0 (HostSdkPrepare, 1);
      prepare->self = g_object_ref (self);
      if (pipeline != NULL)
        prepare->pipeline = g_object_ref (pipeline);

      foundry_process_launcher_push (launcher,
                                     plugin_host_sdk_systemd_run_handler,
                                     prepare,
                                     (GDestroyNotify) host_sdk_prepare_free);
    }

  foundry_process_launcher_add_minimal_environment (launcher);

  return dex_future_new_true ();
}

static DexFuture *
plugin_host_sdk_prepare_to_run (FoundrySdk             *sdk,
                                FoundryBuildPipeline   *pipeline,
                                FoundryProcessLauncher *launcher)
{
  PluginHostSdk *self = (PluginHostSdk *)sdk;
  HostSdkPrepare *prepare = NULL;

  g_assert (PLUGIN_IS_HOST_SDK (self));
  g_assert (!pipeline || FOUNDRY_IS_BUILD_PIPELINE (pipeline));
  g_assert (FOUNDRY_IS_PROCESS_LAUNCHER (launcher));

  foundry_process_launcher_push_host (launcher);

  if (self->systemd_run_path != NULL)
    {
      prepare = g_new0 (HostSdkPrepare, 1);
      prepare->self = g_object_ref (self);
      if (pipeline != NULL)
        prepare->pipeline = g_object_ref (pipeline);

      foundry_process_launcher_push (launcher,
                                     plugin_host_sdk_systemd_run_handler,
                                     prepare,
                                     (GDestroyNotify) host_sdk_prepare_free);
    }

  foundry_process_launcher_add_minimal_environment (launcher);

  return dex_future_new_true ();
}

static DexFuture *
plugin_host_sdk_translate_path (FoundrySdk           *sdk,
                                FoundryBuildPipeline *pipeline,
                                const char           *path)
{
  GFile *file;

  if (_foundry_in_container ())
    file = g_file_new_build_filename ("/var/run/host", path, NULL);
  else
    file = g_file_new_for_path (path);

  return dex_future_new_take_object (g_steal_pointer (&file));
}

static void
plugin_host_sdk_finalize (GObject *object)
{
  PluginHostSdk *self = (PluginHostSdk *)object;

  g_clear_pointer (&self->systemd_run_path, g_free);

  G_OBJECT_CLASS (plugin_host_sdk_parent_class)->finalize (object);
}

static void
plugin_host_sdk_class_init (PluginHostSdkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundrySdkClass *sdk_class = FOUNDRY_SDK_CLASS (klass);

  object_class->finalize = plugin_host_sdk_finalize;

  sdk_class->prepare_to_build = plugin_host_sdk_prepare_to_build;
  sdk_class->prepare_to_run = plugin_host_sdk_prepare_to_run;
  sdk_class->translate_path = plugin_host_sdk_translate_path;
}

static void
plugin_host_sdk_init (PluginHostSdk *self)
{
  self->in_flatpak = g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
}

FoundrySdk *
plugin_host_sdk_new (FoundryContext *context,
                     const char     *systemd_run_path)
{
  PluginHostSdk *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);

  self = g_object_new (PLUGIN_TYPE_HOST_SDK,
                       "context", context,
                       "id", "host",
                       "arch", foundry_get_default_arch (),
                       "name", _("My Computer"),
                       "kind", "host",
                       "installed", TRUE,
                       NULL);
  g_set_str (&self->systemd_run_path, systemd_run_path);

  return FOUNDRY_SDK (self);
}

char *
plugin_host_sdk_build_filename (PluginHostSdk  *self,
                                const char     *first_element,
                                ...)
{
  g_autofree char *joined = NULL;
  va_list args;

  va_start (args, first_element);
  joined = g_build_filename_valist (first_element, &args);
  va_end (args);

  if (self->in_flatpak)
    return g_build_filename ("/var/run/host", joined, NULL);

  if (g_path_is_absolute (joined))
    return g_steal_pointer (&joined);

  return g_build_filename ("/", joined, NULL);
}
