/* plugin-flatpak-sdk.c
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

#include <glib/gi18n-lib.h>

#include "plugin-flatpak.h"
#include "plugin-flatpak-sdk-private.h"

enum {
  PROP_0,
  PROP_INSTALLATION,
  PROP_REF,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PluginFlatpakSdk, plugin_flatpak_sdk, FOUNDRY_TYPE_SDK)

static GParamSpec *properties[N_PROPS];

static void
plugin_flatpak_sdk_constructed (GObject *object)
{
  PluginFlatpakSdk *self = (PluginFlatpakSdk *)object;
  g_autofree char *id = NULL;
  g_autofree char *sdk_name = NULL;
  const char *name;
  const char *arch;
  const char *branch;

  G_OBJECT_CLASS (plugin_flatpak_sdk_parent_class)->constructed (object);

  if (self->installation == NULL || self->ref == NULL)
    g_return_if_reached ();

  name = flatpak_ref_get_name (self->ref);
  arch = flatpak_ref_get_arch (self->ref);
  branch = flatpak_ref_get_branch (self->ref);

  id = g_strdup_printf ("%s/%s/%s", name, arch, branch);

  if (g_str_equal (flatpak_get_default_arch (), arch))
    sdk_name = g_strdup_printf ("%s %s", name, branch);
  else
    sdk_name = g_strdup_printf ("%s %s (%s)", name, branch, arch);

  foundry_sdk_set_id (FOUNDRY_SDK (self), id);
  foundry_sdk_set_name (FOUNDRY_SDK (self), sdk_name);
  foundry_sdk_set_kind (FOUNDRY_SDK (self), "flatpak");
  foundry_sdk_set_arch (FOUNDRY_SDK (self), arch);

  if (FLATPAK_IS_INSTALLED_REF (self->ref))
    foundry_sdk_set_installed (FOUNDRY_SDK (self), TRUE);
}

static void
plugin_flatpak_sdk_finalize (GObject *object)
{
  PluginFlatpakSdk *self = (PluginFlatpakSdk *)object;

  g_clear_object (&self->installation);
  g_clear_object (&self->ref);

  G_OBJECT_CLASS (plugin_flatpak_sdk_parent_class)->finalize (object);
}

static void
plugin_flatpak_sdk_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  PluginFlatpakSdk *self = PLUGIN_FLATPAK_SDK (object);

  switch (prop_id)
    {
    case PROP_INSTALLATION:
      g_value_set_object (value, self->installation);
      break;

    case PROP_REF:
      g_value_set_object (value, self->ref);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_sdk_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  PluginFlatpakSdk *self = PLUGIN_FLATPAK_SDK (object);

  switch (prop_id)
    {
    case PROP_INSTALLATION:
      self->installation = g_value_dup_object (value);
      break;

    case PROP_REF:
      self->ref = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_sdk_class_init (PluginFlatpakSdkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundrySdkClass *sdk_class = FOUNDRY_SDK_CLASS (klass);

  object_class->constructed = plugin_flatpak_sdk_constructed;
  object_class->finalize = plugin_flatpak_sdk_finalize;
  object_class->get_property = plugin_flatpak_sdk_get_property;
  object_class->set_property = plugin_flatpak_sdk_set_property;

  sdk_class->install = plugin_flatpak_sdk_install;

  properties[PROP_INSTALLATION] =
    g_param_spec_object ("installation", NULL, NULL,
                         FLATPAK_TYPE_INSTALLATION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_REF] =
    g_param_spec_object ("ref", NULL, NULL,
                         FLATPAK_TYPE_REF,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_flatpak_sdk_init (PluginFlatpakSdk *self)
{
}

PluginFlatpakSdk *
plugin_flatpak_sdk_new (FoundryContext      *context,
                        FlatpakInstallation *installation,
                        FlatpakRef          *ref)
{
  gboolean extension_only;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (FLATPAK_IS_INSTALLATION (installation), NULL);
  g_return_val_if_fail (FLATPAK_IS_REF (ref), NULL);

  /* Really we need to check this by looking at the metadata bytes. But
   * this is much faster than doing that and generally gets the same
   * answer.
   */
  extension_only = strstr (flatpak_ref_get_name (ref), ".Extension.") != NULL;

  return g_object_new (PLUGIN_TYPE_FLATPAK_SDK,
                       "context", context,
                       "extension-only", extension_only,
                       "installation", installation,
                       "ref", ref,
                       NULL);
}
