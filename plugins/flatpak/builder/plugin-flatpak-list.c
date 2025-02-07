/* plugin-flatpak-list.c
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

#include "plugin-flatpak-list-private.h"
#include "plugin-flatpak-serializable-private.h"

typedef struct
{
  GPtrArray *items;
} PluginFlatpakListPrivate;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (PluginFlatpakList, plugin_flatpak_list, PLUGIN_TYPE_FLATPAK_SERIALIZABLE,
                                  G_ADD_PRIVATE (PluginFlatpakList)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_ITEM_TYPE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static GType
find_item_type (PluginFlatpakList *self,
                JsonNode          *node)
{
  if (JSON_NODE_HOLDS_OBJECT (node))
    {
      JsonObject *object = json_node_get_object (node);

      if (json_object_has_member (object, "type"))
        {
          const char *type = json_object_get_string_member (object, "type");

          if (type != NULL)
            return PLUGIN_FLATPAK_LIST_GET_CLASS (self)->get_item_type (self, type);
        }
    }

  return PLUGIN_FLATPAK_LIST_GET_CLASS (self)->item_type;
}

static DexFuture *
plugin_flatpak_list_deserialize (PluginFlatpakSerializable *serializable,
                                 JsonNode                  *node)
{
  PluginFlatpakList *self = PLUGIN_FLATPAK_LIST (serializable);
  g_autoptr(GFile) base_dir = _plugin_flatpak_serializable_dup_base_dir (serializable);

  if (0) { }
  else if (JSON_NODE_HOLDS_ARRAY (node))
    {
      JsonArray *ar = json_node_get_array (node);
      gsize len = json_array_get_length (ar);

      /* In this mode, we have a simple [{..}, {..}] style array of
       * objects for the list.
       */

      len = json_array_get_length (ar);

      for (gsize i = 0; i < len; i++)
        {
          JsonNode *element = json_array_get_element (ar, i);
          g_autoptr(PluginFlatpakSerializable) child = NULL;
          g_autoptr(GError) error = NULL;
          GType child_item_type;

          child_item_type = find_item_type (self, element);
          child = _plugin_flatpak_serializable_new (child_item_type, base_dir);

          if (!dex_await (_plugin_flatpak_serializable_deserialize (child, element), &error))
            return dex_future_new_for_error (g_steal_pointer (&error));

          plugin_flatpak_list_add (PLUGIN_FLATPAK_LIST (serializable), child);
        }
    }
  else if (JSON_NODE_HOLDS_OBJECT (node))
    {
      JsonObject *object = json_node_get_object (node);
      JsonObjectIter iter;
      const char *member_name = NULL;
      JsonNode *member_node = NULL;
      GType child_item_type;

      /* In this mode, we have a list that is keyed by the name of
       * the child item type.
       *
       * For example:
       *
       * "add-extensions" : { "name" : { ... } }
       */

      json_object_iter_init (&iter, object);
      while (json_object_iter_next (&iter, &member_name, &member_node))
        {
          g_autoptr(PluginFlatpakSerializable) child = NULL;
          g_autoptr(GError) error = NULL;
          g_auto(GValue) value = G_VALUE_INIT;
          GParamSpec *pspec;

          child_item_type = find_item_type (self, member_node);
          child = _plugin_flatpak_serializable_new (child_item_type, base_dir);

          if (!dex_await (_plugin_flatpak_serializable_deserialize (child, member_node), &error))
            return dex_future_new_for_error (g_steal_pointer (&error));

          if (!(pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (child), "name")) ||
              pspec->value_type != G_TYPE_STRING)
            return dex_future_new_reject (G_IO_ERROR,
                                          G_IO_ERROR_FAILED,
                                          "Object \"%s\" msising name property",
                                          G_OBJECT_TYPE_NAME (child));

          g_value_init (&value, G_TYPE_STRING);
          g_value_set_string (&value, member_name);
          g_object_set_property (G_OBJECT (child), "name", &value);

          plugin_flatpak_list_add (PLUGIN_FLATPAK_LIST (serializable), child);
        }
    }

  return dex_future_new_true ();
}

static GType
plugin_flatpak_list_real_get_item_type (PluginFlatpakList *self,
                                        const char        *type)
{
  return PLUGIN_FLATPAK_LIST_GET_CLASS (self)->item_type;
}

static void
plugin_flatpak_list_constructed (GObject *object)
{
  PluginFlatpakList *self = (PluginFlatpakList *)object;

  G_OBJECT_CLASS (plugin_flatpak_list_parent_class)->constructed (object);

  if (!g_type_is_a (PLUGIN_FLATPAK_LIST_GET_CLASS (self)->item_type, PLUGIN_TYPE_FLATPAK_SERIALIZABLE))
    g_error ("Attempt to create %s without a serializable item-type of \"%s\"\n",
             G_OBJECT_TYPE_NAME (self),
             g_type_name (PLUGIN_FLATPAK_LIST_GET_CLASS (self)->item_type));
}

static void
plugin_flatpak_list_dispose (GObject *object)
{
  PluginFlatpakList *self = (PluginFlatpakList *)object;
  PluginFlatpakListPrivate *priv = plugin_flatpak_list_get_instance_private (self);

  if (priv->items->len > 0)
    g_ptr_array_remove_range (priv->items, 0, priv->items->len);

  G_OBJECT_CLASS (plugin_flatpak_list_parent_class)->dispose (object);
}

static void
plugin_flatpak_list_finalize (GObject *object)
{
  PluginFlatpakList *self = (PluginFlatpakList *)object;
  PluginFlatpakListPrivate *priv = plugin_flatpak_list_get_instance_private (self);

  g_clear_pointer (&priv->items, g_ptr_array_unref);

  G_OBJECT_CLASS (plugin_flatpak_list_parent_class)->finalize (object);
}

static void
plugin_flatpak_list_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, PLUGIN_FLATPAK_LIST_GET_CLASS (object)->item_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_list_class_init (PluginFlatpakListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  PluginFlatpakSerializableClass *serializable_class = PLUGIN_FLATPAK_SERIALIZABLE_CLASS (klass);

  object_class->constructed = plugin_flatpak_list_constructed;
  object_class->dispose = plugin_flatpak_list_dispose;
  object_class->finalize = plugin_flatpak_list_finalize;
  object_class->get_property = plugin_flatpak_list_get_property;

  serializable_class->deserialize = plugin_flatpak_list_deserialize;

  klass->get_item_type = plugin_flatpak_list_real_get_item_type;

  properties[PROP_ITEM_TYPE] =
    g_param_spec_gtype ("item-type", NULL, NULL,
                        G_TYPE_OBJECT,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_flatpak_list_init (PluginFlatpakList *self)
{
  PluginFlatpakListPrivate *priv = plugin_flatpak_list_get_instance_private (self);

  priv->items = g_ptr_array_new_with_free_func (g_object_unref);
}

void
plugin_flatpak_list_add (PluginFlatpakList *self,
                         gpointer           instance)
{
  PluginFlatpakListPrivate *priv = plugin_flatpak_list_get_instance_private (self);

  g_return_if_fail (PLUGIN_IS_FLATPAK_LIST (self));
  g_return_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (instance, PLUGIN_FLATPAK_LIST_GET_CLASS (self)->item_type));

  g_ptr_array_add (priv->items, g_object_ref (instance));

  g_list_model_items_changed (G_LIST_MODEL (self), priv->items->len - 1, 0, 1);
}

static guint
plugin_flatpak_list_get_n_items (GListModel *model)
{
  PluginFlatpakList *self = PLUGIN_FLATPAK_LIST (model);
  PluginFlatpakListPrivate *priv = plugin_flatpak_list_get_instance_private (self);

  return priv->items->len;
}

static gpointer
plugin_flatpak_list_get_item (GListModel *model,
                              guint       position)
{
  PluginFlatpakList *self = PLUGIN_FLATPAK_LIST (model);
  PluginFlatpakListPrivate *priv = plugin_flatpak_list_get_instance_private (self);

  if (position >= priv->items->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (priv->items, position));
}

static GType
plugin_flatpak_list_get_item_type (GListModel *model)
{
  return PLUGIN_FLATPAK_LIST_GET_CLASS (model)->item_type;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = plugin_flatpak_list_get_n_items;
  iface->get_item = plugin_flatpak_list_get_item;
  iface->get_item_type = plugin_flatpak_list_get_item_type;
}
