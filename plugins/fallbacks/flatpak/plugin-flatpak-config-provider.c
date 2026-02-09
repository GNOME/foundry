/* plugin-flatpak-config-provider.c
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

#include <stdio.h>
#include <string.h>

#include "plugin-flatpak-config-provider.h"
#include "plugin-flatpak-config.h"

#define DISCOVERY_MAX_DEPTH 3

struct _PluginFlatpakConfigProvider
{
  FoundryConfigProvider parent_instnace;
};

G_DEFINE_FINAL_TYPE (PluginFlatpakConfigProvider, plugin_flatpak_config_provider, FOUNDRY_TYPE_CONFIG_PROVIDER)

static GRegex *filename_regex;

static gboolean host_version_valid;
static int host_version_major;
static int host_version_minor;
static int host_version_micro;

static gboolean
parse_flatpak_version_string (const char *output,
                              int        *out_major,
                              int        *out_minor,
                              int        *out_micro)
{
  const char *p;
  int major, minor, micro;

  g_assert (out_major != NULL);
  g_assert (out_minor != NULL);
  g_assert (out_micro != NULL);

  if (output == NULL)
    return FALSE;

  /* Skip until first digit (e.g. "Flatpak 1.14.2" or "flatpak 0.16.2") */
  for (p = output; *p != '\0'; p++)
    {
      if (g_ascii_isdigit (*p))
        break;
    }

  if (*p == '\0')
    return FALSE;

  if (sscanf (p, "%d.%d.%d", &major, &minor, &micro) != 3)
    return FALSE;

  *out_major = major;
  *out_minor = minor;
  *out_micro = micro;

  return TRUE;
}

static gboolean
plugin_flatpak_fetch_host_version (GError **error)
{
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autofree char *stdout_buf = NULL;

  launcher = foundry_process_launcher_new ();
  foundry_process_launcher_push_host (launcher);
  foundry_process_launcher_append_args (launcher,
                                        FOUNDRY_STRV_INIT ("flatpak", "--version"));

  if (!(subprocess = foundry_process_launcher_spawn_with_flags (launcher,
                                                                G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                                                                error)))
    return FALSE;

  if (!(stdout_buf = dex_await_string (foundry_subprocess_communicate_utf8 (subprocess, NULL), error)))
    return FALSE;

  if (!parse_flatpak_version_string (stdout_buf,
                                     &host_version_major,
                                     &host_version_minor,
                                     &host_version_micro))
    return FALSE;

  host_version_valid = TRUE;

  return TRUE;
}

gboolean
plugin_flatpak_get_host_version (int *out_major,
                                 int *out_minor,
                                 int *out_micro)
{
  g_return_val_if_fail (out_major != NULL, FALSE);
  g_return_val_if_fail (out_minor != NULL, FALSE);
  g_return_val_if_fail (out_micro != NULL, FALSE);

  *out_major = host_version_major;
  *out_minor = host_version_minor;
  *out_micro = host_version_micro;

  return host_version_valid;
}

static DexFuture *
plugin_flatpak_config_provider_load_fiber (gpointer user_data)
{
  PluginFlatpakConfigProvider *self = user_data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GPtrArray) matching = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_dir = NULL;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (PLUGIN_IS_FLATPAK_CONFIG_PROVIDER (self));

  {
    g_autoptr(GError) version_error = NULL;

    /* Probe host flatpak version (e.g. for font remapping quirks in aux) */
    if (!plugin_flatpak_fetch_host_version (&version_error))
      FOUNDRY_CONTEXTUAL_DEBUG (self,
                                "Could not get host flatpak version: %s",
                                version_error ? version_error->message : "unknown");
  }

  if (!(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))) ||
      !(project_dir = foundry_context_dup_project_directory (context)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_CANCELLED,
                                  "Operation cancelled");

  /* First find all the files that match potential flatpak manifests */
  if (!(matching = dex_await_boxed (foundry_file_find_regex_with_depth (project_dir,
                                                                        filename_regex,
                                                                        DISCOVERY_MAX_DEPTH),
                                    &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (guint i = 0; i < matching->len; i++)
    {
      g_autoptr(FoundryFlatpakManifestLoader) loader = NULL;
      g_autoptr(FoundryFlatpakManifest) manifest = NULL;
      g_autoptr(PluginFlatpakConfig) config = NULL;
      g_autoptr(GError) manifest_error = NULL;
      GFile *match = g_ptr_array_index (matching, i);

      loader = foundry_flatpak_manifest_loader_new (match);

      if (!(manifest = dex_await_object (foundry_flatpak_manifest_loader_load (loader), &manifest_error)))
        {
          FOUNDRY_CONTEXTUAL_DEBUG (self,
                                    "Ignoring file \"%s\" because error: %s",
                                    g_file_peek_path (match),
                                    manifest_error->message);
          continue;
        }

      config = plugin_flatpak_config_new (context, manifest, match);
      foundry_config_provider_config_added (FOUNDRY_CONFIG_PROVIDER (self),
                                            FOUNDRY_CONFIG (config));
    }

  return dex_future_new_true ();
}


static DexFuture *
plugin_flatpak_config_provider_load (FoundryConfigProvider *provider)
{
  PluginFlatpakConfigProvider *self = (PluginFlatpakConfigProvider *)provider;
  DexFuture *future;

  FOUNDRY_ENTRY;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (PLUGIN_IS_FLATPAK_CONFIG_PROVIDER (provider));

  future = dex_scheduler_spawn (NULL, 0,
                                plugin_flatpak_config_provider_load_fiber,
                                g_object_ref (self),
                                g_object_unref);

  FOUNDRY_RETURN (future);
}

static void
plugin_flatpak_config_provider_class_init (PluginFlatpakConfigProviderClass *klass)
{
  FoundryConfigProviderClass *config_provider_class = FOUNDRY_CONFIG_PROVIDER_CLASS (klass);
  g_autoptr(GError) error = NULL;

  config_provider_class->load = plugin_flatpak_config_provider_load;

  /* Something that looks like an application ID with a json, yml, or yaml
   * filename suffix. We try to encode some basic rules of the application
   * id to reduce the chances we get something that cannot match.
   */
  filename_regex = g_regex_new ("([A-Za-z][A-Za-z0-9\\-_]*)(\\.([A-Za-z][A-Za-z0-9\\-_]*))+\\.(json|yml|yaml)",
                                G_REGEX_OPTIMIZE,
                                G_REGEX_MATCH_DEFAULT,
                                &error);
  g_assert_no_error (error);
}

static void
plugin_flatpak_config_provider_init (PluginFlatpakConfigProvider *self)
{
}
