/* plugin-flatpak-utils.c
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
 * Authors:
 *       Alexander Larsson <alexl@redhat.com>
 *       Hubert Figuière <hub@figuiere.net>
 *       Christian Hergert <chergert@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "plugin-flatpak-list.h"
#include "plugin-flatpak-utils.h"

typedef struct
{
  GParamSpec *pspec;
  JsonNode   *data;
} XProperty;

static XProperty *
builder_x_property_new (const char *name)
{
  XProperty *property;

  property = g_new0 (XProperty, 1);
  property->pspec = g_param_spec_boxed (name, NULL, NULL,
                                        JSON_TYPE_NODE,
                                        (G_PARAM_READWRITE |
                                         G_PARAM_EXPLICIT_NOTIFY |
                                         G_PARAM_STATIC_STRINGS));

  return property;
}

static void
builder_x_property_free (XProperty *property)
{
  g_clear_pointer (&property->pspec, g_param_spec_unref);
  g_clear_pointer (&property->data, json_node_unref);
  g_free (property);
}

static const char *
builder_x_property_get_name (XProperty *property)
{
  return g_param_spec_get_name (property->pspec);
}

GParamSpec *
plugin_flatpak_serializable_find_property (JsonSerializable *serializable,
                                           const char       *name)
{
  GObjectClass *object_class;
  GParamSpec *pspec;

  g_return_val_if_fail (JSON_IS_SERIALIZABLE (serializable), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  object_class = G_OBJECT_GET_CLASS (serializable);
  pspec = g_object_class_find_property (object_class, name);

  if (pspec == NULL && g_str_has_prefix (name, "x-"))
    {
      GHashTable *x_props = g_object_get_data (G_OBJECT (serializable), "flatpak-x-props");
      XProperty *prop;

      if (x_props == NULL)
        {
          x_props = g_hash_table_new_full (g_str_hash,
                                           g_str_equal,
                                           NULL,
                                           (GDestroyNotify) builder_x_property_free);
          g_object_set_data_full (G_OBJECT (serializable),
                                  "flatpak-x-props",
                                  x_props,
                                  (GDestroyNotify)g_hash_table_unref);
        }

      if (!(prop = g_hash_table_lookup (x_props, name)))
        {
          prop = builder_x_property_new (name);
          g_hash_table_replace (x_props, (char *)builder_x_property_get_name (prop), prop);
        }

      pspec = prop->pspec;
    }

  if (pspec == NULL &&
      !g_str_has_prefix (name, "__") &&
      !g_str_has_prefix (name, "//") &&
      g_strcmp0 (name, "$schema") != 0)
    g_warning ("Unknown property %s for type %s",
               name,
               G_OBJECT_TYPE_NAME (serializable));

  return pspec;
}

GParamSpec **
plugin_flatpak_serializable_list_properties (JsonSerializable *serializable,
                                             guint            *n_pspecs)
{
  GPtrArray *res = g_ptr_array_new ();
  guint n_normal, i;
  g_autofree GParamSpec **normal = NULL;
  GHashTable *x_props;

  normal = g_object_class_list_properties (G_OBJECT_GET_CLASS (serializable), &n_normal);

  for (i = 0; i < n_normal; i++)
    g_ptr_array_add (res, normal[i]);

  x_props = g_object_get_data (G_OBJECT (serializable), "flatpak-x-props");
  if (x_props)
    {
      GHashTableIter iter;
      XProperty *prop;

      g_hash_table_iter_init (&iter, x_props);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&prop))
        g_ptr_array_add (res, prop->pspec);
    }

  if (n_pspecs)
    *n_pspecs = res->len;

  g_ptr_array_add (res, NULL);

  return (GParamSpec **)g_ptr_array_free (res, FALSE);
}

gboolean
plugin_flatpak_serializable_deserialize_property (JsonSerializable *serializable,
                                                  const gchar      *property_name,
                                                  GValue           *value,
                                                  GParamSpec       *pspec,
                                                  JsonNode         *property_node)
{
  GHashTable *x_props = g_object_get_data (G_OBJECT (serializable), "flatpak-x-props");

  if (x_props)
    {
      XProperty *prop = g_hash_table_lookup (x_props, property_name);
      if (prop)
        {
          g_value_set_boxed (value, property_node);
          return TRUE;
        }
    }

  if (g_type_is_a (pspec->value_type, PLUGIN_TYPE_FLATPAK_LIST))
    {
      g_autoptr(PluginFlatpakList) list = plugin_flatpak_list_new_from_json (pspec->value_type, property_node);
      g_value_set_object (value, list);
      return list != NULL;
    }

  return json_serializable_default_deserialize_property (serializable, property_name, value, pspec, property_node);
}

JsonNode *
plugin_flatpak_serializable_serialize_property (JsonSerializable *serializable,
                                                const gchar      *property_name,
                                                const GValue     *value,
                                                GParamSpec       *pspec)
{
  GHashTable *x_props = g_object_get_data (G_OBJECT (serializable), "flatpak-x-props");

  if (x_props)
    {
      XProperty *prop = g_hash_table_lookup (x_props, property_name);
      if (prop)
        return g_value_dup_boxed (value);
    }

  return json_serializable_default_serialize_property (serializable, property_name, value, pspec);
}

void
plugin_flatpak_serializable_set_property (JsonSerializable *serializable,
                                          GParamSpec       *pspec,
                                          const GValue     *value)
{
  GHashTable *x_props = g_object_get_data (G_OBJECT (serializable), "flatpak-x-props");

  if (x_props)
    {
      XProperty *prop = g_hash_table_lookup (x_props, g_param_spec_get_name (pspec));
      if (prop)
        {
          prop->data = g_value_dup_boxed (value);
          return;
        }
    }

  g_object_set_property (G_OBJECT (serializable), pspec->name, value);
}

void
plugin_flatpak_serializable_get_property (JsonSerializable *serializable,
                                          GParamSpec       *pspec,
                                          GValue           *value)
{
  GHashTable *x_props = g_object_get_data (G_OBJECT (serializable), "flatpak-x-props");

  if (x_props)
    {
      XProperty *prop = g_hash_table_lookup (x_props, g_param_spec_get_name (pspec));
      if (prop)
        {
          g_value_set_boxed (value, prop->data);
          return;
        }
    }

  g_object_get_property (G_OBJECT (serializable), pspec->name, value);
}
