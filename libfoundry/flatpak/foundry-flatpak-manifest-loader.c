/* foundry-flatpak-manifest-loader.c
 *
 * Copyright 2015 Red Hat, Inc
 * Copyright 2023 GNOME Foundation Inc.
 * Copyright 2025 Christian Hergert
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

#include "foundry-flatpak-list.h"
#include "foundry-flatpak-manifest.h"
#include "foundry-flatpak-manifest-loader-private.h"
#include "foundry-flatpak-serializable-private.h"

#include "foundry-trace-private.h"
#include "foundry-yaml-private.h"

struct _FoundryFlatpakManifestLoader
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

G_DEFINE_FINAL_TYPE (FoundryFlatpakManifestLoader, foundry_flatpak_manifest_loader, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_flatpak_manifest_loader_finalize (GObject *object)
{
  FoundryFlatpakManifestLoader *self = (FoundryFlatpakManifestLoader *)object;

  g_clear_object (&self->file);
  g_clear_object (&self->base_dir);

  G_OBJECT_CLASS (foundry_flatpak_manifest_loader_parent_class)->finalize (object);
}

static void
foundry_flatpak_manifest_loader_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  FoundryFlatpakManifestLoader *self = FOUNDRY_FLATPAK_MANIFEST_LOADER (object);

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
foundry_flatpak_manifest_loader_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  FoundryFlatpakManifestLoader *self = FOUNDRY_FLATPAK_MANIFEST_LOADER (object);

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
foundry_flatpak_manifest_loader_class_init (FoundryFlatpakManifestLoaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_flatpak_manifest_loader_finalize;
  object_class->get_property = foundry_flatpak_manifest_loader_get_property;
  object_class->set_property = foundry_flatpak_manifest_loader_set_property;

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_flatpak_manifest_loader_init (FoundryFlatpakManifestLoader *self)
{
}

/**
 * foundry_flatpak_manifest_loader_new:
 * @file: a [iface@Gio.File]
 *
 * Returns: (transfer full):
 */
FoundryFlatpakManifestLoader *
foundry_flatpak_manifest_loader_new (GFile *file)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (FOUNDRY_TYPE_FLATPAK_MANIFEST_LOADER,
                       "file", file,
                       NULL);
}

/**
 * foundry_flatpak_manifest_loader_dup_file:
 * @self: a [class@Foundry.FlatpakManifestLoader]
 *
 * Returns: (transfer full):
 */
GFile *
foundry_flatpak_manifest_loader_dup_file (FoundryFlatpakManifestLoader *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FLATPAK_MANIFEST_LOADER (self), NULL);

  return g_object_ref (self->file);
}

/**
 * foundry_flatpak_manifest_loader_dup_base_dir:
 * @self: a [class@Foundry.FlatpakManifestLoader]
 *
 * Returns: (transfer full):
 */
GFile *
foundry_flatpak_manifest_loader_dup_base_dir (FoundryFlatpakManifestLoader *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FLATPAK_MANIFEST_LOADER (self), NULL);

  return g_object_ref (self->base_dir);
}

static JsonNode *
parse_yaml_to_json (GBytes  *contents,
                    GError **error)
{
  g_autoptr(GPtrArray) documents = NULL;

  FOUNDRY_TRACE_SCOPE ("flatpak.manifest.parse-yaml", NULL);

  if (!(documents = _foundry_yaml_parse (NULL,
                                         contents,
                                         FOUNDRY_YAML_PARSE_FLAGS_COERCE_INTEGERS,
                                         NULL,
                                         NULL,
                                         error)))
    return NULL;

  g_assert_cmpuint (documents->len, ==, 1);

  return g_ptr_array_steal_index (documents, 0);
}

DexFuture *
_foundry_flatpak_manifest_load_file_as_json (GFile *file)
{
  g_autoptr(JsonNode) root = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *basename = NULL;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  FOUNDRY_TRACE_SCOPE ("flatpak.manifest.load-json",
                       "%s",
                       g_file_peek_path (file));

  basename = g_file_get_basename (file);

  if (g_str_has_suffix (basename, ".yaml") ||
      g_str_has_suffix (basename, ".yml"))
    {
      g_autoptr(GBytes) bytes = NULL;

      if (!(bytes = dex_await_boxed (dex_file_load_contents_bytes (file), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!(root = parse_yaml_to_json (bytes, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }
  else
    {
      g_autoptr(JsonParser) parser = json_parser_new_immutable ();

      if (!dex_await (foundry_json_parser_load_from_file (parser, file), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      root = json_node_ref (json_parser_get_root (parser));
    }

  return dex_future_new_take_boxed (JSON_TYPE_NODE, g_steal_pointer (&root));
}

static DexFuture *
foundry_flatpak_manifest_loader_load_fiber (gpointer data)
{
  FoundryFlatpakManifestLoader *self = data;
  g_autoptr(FoundryFlatpakManifest) manifest = NULL;
  g_autoptr(JsonNode) root = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_FLATPAK_MANIFEST_LOADER (self));

  FOUNDRY_TRACE_SCOPE ("flatpak.manifest.load", NULL);

  if (!(root = dex_await_boxed (_foundry_flatpak_manifest_load_file_as_json (self->file), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(manifest = dex_await_object (_foundry_flatpak_manifest_loader_deserialize (self, FOUNDRY_TYPE_FLATPAK_MANIFEST, root), &error)))
      return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&manifest));
}

/**
 * foundry_flatpak_manifest_loader_load:
 * @self: a [class@Foundry.FlatpakManifestLoader]
 *
 * Returns: (transfer full):
 */
DexFuture *
foundry_flatpak_manifest_loader_load (FoundryFlatpakManifestLoader *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_FLATPAK_MANIFEST_LOADER (self));

  return dex_scheduler_spawn (NULL, 0,
                              foundry_flatpak_manifest_loader_load_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

DexFuture *
_foundry_flatpak_manifest_loader_deserialize (FoundryFlatpakManifestLoader *self,
                                              GType                         type,
                                              JsonNode                     *node)
{
  g_autoptr(GObject) object = NULL;

  g_return_val_if_fail (FOUNDRY_IS_FLATPAK_MANIFEST_LOADER (self), NULL);
  g_return_val_if_fail (g_type_is_a (type, G_TYPE_OBJECT), NULL);
  g_return_val_if_fail (node != NULL, NULL);

  FOUNDRY_TRACE_SCOPE ("flatpak.manifest.deserialize",
                       "%s",
                       g_type_name (type));

  if (g_type_is_a (type, FOUNDRY_TYPE_FLATPAK_SERIALIZABLE))
    {
      g_autoptr(FoundryFlatpakSerializable) serializable = NULL;

      serializable = _foundry_flatpak_serializable_new (type, self->base_dir);
      return _foundry_flatpak_serializable_deserialize (serializable, node);
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
