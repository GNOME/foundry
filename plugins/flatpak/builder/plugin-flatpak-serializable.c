/* plugin-flatpak-serializable.c
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

#include "plugin-flatpak-serializable-private.h"

typedef struct
{
  GFile      *demarshal_base_dir;
  GHashTable *x_properties;
} PluginFlatpakSerializablePrivate;

static void serializable_iface_init (JsonSerializableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (PluginFlatpakSerializable, plugin_flatpak_serializable, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (PluginFlatpakSerializable)
                                  G_IMPLEMENT_INTERFACE (JSON_TYPE_SERIALIZABLE, serializable_iface_init))

static DexFuture *
plugin_flatpak_serializable_real_deserialize_property (PluginFlatpakSerializable *self,
                                                       const char                *property_name,
                                                       JsonNode                  *property_node)
{
  PluginFlatpakSerializablePrivate *priv = plugin_flatpak_serializable_get_instance_private (self);
  GParamSpec *pspec;

  g_assert (PLUGIN_IS_FLATPAK_SERIALIZABLE (self));
  g_assert (property_name != NULL);
  g_assert (property_node != NULL);

  if (g_str_has_prefix (property_name, "x-"))
    {
      if (priv->x_properties == NULL)
        priv->x_properties = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    (GDestroyNotify) json_node_unref);

      g_hash_table_replace (priv->x_properties,
                            g_strdup (property_name),
                            json_node_ref (property_node));

      return dex_future_new_true ();
    }

  /* Skip type property */
  if (g_strcmp0 (property_name, "type") == 0)
    return dex_future_new_true ();

  if ((pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (self), property_name)))
    {
      if (g_type_is_a (pspec->value_type, PLUGIN_TYPE_FLATPAK_SERIALIZABLE))
        {
          g_autoptr(PluginFlatpakSerializable) child = NULL;
          g_autoptr(GError) error = NULL;

          child = _plugin_flatpak_serializable_new (pspec->value_type, priv->demarshal_base_dir);

          if (dex_await (_plugin_flatpak_serializable_deserialize (child, property_node), &error))
            {
              g_auto(GValue) value = G_VALUE_INIT;

              g_value_init (&value, G_OBJECT_TYPE (child));
              g_value_set_object (&value, child);
              g_object_set_property (G_OBJECT (self), pspec->name, &value);

              return dex_future_new_true ();
            }

          return dex_future_new_for_error (g_steal_pointer (&error));
        }
      else
        {
          g_auto(GValue) value = G_VALUE_INIT;

          if (json_serializable_default_deserialize_property (JSON_SERIALIZABLE (self), property_name, &value, pspec, property_node))
            return dex_future_new_true ();
        }

      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "Cound not transform \"%s\" to \"%s\"",
                                    g_type_name (json_node_get_value_type (property_node)),
                                    g_type_name (pspec->value_type));
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_FAILED,
                                "No such property \"%s\" in type \"%s\"",
                                property_name, G_OBJECT_TYPE_NAME (self));
}

static DexFuture *
plugin_flatpak_serializable_real_deserialize (PluginFlatpakSerializable *self,
                                              JsonNode                  *node)
{
  JsonObject *object;
  JsonObjectIter iter;
  const char *member_name;
  JsonNode *member_node;

  g_assert (PLUGIN_IS_FLATPAK_SERIALIZABLE (self));
  g_assert (node != NULL);

  if (!JSON_NODE_HOLDS_OBJECT (node))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "Got something other than an object");

  object = json_node_get_object (node);

  json_object_iter_init (&iter, object);
  while (json_object_iter_next (&iter, &member_name, &member_node))
    {
      g_autoptr(GError) error = NULL;

      if (!dex_await (_plugin_flatpak_serializable_deserialize_property (self, member_name, member_node), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

static void
plugin_flatpak_serializable_finalize (GObject *object)
{
  PluginFlatpakSerializable *self = (PluginFlatpakSerializable *)object;
  PluginFlatpakSerializablePrivate *priv = plugin_flatpak_serializable_get_instance_private (self);

  g_clear_pointer (&priv->x_properties, g_hash_table_unref);
  g_clear_object (&priv->demarshal_base_dir);

  G_OBJECT_CLASS (plugin_flatpak_serializable_parent_class)->finalize (object);
}

static void
plugin_flatpak_serializable_class_init (PluginFlatpakSerializableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_flatpak_serializable_finalize;

  klass->deserialize = plugin_flatpak_serializable_real_deserialize;
  klass->deserialize_property = plugin_flatpak_serializable_real_deserialize_property;
}

static void
plugin_flatpak_serializable_init (PluginFlatpakSerializable *self)
{
}

gpointer
_plugin_flatpak_serializable_new (GType  type,
                                  GFile *demarshal_base_dir)
{
  PluginFlatpakSerializablePrivate *priv;
  PluginFlatpakSerializable *self;

  g_return_val_if_fail (type != PLUGIN_TYPE_FLATPAK_SERIALIZABLE, NULL);
  g_return_val_if_fail (g_type_is_a (type, PLUGIN_TYPE_FLATPAK_SERIALIZABLE), NULL);
  g_return_val_if_fail (G_IS_FILE (demarshal_base_dir), NULL);

  self = g_object_new (type, NULL);
  priv = plugin_flatpak_serializable_get_instance_private (self);
  priv->demarshal_base_dir = g_object_ref (demarshal_base_dir);

  return self;
}

/**
 * plugin_flatpak_serializable_resolve_file:
 * @self: a [class@Plugin.FlatpakSerializable]
 *
 * Returns: (transfer full): a #GFile or %NULL and @error is set
 */
GFile *
plugin_flatpak_serializable_resolve_file (PluginFlatpakSerializable  *self,
                                          const char                 *path,
                                          GError                    **error)
{
  PluginFlatpakSerializablePrivate *priv = plugin_flatpak_serializable_get_instance_private (self);
  g_autoptr(GFile) child = NULL;

  g_return_val_if_fail (PLUGIN_IS_FLATPAK_SERIALIZABLE (self), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  child = g_file_get_child (priv->demarshal_base_dir, path);

  return foundry_file_canonicalize (child, error);
}

DexFuture *
_plugin_flatpak_serializable_deserialize (PluginFlatpakSerializable *self,
                                          JsonNode                  *node)
{
  g_autoptr(JsonNode) loaded = NULL;

  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_SERIALIZABLE (self));
  dex_return_error_if_fail (node != NULL);

  if (JSON_NODE_HOLDS_VALUE (node) &&
      json_node_get_value_type (node) == G_TYPE_STRING)
    {
      const char *path = json_node_get_string (node);
      g_autoptr(JsonParser) parser = NULL;
      g_autoptr(GError) error = NULL;
      g_autoptr(GFile) file = NULL;

      if (!(file = plugin_flatpak_serializable_resolve_file (self, path, &error)))
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_NOT_FOUND,
                                      "Failed to load \"%s\"",
                                      path);

      parser = json_parser_new_immutable ();

      if (!dex_await (foundry_json_parser_load_from_file (parser, file), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      node = loaded = json_node_ref (json_parser_get_root (parser));
    }

  return dex_future_then (PLUGIN_FLATPAK_SERIALIZABLE_GET_CLASS (self)->deserialize (self, node),
                          foundry_future_return_object,
                          g_object_ref (self),
                          g_object_unref);
}

DexFuture *
_plugin_flatpak_serializable_deserialize_property (PluginFlatpakSerializable *self,
                                                   const char                *property_name,
                                                   JsonNode                  *property_node)
{
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_SERIALIZABLE (self));
  dex_return_error_if_fail (property_name != NULL);
  dex_return_error_if_fail (property_node != NULL);

  return PLUGIN_FLATPAK_SERIALIZABLE_GET_CLASS (self)->deserialize_property (self, property_name, property_node);
}

static gboolean
_as_json_serializable_deserialize_property (JsonSerializable *serializable,
                                            const char       *property_name,
                                            GValue           *value,
                                            GParamSpec       *pspec,
                                            JsonNode         *node)
{
  g_critical ("What\n");
  return FALSE;
}

static void
serializable_iface_init (JsonSerializableIface *iface)
{
  /* Bridge JSON Serializable to our helpers */

  iface->deserialize_property = _as_json_serializable_deserialize_property;
}

GFile *
_plugin_flatpak_serializable_dup_base_dir (PluginFlatpakSerializable *self)
{
  PluginFlatpakSerializablePrivate *priv = plugin_flatpak_serializable_get_instance_private (self);

  g_return_val_if_fail (PLUGIN_IS_FLATPAK_SERIALIZABLE (self), NULL);

  return g_object_ref (priv->demarshal_base_dir);
}
