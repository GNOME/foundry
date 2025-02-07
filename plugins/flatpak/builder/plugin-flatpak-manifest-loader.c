/* plugin-flatpak-manifest-loader.c
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

#include <foundry.h>

#include "plugin-flatpak-list.h"
#include "plugin-flatpak-manifest.h"
#include "plugin-flatpak-manifest-loader-private.h"
#include "plugin-flatpak-serializable-private.h"

struct _PluginFlatpakManifestLoader
{
  GObject  parent_instance;
  GFile   *file;
  GFile   *base_dir;
};

enum {
  PROP_0,
  PROP_FILE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PluginFlatpakManifestLoader, plugin_flatpak_manifest_loader, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
plugin_flatpak_manifest_loader_finalize (GObject *object)
{
  PluginFlatpakManifestLoader *self = (PluginFlatpakManifestLoader *)object;

  g_clear_object (&self->file);
  g_clear_object (&self->base_dir);

  G_OBJECT_CLASS (plugin_flatpak_manifest_loader_parent_class)->finalize (object);
}

static void
plugin_flatpak_manifest_loader_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  PluginFlatpakManifestLoader *self = PLUGIN_FLATPAK_MANIFEST_LOADER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_manifest_loader_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  PluginFlatpakManifestLoader *self = PLUGIN_FLATPAK_MANIFEST_LOADER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      if ((self->file = g_value_dup_object (value)))
        self->base_dir = g_file_get_parent (self->file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_manifest_loader_class_init (PluginFlatpakManifestLoaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_flatpak_manifest_loader_finalize;
  object_class->get_property = plugin_flatpak_manifest_loader_get_property;
  object_class->set_property = plugin_flatpak_manifest_loader_set_property;

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_flatpak_manifest_loader_init (PluginFlatpakManifestLoader *self)
{
}

PluginFlatpakManifestLoader *
plugin_flatpak_manifest_loader_new (GFile *file)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (PLUGIN_TYPE_FLATPAK_MANIFEST_LOADER,
                       "file", file,
                       NULL);
}

GFile *
plugin_flatpak_manifest_loader_dup_file (PluginFlatpakManifestLoader *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_MANIFEST_LOADER (self), NULL);

  return g_object_ref (self->file);
}

GFile *
plugin_flatpak_manifest_loader_dup_base_dir (PluginFlatpakManifestLoader *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_MANIFEST_LOADER (self), NULL);

  return g_object_ref (self->base_dir);
}

static DexFuture *
plugin_flatpak_manifest_loader_load_fiber (gpointer data)
{
  PluginFlatpakManifestLoader *self = data;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (PLUGIN_IS_FLATPAK_MANIFEST_LOADER (self));

  parser = json_parser_new_immutable ();

  if (!dex_await (foundry_json_parser_load_from_file (parser, self->file), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return _plugin_flatpak_manifest_loader_deserialize (self,
                                                      PLUGIN_TYPE_FLATPAK_MANIFEST,
                                                      json_parser_get_root (parser));
}

DexFuture *
plugin_flatpak_manifest_loader_load (PluginFlatpakManifestLoader *self)
{
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_MANIFEST_LOADER (self));

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              plugin_flatpak_manifest_loader_load_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

DexFuture *
_plugin_flatpak_manifest_loader_deserialize (PluginFlatpakManifestLoader *self,
                                             GType                        type,
                                             JsonNode                    *node)
{
  g_autoptr(GObject) object = NULL;

  g_return_val_if_fail (PLUGIN_IS_FLATPAK_MANIFEST_LOADER (self), NULL);
  g_return_val_if_fail (g_type_is_a (type, G_TYPE_OBJECT), NULL);
  g_return_val_if_fail (node != NULL, NULL);

  if (g_type_is_a (type, PLUGIN_TYPE_FLATPAK_SERIALIZABLE))
    {
      g_autoptr(PluginFlatpakSerializable) serializable = NULL;

      serializable = _plugin_flatpak_serializable_new (type, self->base_dir);
      return _plugin_flatpak_serializable_deserialize (serializable, node);
    }

  if (JSON_NODE_HOLDS_NULL (node))
    return dex_future_new_take_object (NULL);

  if ((object = json_gobject_deserialize (type, node)))
    return dex_future_new_take_object (g_steal_pointer (&object));

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_FAILED,
                                "Failed to deserialize type \"%s\"",
                                g_type_name (type));
}
