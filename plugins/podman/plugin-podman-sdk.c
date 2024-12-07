/* plugin-podman-sdk.c
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

#include "plugin-podman-sdk.h"

typedef struct
{
  GHashTable *labels;
  gboolean has_started;
} PluginPodmanSdkPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (PluginPodmanSdk, plugin_podman_sdk, FOUNDRY_TYPE_SDK)

static void
foundry_podman_sdk_deserialize_labels (PluginPodmanSdk *self,
                                       JsonObject      *labels)
{
  PluginPodmanSdkPrivate *priv = plugin_podman_sdk_get_instance_private (self);
  JsonObjectIter iter;
  const char *key;
  JsonNode *value;

  g_assert (PLUGIN_IS_PODMAN_SDK (self));
  g_assert (labels != NULL);

  json_object_iter_init (&iter, labels);

  while (json_object_iter_next (&iter, &key, &value))
    {
      if (JSON_NODE_HOLDS_VALUE (value) &&
          json_node_get_value_type (value) == G_TYPE_STRING)
        {
          const char *value_str = json_node_get_string (value);

          g_hash_table_insert (priv->labels, g_strdup (key), g_strdup (value_str));
        }
    }
}

static void
foundry_podman_sdk_deserialize_name (PluginPodmanSdk *self,
                                     JsonArray       *names)
{
  g_assert (PLUGIN_IS_PODMAN_SDK (self));
  g_assert (names != NULL);

  if (json_array_get_length (names) > 0)
    {
      JsonNode *element = json_array_get_element (names, 0);

      if (element != NULL &&
          JSON_NODE_HOLDS_VALUE (element) &&
          json_node_get_value_type (element) == G_TYPE_STRING)
        foundry_sdk_set_name (FOUNDRY_SDK (self),
                              json_node_get_string (element));
    }
}

static gboolean
plugin_podman_sdk_real_deserialize (PluginPodmanSdk  *self,
                                    JsonObject       *object,
                                    GError          **error)
{
  JsonObject *labels_object;
  JsonArray *names_array;
  JsonNode *names;
  JsonNode *labels;
  JsonNode *id;

  g_assert (PLUGIN_IS_PODMAN_SDK (self));
  g_assert (object != NULL);

  if (!(json_object_has_member (object, "Id") &&
        (id = json_object_get_member (object, "Id")) &&
        JSON_NODE_HOLDS_VALUE (id) &&
        json_node_get_value_type (id) == G_TYPE_STRING))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Failed to locate Id in podman container description");
      return FALSE;
    }

  foundry_sdk_set_id (FOUNDRY_SDK (self),
                      json_node_get_string (id));

  if (json_object_has_member (object, "Labels") &&
      (labels = json_object_get_member (object, "Labels")) &&
      JSON_NODE_HOLDS_OBJECT (labels) &&
      (labels_object = json_node_get_object (labels)))
    foundry_podman_sdk_deserialize_labels (self, labels_object);

  if (json_object_has_member (object, "Names") &&
      (names = json_object_get_member (object, "Names")) &&
      JSON_NODE_HOLDS_ARRAY (names) &&
      (names_array = json_node_get_array (names)))
    foundry_podman_sdk_deserialize_name (self, names_array);

  return TRUE;
}

static void
plugin_podman_sdk_finalize (GObject *object)
{
  PluginPodmanSdk *self = (PluginPodmanSdk *)object;
  PluginPodmanSdkPrivate *priv = plugin_podman_sdk_get_instance_private (self);

  g_clear_pointer (&priv->labels, g_hash_table_unref);

  G_OBJECT_CLASS (plugin_podman_sdk_parent_class)->finalize (object);
}

static void
plugin_podman_sdk_class_init (PluginPodmanSdkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_podman_sdk_finalize;

  klass->deserialize = plugin_podman_sdk_real_deserialize;
}

static void
plugin_podman_sdk_init (PluginPodmanSdk *self)
{
  PluginPodmanSdkPrivate *priv = plugin_podman_sdk_get_instance_private (self);

  priv->labels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  foundry_sdk_set_kind (FOUNDRY_SDK (self), "podman");
  foundry_sdk_set_installed (FOUNDRY_SDK (self), TRUE);
}

gboolean
plugin_podman_sdk_deserialize (PluginPodmanSdk  *self,
                               JsonObject       *object,
                               GError          **error)
{
  g_return_val_if_fail (PLUGIN_IS_PODMAN_SDK (self), FALSE);
  g_return_val_if_fail (object != NULL, FALSE);

  return PLUGIN_PODMAN_SDK_GET_CLASS (self)->deserialize (self, object, error);
}
