/* plugin-flatpak-config.c
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
#include "plugin-flatpak-config-private.h"
#include "plugin-flatpak-json-manifest.h"

#define PRIORITY_DEFAULT     10000
#define PRIORITY_MAYBE_DEVEL 11000
#define PRIORITY_DEVEL       12000

enum {
  PROP_0,
  PROP_FILE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (PluginFlatpakConfig, plugin_flatpak_config, FOUNDRY_TYPE_CONFIG)

static GParamSpec *properties[N_PROPS];

static char **
plugin_flatpak_config_dup_config_opts (FoundryConfig *config)
{
  PluginFlatpakConfig *self = (PluginFlatpakConfig *)config;
  g_autoptr(GStrvBuilder) builder = NULL;
  g_autofree char *buildsystem = NULL;

  g_assert (PLUGIN_IS_FLATPAK_CONFIG (self));

  buildsystem = foundry_config_dup_build_system (config);
  builder = g_strv_builder_new ();

  if (foundry_str_equal0 (buildsystem, "meson"))
    {
      g_strv_builder_add (builder, "--prefix=/app");
      g_strv_builder_add (builder, "--libdir=lib");
    }
  else if (foundry_str_equal0 (buildsystem, "cmake-ninja") ||
           foundry_str_equal0 (buildsystem, "cmake"))
    {
      g_strv_builder_add (builder, "-DCMAKE_INSTALL_LIBDIR:PATH=lib");
    }

  return g_strv_builder_end (builder);
}

static gboolean
plugin_flatpak_config_can_default (FoundryConfig *config,
                                   guint         *priority)
{
  PluginFlatpakConfig *self = (PluginFlatpakConfig *)config;
  g_autofree char *name = NULL;

  g_assert (PLUGIN_IS_FLATPAK_CONFIG (self));
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
plugin_flatpak_config_resolve_sdk_fiber (gpointer data)
{
  FoundryPair *pair = data;
  PluginFlatpakConfig *self = PLUGIN_FLATPAK_CONFIG (pair->first);
  FoundryDevice *device = FOUNDRY_DEVICE (pair->second);
  g_autoptr(FoundrySdkManager) sdk_manager = NULL;
  g_autoptr(FoundryDeviceInfo) device_info = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryTriplet) triplet = NULL;
  g_autoptr(FoundrySdk) sdk = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *id = NULL;
  const char *arch;
  const char *runtime;

  g_assert (PLUGIN_IS_FLATPAK_CONFIG (self));
  g_assert (FOUNDRY_IS_DEVICE (device));

  if (self->runtime == NULL ||
      self->runtime_version == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "Manifest is missing information required to resolve SDK");

  runtime = self->sdk ? self->sdk : self->runtime;

  if (!(device_info = dex_await_object (foundry_device_load_info (device), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  triplet = foundry_device_info_dup_triplet (device_info);
  arch = foundry_triplet_get_arch (triplet);
  id = g_strdup_printf ("%s/%s/%s", runtime, arch, self->runtime_version);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  sdk_manager = foundry_context_dup_sdk_manager (context);

  return foundry_sdk_manager_find_by_id (sdk_manager, id);
}

static DexFuture *
plugin_flatpak_config_resolve_sdk (FoundryConfig *config,
                                   FoundryDevice *device)
{
  PluginFlatpakConfig *self = (PluginFlatpakConfig *)config;

  g_assert (PLUGIN_IS_FLATPAK_CONFIG (self));
  g_assert (FOUNDRY_IS_DEVICE (device));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_flatpak_config_resolve_sdk_fiber,
                              foundry_pair_new (self, device),
                              (GDestroyNotify) foundry_pair_free);
}

static char *
plugin_flatpak_config_dup_build_system (FoundryConfig *config)
{
  return g_strdup (PLUGIN_FLATPAK_CONFIG (config)->build_system);
}

static FoundryCommand *
plugin_flatpak_config_dup_default_command (FoundryConfig *config)
{
  PluginFlatpakConfig *self = (PluginFlatpakConfig *)config;
  g_autoptr(FoundryCommand) command = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GStrvBuilder) argv_builder = NULL;
  g_auto(GStrv) argv = NULL;

  g_assert (PLUGIN_IS_FLATPAK_CONFIG (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  argv_builder = g_strv_builder_new ();
  g_strv_builder_add (argv_builder, self->command);
  if (self->x_run_args != NULL)
    g_strv_builder_addv (argv_builder, (const char **)self->x_run_args);
  argv = g_strv_builder_end (argv_builder);

  command = foundry_command_new (context);
  foundry_command_set_argv (command, (const char * const *)argv);

  /* TODO: Setup environment for various aux components */

  return g_steal_pointer (&command);
}

static void
plugin_flatpak_config_constructed (GObject *object)
{
  PluginFlatpakConfig *self = (PluginFlatpakConfig *)object;

  G_OBJECT_CLASS (plugin_flatpak_config_parent_class)->constructed (object);

  if (self->file != NULL)
    {
      g_autofree char *name = g_file_get_basename (self->file);

      foundry_config_set_id (FOUNDRY_CONFIG (self), name);
      foundry_config_set_name (FOUNDRY_CONFIG (self), name);
    }
}

static void
plugin_flatpak_config_finalize (GObject *object)
{
  PluginFlatpakConfig *self = (PluginFlatpakConfig *)object;

  g_clear_object (&self->file);

  g_clear_pointer (&self->build_system, g_free);
  g_clear_pointer (&self->command, g_free);
  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->primary_module_name, g_free);
  g_clear_pointer (&self->runtime, g_free);
  g_clear_pointer (&self->runtime_version, g_free);
  g_clear_pointer (&self->sdk, g_free);
  g_clear_pointer (&self->build_args, g_strfreev);
  g_clear_pointer (&self->primary_build_args, g_strfreev);
  g_clear_pointer (&self->primary_build_commands, g_strfreev);
  g_clear_pointer (&self->env, g_strfreev);
  g_clear_pointer (&self->primary_env, g_strfreev);
  g_clear_pointer (&self->prepend_path, g_free);
  g_clear_pointer (&self->append_path, g_free);
  g_clear_pointer (&self->x_run_args, g_strfreev);
  g_clear_pointer (&self->finish_args, g_strfreev);

  G_OBJECT_CLASS (plugin_flatpak_config_parent_class)->finalize (object);
}

static void
plugin_flatpak_config_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  PluginFlatpakConfig *self = PLUGIN_FLATPAK_CONFIG (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_take_object (value, plugin_flatpak_config_dup_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_config_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  PluginFlatpakConfig *self = PLUGIN_FLATPAK_CONFIG (object);

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
plugin_flatpak_config_class_init (PluginFlatpakConfigClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryConfigClass *config_class = FOUNDRY_CONFIG_CLASS (klass);

  object_class->constructed = plugin_flatpak_config_constructed;
  object_class->finalize = plugin_flatpak_config_finalize;
  object_class->get_property = plugin_flatpak_config_get_property;
  object_class->set_property = plugin_flatpak_config_set_property;

  config_class->can_default = plugin_flatpak_config_can_default;
  config_class->resolve_sdk = plugin_flatpak_config_resolve_sdk;
  config_class->dup_build_system = plugin_flatpak_config_dup_build_system;
  config_class->dup_config_opts = plugin_flatpak_config_dup_config_opts;
  config_class->dup_default_command = plugin_flatpak_config_dup_default_command;

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_flatpak_config_init (PluginFlatpakConfig *self)
{
}

/**
 * plugin_flatpak_config_dup_file:
 * @self: a #PluginFlatpakConfig
 *
 * Gets the underlying [iface@Gio.File].
 *
 * Returns: (transfer full): a [iface@Gio.File]
 */
GFile *
plugin_flatpak_config_dup_file (PluginFlatpakConfig *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_CONFIG (self), NULL);

  return g_object_ref (self->file);
}

static DexFuture *
plugin_flatpak_config_resolve_fiber (gpointer user_data)
{
  PluginFlatpakConfig *self = user_data;
  g_autoptr(FoundrySdkManager) sdk_manager = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GPtrArray) installations = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (PLUGIN_IS_FLATPAK_CONFIG (self));

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
_plugin_flatpak_config_resolve (PluginFlatpakConfig *self)
{
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_CONFIG (self));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_flatpak_config_resolve_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

void
_plugin_flatpak_config_set_id (PluginFlatpakConfig *self,
                               const char          *id)
{
  g_set_str (&self->id, id);
}

void
_plugin_flatpak_config_set_runtime (PluginFlatpakConfig *self,
                                    const char          *runtime)
{
  g_set_str (&self->runtime, runtime);
}

void
_plugin_flatpak_config_set_runtime_version (PluginFlatpakConfig *self,
                                            const char          *runtime_version)
{
  g_set_str (&self->runtime_version, runtime_version);
}

void
_plugin_flatpak_config_set_sdk (PluginFlatpakConfig *self,
                                const char          *sdk)
{
  g_set_str (&self->sdk, sdk);
}

void
_plugin_flatpak_config_set_command (PluginFlatpakConfig *self,
                                    const char          *command)
{
  g_set_str (&self->command, command);
}

void
_plugin_flatpak_config_set_build_system (PluginFlatpakConfig *self,
                                         const char          *build_system)
{
  if (g_strcmp0 (build_system, "simple") == 0)
    build_system = "flatpak-simple";

  g_set_str (&self->build_system, build_system);
}

char *
plugin_flatpak_config_dup_id (PluginFlatpakConfig *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_CONFIG (self), NULL);

  return g_strdup (self->id);
}

char *
plugin_flatpak_config_dup_runtime (PluginFlatpakConfig *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_CONFIG (self), NULL);

  return g_strdup (self->runtime);
}

char *
plugin_flatpak_config_dup_runtime_version (PluginFlatpakConfig *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_CONFIG (self), NULL);

  return g_strdup (self->runtime_version);
}

char *
plugin_flatpak_config_dup_sdk (PluginFlatpakConfig *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_CONFIG (self), NULL);

  return g_strdup (self->sdk);
}

char *
plugin_flatpak_config_dup_primary_module_name (PluginFlatpakConfig *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_CONFIG (self), NULL);

  return g_strdup (self->primary_module_name);
}
