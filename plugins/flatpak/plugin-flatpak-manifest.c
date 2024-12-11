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

#include "plugin-flatpak-manifest-private.h"
#include "plugin-flatpak-json-manifest.h"

enum {
  PROP_0,
  PROP_FILE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (PluginFlatpakManifest, plugin_flatpak_manifest, FOUNDRY_TYPE_CONFIG)

static GParamSpec *properties[N_PROPS];

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

  object_class->constructed = plugin_flatpak_manifest_constructed;
  object_class->finalize = plugin_flatpak_manifest_finalize;
  object_class->get_property = plugin_flatpak_manifest_get_property;
  object_class->set_property = plugin_flatpak_manifest_set_property;

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
