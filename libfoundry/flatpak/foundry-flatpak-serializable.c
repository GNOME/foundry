/* foundry-flatpak-serializable.c
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

#include "foundry-flatpak-serializable-private.h"

typedef struct
{
  GFile      *demarshal_base_dir;
  GHashTable *x_properties;
} FoundryFlatpakSerializablePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (FoundryFlatpakSerializable, foundry_flatpak_serializable, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (FoundryFlatpakSerializable)
                                  G_IMPLEMENT_INTERFACE (JSON_TYPE_SERIALIZABLE, NULL))

static DexFuture *
foundry_flatpak_serializable_real_deserialize_property (FoundryFlatpakSerializable *self,
                                                        const char                 *property_name,
                                                        JsonNode                   *property_node)
{
  FoundryFlatpakSerializablePrivate *priv = foundry_flatpak_serializable_get_instance_private (self);
  GParamSpec *pspec;

  g_assert (FOUNDRY_IS_FLATPAK_SERIALIZABLE (self));
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

  if ((pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (self), property_name)))
    {
      if (g_type_is_a (pspec->value_type, FOUNDRY_TYPE_FLATPAK_SERIALIZABLE))
        {
          g_autoptr(FoundryFlatpakSerializable) child = NULL;
          g_autoptr(GError) error = NULL;

          child = _foundry_flatpak_serializable_new (pspec->value_type, priv->demarshal_base_dir);

          if (dex_await (_foundry_flatpak_serializable_deserialize (child, property_node), &error))
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

          if (json_serializable_default_deserialize_property (JSON_SERIALIZABLE (self), pspec->name, &value, pspec, property_node))
            {
              g_object_set_property (G_OBJECT (self), pspec->name, &value);
              return dex_future_new_true ();
            }
        }

      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "Cound not transform \"%s\" to \"%s\"",
                                    g_type_name (json_node_get_value_type (property_node)),
                                    g_type_name (pspec->value_type));
    }

  /* Skip type, not really a property */
  if (g_strcmp0 (property_name, "type") == 0)
    return dex_future_new_true ();

  /* Skip properties that flatpak-builder also ignores.
   *
   * NOTE: If we do write-back support eventually, we may want to stash
   *       these so they can be added back in a non-destructive manner.
   */
  if (property_name != NULL &&
      (strcmp (property_name, "$schema") == 0 ||
       g_str_has_prefix (property_name, "//") ||
       g_str_has_prefix (property_name, "__")))
    return dex_future_new_true ();

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_FAILED,
                                "No such property \"%s\" in type \"%s\"",
                                property_name, G_OBJECT_TYPE_NAME (self));
}

static DexFuture *
foundry_flatpak_serializable_real_deserialize (FoundryFlatpakSerializable *self,
                                               JsonNode                   *node)
{
  JsonObject *object;
  JsonObjectIter iter;
  const char *member_name;
  JsonNode *member_node;

  g_assert (FOUNDRY_IS_FLATPAK_SERIALIZABLE (self));
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

      if (!dex_await (_foundry_flatpak_serializable_deserialize_property (self, member_name, member_node), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

static int
compare_by_name (gconstpointer a,
                 gconstpointer b)
{
  const GParamSpec * const *pspec_a = a;
  const GParamSpec * const *pspec_b = b;

  return strcmp ((*pspec_a)->name, (*pspec_b)->name);
}

static JsonNode *
node_new_env (const char * const *strv)
{
  g_autoptr(JsonObject) object = json_object_new ();
  JsonNode *node;

  for (guint i = 0; strv[i]; i++)
    {
      const char *eq = strchr (strv[i], '=');
      g_autofree char *key = NULL;
      const char *value;

      if (!eq)
        continue;

      key = g_strndup (strv[i], eq - strv[i]);
      value = eq + 1;

      json_object_set_string_member (object, key, value);
    }

  node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (node, object);
  return node;
}

static JsonNode *
node_new_strv (const char * const *strv)
{
  if (strv && strv[0])
    {
      g_autoptr(JsonArray) array = json_array_new ();
      JsonNode *node = json_node_new (JSON_NODE_ARRAY);
      for (guint i = 0; strv[i]; i++)
        json_array_add_string_element (array, strv[i]);
      json_node_set_array (node, array);
      return node;
    }

  return NULL;
}

static void
serialize_property (JsonObject       *object,
                    const GParamSpec *pspec,
                    const GValue     *value)
{
  g_assert (object != NULL);
  g_assert (pspec != NULL);
  g_assert (G_IS_VALUE (value));

  if (G_IS_PARAM_SPEC_STRING (pspec))
    {
      if (g_value_get_string (value) &&
          g_strcmp0 (g_value_get_string (value), ((GParamSpecString *)pspec)->default_value) != 0)
        json_object_set_string_member (object, pspec->name, g_value_get_string (value));
    }
  else if (G_IS_PARAM_SPEC_BOOLEAN (pspec))
    {
      if (g_value_get_boolean (value) != ((GParamSpecBoolean *)pspec)->default_value)
        json_object_set_boolean_member (object, pspec->name, g_value_get_boolean (value));
    }
  else if (G_IS_PARAM_SPEC_INT (pspec))
    {
      if (g_value_get_int (value) != ((GParamSpecInt *)pspec)->default_value)
        json_object_set_int_member (object, pspec->name, g_value_get_int (value));
    }
  else if (G_IS_PARAM_SPEC_INT64 (pspec))
    {
      if (g_value_get_int64 (value) != ((GParamSpecInt64 *)pspec)->default_value)
        json_object_set_int_member (object, pspec->name, g_value_get_int64 (value));
    }
  else if (G_IS_PARAM_SPEC_DOUBLE (pspec))
    {
      if (g_value_get_double (value) != ((GParamSpecDouble *)pspec)->default_value)
        json_object_set_double_member (object, pspec->name, g_value_get_double (value));
    }
  else if (G_IS_PARAM_SPEC_BOXED (pspec))
    {
      if (pspec->value_type == G_TYPE_STRV)
        {
          const char * const *strv = g_value_get_boxed (value);

          if (strv != NULL && strv[0] != NULL)
            {
              if (g_str_equal (pspec->name, "env"))
                json_object_set_member (object, pspec->name, node_new_env (strv));
              else
                json_object_set_member (object, pspec->name, node_new_strv (strv));
            }
        }
    }
  else if (G_IS_PARAM_SPEC_OBJECT (pspec))
    {
      GObject *child = g_value_get_object (value);

      if (FOUNDRY_IS_FLATPAK_SERIALIZABLE (child))
        {
          g_autoptr(JsonNode) node = NULL;

          if ((node = _foundry_flatpak_serializable_serialize (FOUNDRY_FLATPAK_SERIALIZABLE (child))))
            {
              if (JSON_NODE_HOLDS_OBJECT (node) &&
                  json_object_get_size (json_node_get_object (node)) == 0)
                g_clear_pointer (&node, json_node_unref);

              if (node != NULL)
                json_object_set_member (object, pspec->name, g_steal_pointer (&node));
            }
        }
    }
}

static void
serialize_x_property (gpointer key,
                      gpointer value,
                      gpointer user_data)
{
  const char *property_name = key;
  JsonNode *property_node = value;
  JsonObject *object = user_data;

  json_object_set_member (object, property_name, json_node_ref (property_node));
}

static JsonNode *
foundry_flatpak_serializable_real_serialize (FoundryFlatpakSerializable *self)
{
  FoundryFlatpakSerializablePrivate *priv;
  GObjectClass *object_class;
  g_autofree GParamSpec **pspecs = NULL;
  g_autoptr(JsonObject) object = NULL;
  g_autoptr(JsonNode) node = NULL;
  guint n_pspecs;

  g_assert (FOUNDRY_IS_FLATPAK_SERIALIZABLE (self));

  priv = foundry_flatpak_serializable_get_instance_private (self);
  object_class = G_OBJECT_GET_CLASS (self);
  object = json_object_new ();

  if (FOUNDRY_IS_FLATPAK_SOURCE (self) &&
      FOUNDRY_FLATPAK_SOURCE_GET_CLASS (self)->type)
    json_object_set_string_member (object, "type", FOUNDRY_FLATPAK_SOURCE_GET_CLASS (self)->type);

  pspecs = g_object_class_list_properties (object_class, &n_pspecs);

  if (pspecs != NULL && n_pspecs > 0)
    {
      qsort (pspecs, n_pspecs, sizeof (GParamSpec *), compare_by_name);

      for (guint i = 0; i < n_pspecs; i++)
        {
          const GParamSpec *pspec = pspecs[i];
          g_auto(GValue) value = G_VALUE_INIT;

          if ((pspec->flags & G_PARAM_READWRITE) != G_PARAM_READWRITE)
            continue;

          g_value_init (&value, pspec->value_type);
          g_object_get_property (G_OBJECT (self), pspec->name, &value);

          serialize_property (object, pspec, &value);
        }
    }

  if (priv->x_properties != NULL)
    g_hash_table_foreach (priv->x_properties, serialize_x_property, object);

  if (json_object_get_size (object) == 0)
    return NULL;

  node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (node, object);

  return g_steal_pointer (&node);
}

static void
foundry_flatpak_serializable_finalize (GObject *object)
{
  FoundryFlatpakSerializable *self = (FoundryFlatpakSerializable *)object;
  FoundryFlatpakSerializablePrivate *priv = foundry_flatpak_serializable_get_instance_private (self);

  g_clear_pointer (&priv->x_properties, g_hash_table_unref);
  g_clear_object (&priv->demarshal_base_dir);

  G_OBJECT_CLASS (foundry_flatpak_serializable_parent_class)->finalize (object);
}

static void
foundry_flatpak_serializable_class_init (FoundryFlatpakSerializableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_flatpak_serializable_finalize;

  klass->deserialize = foundry_flatpak_serializable_real_deserialize;
  klass->deserialize_property = foundry_flatpak_serializable_real_deserialize_property;
  klass->serialize = foundry_flatpak_serializable_real_serialize;
}

static void
foundry_flatpak_serializable_init (FoundryFlatpakSerializable *self)
{
}

gpointer
_foundry_flatpak_serializable_new (GType  type,
                                   GFile *demarshal_base_dir)
{
  FoundryFlatpakSerializablePrivate *priv;
  FoundryFlatpakSerializable *self;

  g_return_val_if_fail (type != FOUNDRY_TYPE_FLATPAK_SERIALIZABLE, NULL);
  g_return_val_if_fail (g_type_is_a (type, FOUNDRY_TYPE_FLATPAK_SERIALIZABLE), NULL);
  g_return_val_if_fail (G_IS_FILE (demarshal_base_dir), NULL);

  self = g_object_new (type, NULL);
  priv = foundry_flatpak_serializable_get_instance_private (self);
  priv->demarshal_base_dir = g_object_ref (demarshal_base_dir);

  return self;
}

/**
 * foundry_flatpak_serializable_resolve_file:
 * @self: a [class@Foundry.FlatpakSerializable]
 *
 * Returns: (transfer full): a #GFile or %NULL and @error is set
 */
GFile *
foundry_flatpak_serializable_resolve_file (FoundryFlatpakSerializable  *self,
                                           const char                  *path,
                                           GError                     **error)
{
  FoundryFlatpakSerializablePrivate *priv = foundry_flatpak_serializable_get_instance_private (self);
  g_autoptr(GFile) child = NULL;
  g_autoptr(GFile) canonical = NULL;

  g_return_val_if_fail (FOUNDRY_IS_FLATPAK_SERIALIZABLE (self), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  child = g_file_get_child (priv->demarshal_base_dir, path);
  if (!(canonical = foundry_file_canonicalize (child, error)))
    return NULL;

  if (!foundry_file_is_in (canonical, priv->demarshal_base_dir))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "Cannot access \"%s\" outside of base directory",
                   g_file_peek_path (canonical));
      return NULL;
    }

  return g_steal_pointer (&canonical);
}

DexFuture *
_foundry_flatpak_serializable_deserialize (FoundryFlatpakSerializable *self,
                                           JsonNode                   *node)
{
  g_autoptr(JsonNode) loaded = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_FLATPAK_SERIALIZABLE (self));
  dex_return_error_if_fail (node != NULL);

  if (JSON_NODE_HOLDS_VALUE (node) &&
      json_node_get_value_type (node) == G_TYPE_STRING)
    {
      const char *path = json_node_get_string (node);
      g_autoptr(JsonParser) parser = NULL;
      g_autoptr(GError) error = NULL;
      g_autoptr(GFile) file = NULL;

      if (!(file = foundry_flatpak_serializable_resolve_file (self, path, &error)))
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_NOT_FOUND,
                                      "Failed to load \"%s\"",
                                      path);

      parser = json_parser_new_immutable ();

      if (!dex_await (foundry_json_parser_load_from_file (parser, file), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      node = loaded = json_node_ref (json_parser_get_root (parser));
    }

  return dex_future_then (FOUNDRY_FLATPAK_SERIALIZABLE_GET_CLASS (self)->deserialize (self, node),
                          foundry_future_return_object,
                          g_object_ref (self),
                          g_object_unref);
}

DexFuture *
_foundry_flatpak_serializable_deserialize_property (FoundryFlatpakSerializable *self,
                                                    const char                 *property_name,
                                                    JsonNode                   *property_node)
{
  dex_return_error_if_fail (FOUNDRY_IS_FLATPAK_SERIALIZABLE (self));
  dex_return_error_if_fail (property_name != NULL);
  dex_return_error_if_fail (property_node != NULL);

  return FOUNDRY_FLATPAK_SERIALIZABLE_GET_CLASS (self)->deserialize_property (self, property_name, property_node);
}

GFile *
_foundry_flatpak_serializable_dup_base_dir (FoundryFlatpakSerializable *self)
{
  FoundryFlatpakSerializablePrivate *priv = foundry_flatpak_serializable_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_FLATPAK_SERIALIZABLE (self), NULL);

  return g_object_ref (priv->demarshal_base_dir);
}

/**
 * foundry_flatpak_serializable_dup_x_string:
 * @self: a [class@Foundry.FlatpakSerializable]
 *
 * Returns: (transfer full) (nullable):
 */
char *
foundry_flatpak_serializable_dup_x_string (FoundryFlatpakSerializable *self,
                                           const char                 *property)
{
  FoundryFlatpakSerializablePrivate *priv = foundry_flatpak_serializable_get_instance_private (self);
  JsonNode *node;

  g_return_val_if_fail (FOUNDRY_IS_FLATPAK_SERIALIZABLE (self), NULL);
  g_return_val_if_fail (property != NULL, NULL);

  if (priv->x_properties == NULL)
    return NULL;

  if ((node = g_hash_table_lookup (priv->x_properties, property)))
    {
      if (JSON_NODE_HOLDS_VALUE (node) &&
          G_TYPE_STRING == json_node_get_value_type (node))
        return g_strdup (json_node_get_string (node));
    }

  return NULL;
}

/**
 * foundry_flatpak_serializable_dup_x_strv:
 * @self: a [class@Foundry.FlatpakSerializable]
 *
 * Returns: (transfer full) (nullable):
 */
char **
foundry_flatpak_serializable_dup_x_strv (FoundryFlatpakSerializable *self,
                                         const char                 *property)
{
  FoundryFlatpakSerializablePrivate *priv = foundry_flatpak_serializable_get_instance_private (self);
  JsonNode *node;

  g_return_val_if_fail (FOUNDRY_IS_FLATPAK_SERIALIZABLE (self), NULL);
  g_return_val_if_fail (property != NULL, NULL);

  if (priv->x_properties == NULL)
    return NULL;

  if ((node = g_hash_table_lookup (priv->x_properties, property)))
    {
      g_autoptr(GStrvBuilder) builder = g_strv_builder_new ();

      if (JSON_NODE_HOLDS_ARRAY (node))
        {
          JsonArray *ar = json_node_get_array (node);
          gsize len = json_array_get_length (ar);

          for (gsize i = 0; i < len; i++)
            {
              const char *str = json_array_get_string_element (ar, i);

              if (str != NULL)
                g_strv_builder_add (builder, str);
            }
        }

      return g_strv_builder_end (builder);
    }

  return NULL;
}

JsonNode *
_foundry_flatpak_serializable_serialize (FoundryFlatpakSerializable *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FLATPAK_SERIALIZABLE (self), NULL);

  if (FOUNDRY_FLATPAK_SERIALIZABLE_GET_CLASS (self)->serialize)
    return FOUNDRY_FLATPAK_SERIALIZABLE_GET_CLASS (self)->serialize (self);

  return NULL;
}
