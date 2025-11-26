/* foundry-path-navigator.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-path-navigator.h"
#include "foundry-util.h"

G_DEFINE_ABSTRACT_TYPE (FoundryPathNavigator, foundry_path_navigator, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ICON,
  PROP_INTENT,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_path_navigator_dispose (GObject *object)
{
  G_OBJECT_CLASS (foundry_path_navigator_parent_class)->dispose (object);
}

static void
foundry_path_navigator_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  FoundryPathNavigator *self = FOUNDRY_PATH_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_ICON:
      g_value_take_object (value, foundry_path_navigator_dup_icon (self));
      break;

    case PROP_INTENT:
      g_value_take_object (value, foundry_path_navigator_dup_intent (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, foundry_path_navigator_dup_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_path_navigator_class_init (FoundryPathNavigatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_path_navigator_dispose;
  object_class->get_property = foundry_path_navigator_get_property;

  properties[PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_INTENT] =
    g_param_spec_object ("intent", NULL, NULL,
                         FOUNDRY_TYPE_INTENT,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_path_navigator_init (FoundryPathNavigator *self)
{
}

/**
 * foundry_path_navigator_dup_icon:
 * @self: a [class@FoundryAdw.PathNavigator]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
GIcon *
foundry_path_navigator_dup_icon (FoundryPathNavigator *self)
{
  g_return_val_if_fail (FOUNDRY_IS_PATH_NAVIGATOR (self), NULL);

  if (FOUNDRY_PATH_NAVIGATOR_GET_CLASS (self)->dup_icon)
    return FOUNDRY_PATH_NAVIGATOR_GET_CLASS (self)->dup_icon (self);

  return NULL;
}

/**
 * foundry_path_navigator_dup_title:
 * @self: a [class@FoundryAdw.PathNavigator]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_path_navigator_dup_title (FoundryPathNavigator *self)
{
  g_return_val_if_fail (FOUNDRY_IS_PATH_NAVIGATOR (self), NULL);

  if (FOUNDRY_PATH_NAVIGATOR_GET_CLASS (self)->dup_title)
    return FOUNDRY_PATH_NAVIGATOR_GET_CLASS (self)->dup_title (self);

  return NULL;
}

/**
 * foundry_path_navigator_find_parent:
 * @self: a [class@FoundryAdw.PathNavigator]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves
 *   to a [class@FoundryAdw.PathNavigator] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_path_navigator_find_parent (FoundryPathNavigator *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_PATH_NAVIGATOR (self));

  if (FOUNDRY_PATH_NAVIGATOR_GET_CLASS (self)->find_parent)
    return FOUNDRY_PATH_NAVIGATOR_GET_CLASS (self)->find_parent (self);

  return foundry_future_new_not_supported ();
}

/**
 * foundry_path_navigator_list_siblings:
 * @self: a [class@FoundryAdw.PathNavigator]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves
 *   to a [class@FoundryAdw.PathNavigator] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_path_navigator_list_siblings (FoundryPathNavigator *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_PATH_NAVIGATOR (self));

  if (FOUNDRY_PATH_NAVIGATOR_GET_CLASS (self)->list_siblings)
    return FOUNDRY_PATH_NAVIGATOR_GET_CLASS (self)->list_siblings (self);

  return foundry_future_new_not_supported ();
}

/**
 * foundry_path_navigator_list_children:
 * @self: a [class@FoundryAdw.PathNavigator]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves
 *   to a [class@FoundryAdw.PathNavigator] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_path_navigator_list_children (FoundryPathNavigator *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_PATH_NAVIGATOR (self));

  if (FOUNDRY_PATH_NAVIGATOR_GET_CLASS (self)->list_children)
    return FOUNDRY_PATH_NAVIGATOR_GET_CLASS (self)->list_children (self);

  return foundry_future_new_not_supported ();
}

static DexFuture *
foundry_path_navigator_list_to_root_fiber (gpointer data)
{
  FoundryPathNavigator *self = data;
  g_autoptr(FoundryPathNavigator) navigator = NULL;
  g_autoptr(GListStore) store = NULL;

  g_assert (FOUNDRY_IS_PATH_NAVIGATOR (self));

  store = g_list_store_new (FOUNDRY_TYPE_PATH_NAVIGATOR);
  navigator = g_object_ref (self);

  while (navigator != NULL)
    {
      g_autoptr(FoundryPathNavigator) parent = NULL;

      g_list_store_insert (store, 0, navigator);
      parent = dex_await_object (foundry_path_navigator_find_parent (navigator), NULL);
      g_set_object (&navigator, parent);
    }

  return dex_future_new_for_object (g_steal_pointer (&store));
}

/**
 * foundry_path_navigator_list_to_root:
 * @self: a [class@FoundryAdw.PathNavigator]
 *
 * Asynchronously populates a [iface@Gio.ListModel] of [class@FoundryAdw.PathNavigator]
 * starting from this navigator to the root navigator by following
 * [method@FoundryAdw.PathNavigator.find_parent] until there are no more parents.
 *
 * The root navigator is placed at position 0 and @self is placed in the last
 * position.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@FoundryAdw.PathNavigator].
 *
 * Since: 1.1
 */
DexFuture *
foundry_path_navigator_list_to_root (FoundryPathNavigator *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_PATH_NAVIGATOR (self));

  return dex_scheduler_spawn (NULL, 0,
                              foundry_path_navigator_list_to_root_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

/**
 * foundry_path_navigator_dup_intent:
 * @self: a [class@FoundryAdw.PathNavigator]
 *
 * Gets an intent that can be used to navigate to this path navigator.
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.Intent] or %NULL
 *
 * Since: 1.1
 */
FoundryIntent *
foundry_path_navigator_dup_intent (FoundryPathNavigator *self)
{
  g_return_val_if_fail (FOUNDRY_IS_PATH_NAVIGATOR (self), NULL);

  if (FOUNDRY_PATH_NAVIGATOR_GET_CLASS (self)->dup_intent)
    return FOUNDRY_PATH_NAVIGATOR_GET_CLASS (self)->dup_intent (self);

  return NULL;
}
