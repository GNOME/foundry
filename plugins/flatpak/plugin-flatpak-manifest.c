/* plugin-flatpak-manifest.c
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

#include "plugin-flatpak.h"
#include "plugin-flatpak-manifest-private.h"
#include "plugin-flatpak-json-manifest.h"

#define PRIORITY_DEFAULT     10000
#define PRIORITY_MAYBE_DEVEL 11000
#define PRIORITY_DEVEL       12000

enum {
  PROP_0,
  PROP_FILE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (PluginFlatpakManifest, plugin_flatpak_manifest, FOUNDRY_TYPE_CONFIG)

static GParamSpec *properties[N_PROPS];

static gboolean
plugin_flatpak_manifest_can_default (FoundryConfig *config,
                                     guint         *priority)
{
  PluginFlatpakManifest *self = (PluginFlatpakManifest *)config;
  g_autofree char *name = NULL;

  g_assert (PLUGIN_IS_FLATPAK_MANIFEST (self));
  g_assert (priority != NULL);

  if (!(name = g_file_get_basename (self->file)))
    return FALSE;

  *priority = PRIORITY_DEFAULT;

  if (strstr (name, "Devel") != NULL)
    *priority = PRIORITY_MAYBE_DEVEL;

  if (strstr (name, ".Devel.") != NULL)
    *priority = PRIORITY_DEVEL;

  return TRUE;
}

static DexFuture *
plugin_flatpak_manifest_resolve_sdk (FoundryConfig *config,
                                     FoundryDevice *device)
{
  PluginFlatpakManifest *self = (PluginFlatpakManifest *)config;
  g_autoptr(FoundrySdkManager) sdk_manager = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundrySdk) sdk = NULL;
  g_autoptr(FoundryTriplet) triplet = NULL;
  g_autofree char *id = NULL;
  const char *arch;
  const char *runtime;

  g_assert (PLUGIN_IS_FLATPAK_MANIFEST (self));
  g_assert (FOUNDRY_IS_DEVICE (device));

  if (self->runtime == NULL ||
      self->runtime_version == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "Manifest is missing information required to resolve SDK");

  runtime = self->sdk ? self->sdk : self->runtime;
  triplet = foundry_device_dup_triplet (device);
  arch = foundry_triplet_get_arch (triplet);
  id = g_strdup_printf ("%s/%s/%s", runtime, arch, self->runtime_version);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  sdk_manager = foundry_context_dup_sdk_manager (context);

  return foundry_sdk_manager_find_by_id (sdk_manager, id);
}

static char *
plugin_flatpak_manifest_dup_build_system (FoundryConfig *config)
{
  return g_strdup (PLUGIN_FLATPAK_MANIFEST (config)->build_system);
}

static void
plugin_flatpak_manifest_constructed (GObject *object)
{
  PluginFlatpakManifest *self = (PluginFlatpakManifest *)object;

  G_OBJECT_CLASS (plugin_flatpak_manifest_parent_class)->constructed (object);

  if (self->file != NULL)
    {
      g_autofree char *name = g_file_get_basename (self->file);

      foundry_config_set_id (FOUNDRY_CONFIG (self), name);
      foundry_config_set_name (FOUNDRY_CONFIG (self), name);
    }
}

static void
plugin_flatpak_manifest_finalize (GObject *object)
{
  PluginFlatpakManifest *self = (PluginFlatpakManifest *)object;

  g_clear_object (&self->file);

  g_clear_pointer (&self->build_system, g_free);
  g_clear_pointer (&self->command, g_free);
  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->primary_module_name, g_free);
  g_clear_pointer (&self->runtime, g_free);
  g_clear_pointer (&self->runtime_version, g_free);
  g_clear_pointer (&self->sdk, g_free);

  G_OBJECT_CLASS (plugin_flatpak_manifest_parent_class)->finalize (object);
}

static void
plugin_flatpak_manifest_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  PluginFlatpakManifest *self = PLUGIN_FLATPAK_MANIFEST (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_take_object (value, plugin_flatpak_manifest_dup_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_manifest_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  PluginFlatpakManifest *self = PLUGIN_FLATPAK_MANIFEST (object);

  switch (prop_id)
    {
    case PROP_FILE:
      self->file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_manifest_class_init (PluginFlatpakManifestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryConfigClass *config_class = FOUNDRY_CONFIG_CLASS (klass);

  object_class->constructed = plugin_flatpak_manifest_constructed;
  object_class->finalize = plugin_flatpak_manifest_finalize;
  object_class->get_property = plugin_flatpak_manifest_get_property;
  object_class->set_property = plugin_flatpak_manifest_set_property;

  config_class->can_default = plugin_flatpak_manifest_can_default;
  config_class->resolve_sdk = plugin_flatpak_manifest_resolve_sdk;
  config_class->dup_build_system = plugin_flatpak_manifest_dup_build_system;

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_flatpak_manifest_init (PluginFlatpakManifest *self)
{
}

/**
 * plugin_flatpak_manifest_dup_file:
 * @self: a #PluginFlatpakManifest
 *
 * Gets the underlying [iface@Gio.File].
 *
 * Returns: (transfer full): a [iface@Gio.File]
 */
GFile *
plugin_flatpak_manifest_dup_file (PluginFlatpakManifest *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_MANIFEST (self), NULL);

  return g_object_ref (self->file);
}

static DexFuture *
plugin_flatpak_manifest_resolve_fiber (gpointer user_data)
{
  PluginFlatpakManifest *self = user_data;
  g_autoptr(FoundrySdkManager) sdk_manager = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GPtrArray) installations = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (PLUGIN_IS_FLATPAK_MANIFEST (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  sdk_manager = foundry_context_dup_sdk_manager (context);

  /* First make sure SDK manager has loaded */
  dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (sdk_manager)), NULL);

  /* Get installations */
  if (!(installations = dex_await_boxed (plugin_flatpak_load_installations (), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  /* Try to find matching Ref, even if not installed */
  if (self->runtime && self->runtime_version)
    {
      g_autoptr(FoundrySdk) sdk = NULL;
      g_autofree char *ref_str = g_strdup_printf ("%s/%s/%s",
                                                  self->runtime,
                                                  flatpak_get_default_arch (),
                                                  self->runtime_version);

      if ((sdk = dex_await_object (foundry_sdk_manager_find_by_id (sdk_manager, ref_str), NULL)))
        {
          self->sdk_for_run = g_object_ref (sdk);
        }
      else
        {
          for (guint i = 0; i < installations->len; i++)
            {
              FlatpakInstallation *installation = g_ptr_array_index (installations, i);
              g_autoptr(FlatpakRef) ref = NULL;

              if ((ref = dex_await_object (plugin_flatpak_find_ref (context, installation, self->runtime, NULL, self->runtime_version), NULL)))
                {
                  /* not installed but found */
                }
            }
        }
    }

  return dex_future_new_true ();
}

DexFuture *
_plugin_flatpak_manifest_resolve (PluginFlatpakManifest *self)
{
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_MANIFEST (self));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_flatpak_manifest_resolve_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

void
_plugin_flatpak_manifest_set_id (PluginFlatpakManifest *self,
                                 const char            *id)
{
  g_set_str (&self->id, id);
}

void
_plugin_flatpak_manifest_set_runtime (PluginFlatpakManifest *self,
                                      const char            *runtime)
{
  g_set_str (&self->runtime, runtime);
}

void
_plugin_flatpak_manifest_set_runtime_version (PluginFlatpakManifest *self,
                                              const char            *runtime_version)
{
  g_set_str (&self->runtime_version, runtime_version);
}

void
_plugin_flatpak_manifest_set_sdk (PluginFlatpakManifest *self,
                                  const char            *sdk)
{
  g_set_str (&self->sdk, sdk);
}

void
_plugin_flatpak_manifest_set_command (PluginFlatpakManifest *self,
                                      const char            *command)
{
  g_set_str (&self->command, command);
}

void
_plugin_flatpak_manifest_set_build_system (PluginFlatpakManifest *self,
                                           const char            *build_system)
{
  if (g_strcmp0 (build_system, "simple") == 0)
    build_system = "flatpak-simple";

  g_set_str (&self->build_system, build_system);
}

char *
plugin_flatpak_manifest_dup_id (PluginFlatpakManifest *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_MANIFEST (self), NULL);

  return g_strdup (self->id);
}

char *
plugin_flatpak_manifest_dup_runtime (PluginFlatpakManifest *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_MANIFEST (self), NULL);

  return g_strdup (self->runtime);
}

char *
plugin_flatpak_manifest_dup_runtime_version (PluginFlatpakManifest *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_MANIFEST (self), NULL);

  return g_strdup (self->runtime_version);
}

char *
plugin_flatpak_manifest_dup_sdk (PluginFlatpakManifest *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_MANIFEST (self), NULL);

  return g_strdup (self->sdk);
}

char *
plugin_flatpak_manifest_dup_primary_module_name (PluginFlatpakManifest *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_MANIFEST (self), NULL);

  return g_strdup (self->primary_module_name);
}

char **
_plugin_flatpak_manifest_get_commands (PluginFlatpakManifest *manifest)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_MANIFEST (manifest), NULL);

  if (PLUGIN_FLATPAK_MANIFEST_GET_CLASS (manifest)->get_commands)
    return PLUGIN_FLATPAK_MANIFEST_GET_CLASS (manifest)->get_commands (manifest);

  return NULL;
}
