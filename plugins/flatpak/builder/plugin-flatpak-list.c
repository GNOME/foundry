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

#include "plugin-flatpak-list.h"

typedef struct
{
  GPtrArray *items;
} PluginFlatpakListPrivate;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (PluginFlatpakList, plugin_flatpak_list, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (PluginFlatpakList)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_ITEM_TYPE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

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
  PluginFlatpakList *self = PLUGIN_FLATPAK_LIST (object);

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

  object_class->dispose = plugin_flatpak_list_dispose;
  object_class->finalize = plugin_flatpak_list_finalize;
  object_class->get_property = plugin_flatpak_list_get_property;

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
