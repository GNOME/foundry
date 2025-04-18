/* plugin-devhelp-navigatable.c
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

#include <glib/gi18n.h>

#include "eggflattenlistmodel.h"
#include "eggmaplistmodel.h"

#include "plugin-devhelp-book.h"
#include "plugin-devhelp-heading.h"
#include "plugin-devhelp-keyword.h"
#include "plugin-devhelp-navigatable.h"

struct _PluginDevhelpNavigatable
{
  FoundryDocumentation parent_instance;

  GObject *item;
  GIcon *icon;
  GIcon *menu_icon;
  char *menu_title;
  char *title;
  char *uri;
};

G_DEFINE_FINAL_TYPE (PluginDevhelpNavigatable, plugin_devhelp_navigatable, FOUNDRY_TYPE_DOCUMENTATION)

enum {
  PROP_0,
  PROP_ICON,
  PROP_ITEM,
  PROP_MENU_ICON,
  PROP_MENU_TITLE,
  PROP_TITLE,
  PROP_URI,
  N_PROPS
};

enum {
  FIND_CHILDREN,
  FIND_PARENT,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];
static GIcon *book_symbolic;
static GIcon *library_symbolic;

static DexFuture *
plugin_devhelp_navigatable_not_supported (PluginDevhelpNavigatable *self)
{
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not Supported");
}

static DexFuture *
plugin_devhelp_navigatable_wrap_in_navigatable (DexFuture *completed,
                                                gpointer   user_data)
{
  const GValue *value;
  GObject *resource;

  g_assert (DEX_IS_FUTURE (completed));

  value = dex_future_get_value (completed, NULL);
  resource = g_value_get_object (value);

  return dex_future_new_take_object (plugin_devhelp_navigatable_new_for_resource (resource));
}

static gpointer
plugin_devhelp_navigatable_wrap_in_map_func (gpointer item,
                                             gpointer user_data)
{
  g_autoptr(GObject) object = item;

  return plugin_devhelp_navigatable_new_for_resource (object);
}

static DexFuture *
plugin_devhelp_navigatable_wrap_in_map (DexFuture *completed,
                                        gpointer   user_data)
{
  EggMapListModel *map;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));

  value = dex_future_get_value (completed, NULL);
  map = egg_map_list_model_new (g_value_dup_object (value),
                                plugin_devhelp_navigatable_wrap_in_map_func,
                                NULL, NULL);

  return dex_future_new_take_object (map);
}

static DexFuture *
join_future_models (DexFuture *completed,
                    gpointer   user_data)
{
  GListModel *first = g_value_get_object (dex_future_set_get_value_at (DEX_FUTURE_SET (completed), 0, NULL));
  GListModel *second = g_value_get_object (dex_future_set_get_value_at (DEX_FUTURE_SET (completed), 1, NULL));
  GListStore *store = g_list_store_new (G_TYPE_LIST_MODEL);

  g_list_store_append (store, first);
  g_list_store_append (store, second);

  return dex_future_new_take_object (egg_flatten_list_model_new (G_LIST_MODEL (store)));
}

static DexFuture *
plugin_devhelp_navigatable_find_parent_for_resource (PluginDevhelpNavigatable *self,
                                                     GObject                  *object)
{
  g_assert (PLUGIN_IS_DEVHELP_NAVIGATABLE (self));
  g_assert (G_IS_OBJECT (object));

  if (PLUGIN_IS_DEVHELP_SDK (object))
    {
      PluginDevhelpRepository *repository = NULL;
      g_object_get (object, "repository", &repository, NULL);
      return dex_future_then (dex_future_new_take_object (repository),
                              plugin_devhelp_navigatable_wrap_in_navigatable,
                              NULL, NULL);
    }

  if (PLUGIN_IS_DEVHELP_BOOK (object))
    return dex_future_then (plugin_devhelp_book_find_sdk (PLUGIN_DEVHELP_BOOK (object)),
                            plugin_devhelp_navigatable_wrap_in_navigatable,
                            NULL, NULL);

  if (PLUGIN_IS_DEVHELP_HEADING (object))
    return dex_future_then (plugin_devhelp_heading_find_parent (PLUGIN_DEVHELP_HEADING (object)),
                            plugin_devhelp_navigatable_wrap_in_navigatable,
                            NULL, NULL);

  if (PLUGIN_IS_DEVHELP_KEYWORD (object))
    return dex_future_then (plugin_devhelp_keyword_find_book (PLUGIN_DEVHELP_KEYWORD (object)),
                            plugin_devhelp_navigatable_wrap_in_navigatable,
                            NULL, NULL);

  return plugin_devhelp_navigatable_not_supported (self);
}

static DexFuture *
plugin_devhelp_navigatable_find_children_for_resource (PluginDevhelpNavigatable *self,
                                                       GObject                  *object)
{
  g_assert (PLUGIN_IS_DEVHELP_NAVIGATABLE (self));
  g_assert (G_IS_OBJECT (object));

  if (PLUGIN_IS_DEVHELP_REPOSITORY (object))
    return dex_future_then (plugin_devhelp_repository_list_sdks (PLUGIN_DEVHELP_REPOSITORY (object)),
                            plugin_devhelp_navigatable_wrap_in_map,
                            NULL, NULL);

  if (PLUGIN_IS_DEVHELP_HEADING (object))
    return dex_future_then (plugin_devhelp_heading_list_headings (PLUGIN_DEVHELP_HEADING (object)),
                            plugin_devhelp_navigatable_wrap_in_map,
                            NULL, NULL);

  if (PLUGIN_IS_DEVHELP_BOOK (object))
    return dex_future_then (plugin_devhelp_book_list_headings (PLUGIN_DEVHELP_BOOK (object)),
                            plugin_devhelp_navigatable_wrap_in_map,
                            NULL, NULL);

  if (PLUGIN_IS_DEVHELP_SDK (object))
    return dex_future_then (plugin_devhelp_sdk_list_books (PLUGIN_DEVHELP_SDK (object)),
                            plugin_devhelp_navigatable_wrap_in_map,
                            NULL, NULL);

  return plugin_devhelp_navigatable_not_supported (self);
}

static char *
plugin_devhelp_navigatable_real_dup_title (FoundryDocumentation *documentation)
{
  return g_strdup (PLUGIN_DEVHELP_NAVIGATABLE (documentation)->title);
}

static char *
plugin_devhelp_navigatable_real_dup_uri (FoundryDocumentation *documentation)
{
  return g_strdup (PLUGIN_DEVHELP_NAVIGATABLE (documentation)->uri);
}

static GIcon *
plugin_devhelp_navigatable_dup_icon (FoundryDocumentation *documentation)
{
  PluginDevhelpNavigatable *self = PLUGIN_DEVHELP_NAVIGATABLE (documentation);

  if (self->icon)
    return g_object_ref (self->icon);

  return NULL;
}

static DexFuture *
plugin_devhelp_navigatable_find_parent (FoundryDocumentation *documentation)
{
  PluginDevhelpNavigatable *self = (PluginDevhelpNavigatable *)documentation;
  DexFuture *future = NULL;

  g_assert (PLUGIN_IS_DEVHELP_NAVIGATABLE (self));

  g_signal_emit (self, signals[FIND_PARENT], 0, &future);

  return future;
}

static void
plugin_devhelp_navigatable_finalize (GObject *object)
{
  PluginDevhelpNavigatable *self = (PluginDevhelpNavigatable *)object;

  g_clear_pointer (&self->menu_title, g_free);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->uri, g_free);
  g_clear_object (&self->icon);
  g_clear_object (&self->menu_icon);
  g_clear_object (&self->item);

  G_OBJECT_CLASS (plugin_devhelp_navigatable_parent_class)->finalize (object);
}

static void
plugin_devhelp_navigatable_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  PluginDevhelpNavigatable *self = PLUGIN_DEVHELP_NAVIGATABLE (object);

  switch (prop_id)
    {
    case PROP_ICON:
      g_value_set_object (value, plugin_devhelp_navigatable_get_icon (self));
      break;

    case PROP_ITEM:
      g_value_set_object (value, plugin_devhelp_navigatable_get_item (self));
      break;

    case PROP_MENU_ICON:
      g_value_set_object (value, plugin_devhelp_navigatable_get_menu_icon (self));
      break;

    case PROP_MENU_TITLE:
      g_value_set_string (value, plugin_devhelp_navigatable_get_menu_title (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, plugin_devhelp_navigatable_get_title (self));
      break;

    case PROP_URI:
      g_value_set_string (value, plugin_devhelp_navigatable_get_uri (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_devhelp_navigatable_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  PluginDevhelpNavigatable *self = PLUGIN_DEVHELP_NAVIGATABLE (object);

  switch (prop_id)
    {
    case PROP_ICON:
      plugin_devhelp_navigatable_set_icon (self, g_value_get_object (value));
      break;

    case PROP_ITEM:
      plugin_devhelp_navigatable_set_item (self, g_value_get_object (value));
      break;

    case PROP_MENU_ICON:
      plugin_devhelp_navigatable_set_menu_icon (self, g_value_get_object (value));
      break;

    case PROP_MENU_TITLE:
      plugin_devhelp_navigatable_set_menu_title (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      plugin_devhelp_navigatable_set_title (self, g_value_get_string (value));
      break;

    case PROP_URI:
      plugin_devhelp_navigatable_set_uri (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_devhelp_navigatable_class_init (PluginDevhelpNavigatableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryDocumentationClass *documentation_class = FOUNDRY_DOCUMENTATION_CLASS (klass);

  object_class->finalize = plugin_devhelp_navigatable_finalize;
  object_class->get_property = plugin_devhelp_navigatable_get_property;
  object_class->set_property = plugin_devhelp_navigatable_set_property;

  documentation_class->dup_icon = plugin_devhelp_navigatable_dup_icon;
  documentation_class->dup_title = plugin_devhelp_navigatable_real_dup_title;
  documentation_class->dup_uri = plugin_devhelp_navigatable_real_dup_uri;
  documentation_class->find_parent = plugin_devhelp_navigatable_find_parent;

  properties[PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_MENU_ICON] =
    g_param_spec_object ("menu-icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_MENU_TITLE] =
    g_param_spec_string ("menu-title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[FIND_PARENT] =
    g_signal_new_class_handler ("find-parent",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (plugin_devhelp_navigatable_not_supported),
                                g_signal_accumulator_first_wins, NULL,
                                NULL,
                                G_TYPE_POINTER, 0);

  signals[FIND_CHILDREN] =
    g_signal_new_class_handler ("find-children",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (plugin_devhelp_navigatable_not_supported),
                                g_signal_accumulator_first_wins,
                                NULL,
                                NULL,
                                G_TYPE_POINTER, 0);
}

static void
plugin_devhelp_navigatable_init (PluginDevhelpNavigatable *self)
{
}

PluginDevhelpNavigatable *
plugin_devhelp_navigatable_new (void)
{
  return g_object_new (PLUGIN_TYPE_DEVHELP_NAVIGATABLE, NULL);
}

PluginDevhelpNavigatable *
plugin_devhelp_navigatable_new_for_resource (GObject *object)
{
  PluginDevhelpNavigatable *self;
  g_autoptr(GIcon) icon = NULL;
  g_autofree char *freeme_title = NULL;
  const char *title = NULL;
  const char *uri = NULL;

  g_return_val_if_fail (G_IS_OBJECT (object), NULL);

  if (PLUGIN_IS_DEVHELP_NAVIGATABLE (object))
    return g_object_ref (PLUGIN_DEVHELP_NAVIGATABLE (object));

  if (library_symbolic == NULL)
    library_symbolic = g_themed_icon_new ("library-symbolic");

  if (book_symbolic == NULL)
    book_symbolic = g_themed_icon_new ("book-symbolic");

  if (PLUGIN_IS_DEVHELP_REPOSITORY (object))
    {
      title = _("PluginDevhelp");
      icon = g_object_ref (library_symbolic);
      uri = NULL;
    }
  else if (PLUGIN_IS_DEVHELP_SDK (object))
    {
      PluginDevhelpSdk *sdk = PLUGIN_DEVHELP_SDK (object);
      const char *icon_name;

      title = freeme_title = plugin_devhelp_sdk_dup_title (sdk);
      icon_name = plugin_devhelp_sdk_get_icon_name (sdk);
      uri = NULL;

      if (icon_name != NULL)
        icon = g_themed_icon_new (icon_name);
    }
  else if (PLUGIN_IS_DEVHELP_BOOK (object))
    {
      PluginDevhelpBook *book = PLUGIN_DEVHELP_BOOK (object);

      title = plugin_devhelp_book_get_title (book);
      uri = plugin_devhelp_book_get_default_uri (book);
      icon = g_object_ref (book_symbolic);
    }
  else if (PLUGIN_IS_DEVHELP_HEADING (object))
    {
      PluginDevhelpHeading *heading = PLUGIN_DEVHELP_HEADING (object);

      title = plugin_devhelp_heading_get_title (heading);
      uri = plugin_devhelp_heading_get_uri (heading);
    }
  else if (PLUGIN_IS_DEVHELP_KEYWORD (object))
    {
      PluginDevhelpKeyword *keyword = PLUGIN_DEVHELP_KEYWORD (object);
      const char *icon_name = NULL;
      const char *kind;

      title = plugin_devhelp_keyword_get_name (keyword);
      uri = plugin_devhelp_keyword_get_uri (keyword);
      kind = plugin_devhelp_keyword_get_kind (keyword);

      if (g_strcmp0 (kind, "function") == 0)
        icon_name = "lang-function-symbolic";
      else if (g_strcmp0 (kind, "struct") == 0)
        icon_name = "lang-struct-symbolic";
      else if (g_strcmp0 (kind, "enum") == 0)
        icon_name = "lang-enum-symbolic";
      else if (g_strcmp0 (kind, "member") == 0)
        icon_name = "lang-struct-field-symbolic";
      else if (g_strcmp0 (kind, "constant") == 0)
        icon_name = "lang-constant-symbolic";
      else if (g_strcmp0 (kind, "macro") == 0)
        icon_name = "lang-macro-symbolic";

      if (title != NULL && g_str_has_prefix (title, "The "))
        {
          if (g_str_has_suffix (title, " property"))
            icon_name = "lang-method-symbolic";
          else if (g_str_has_suffix (title, " method"))
            icon_name = "lang-method-symbolic";
          else if (g_str_has_suffix (title, " signal"))
            icon_name = "lang-signal-symbolic";
        }

      if (icon_name != NULL)
        icon = g_themed_icon_new (icon_name);
    }

  self = g_object_new (PLUGIN_TYPE_DEVHELP_NAVIGATABLE,
                       "uri", uri,
                       "title", title,
                       "icon", icon,
                       "item", object,
                       NULL);

  g_signal_connect_object (self,
                           "find-parent",
                           G_CALLBACK (plugin_devhelp_navigatable_find_parent_for_resource),
                           object,
                           0);

  g_signal_connect_object (self,
                           "find-children",
                           G_CALLBACK (plugin_devhelp_navigatable_find_children_for_resource),
                           object,
                           0);

  return g_steal_pointer (&self);
}

GIcon *
plugin_devhelp_navigatable_get_icon (PluginDevhelpNavigatable *self)
{
  g_return_val_if_fail (PLUGIN_IS_DEVHELP_NAVIGATABLE (self), NULL);

  return self->icon;
}

void
plugin_devhelp_navigatable_set_icon (PluginDevhelpNavigatable *self,
                                     GIcon                    *icon)
{
  g_return_if_fail (PLUGIN_IS_DEVHELP_NAVIGATABLE (self));

  if (g_set_object (&self->icon, icon))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON]);

      if (self->menu_icon == NULL)
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MENU_ICON]);
    }
}

GIcon *
plugin_devhelp_navigatable_get_menu_icon (PluginDevhelpNavigatable *self)
{
  g_return_val_if_fail (PLUGIN_IS_DEVHELP_NAVIGATABLE (self), NULL);

  if (self->menu_icon == NULL)
    return self->icon;

  return self->menu_icon;
}

void
plugin_devhelp_navigatable_set_menu_icon (PluginDevhelpNavigatable *self,
                                          GIcon                    *menu_icon)
{
  g_return_if_fail (PLUGIN_IS_DEVHELP_NAVIGATABLE (self));
  g_return_if_fail (!menu_icon || G_IS_ICON (menu_icon));

  if (g_set_object (&self->menu_icon, menu_icon))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MENU_ICON]);
}

const char *
plugin_devhelp_navigatable_get_title (PluginDevhelpNavigatable *self)
{
  g_return_val_if_fail (PLUGIN_IS_DEVHELP_NAVIGATABLE (self), NULL);

  return self->title;
}

void
plugin_devhelp_navigatable_set_title (PluginDevhelpNavigatable *self,
                                      const char               *title)
{
  g_return_if_fail (PLUGIN_IS_DEVHELP_NAVIGATABLE (self));

  if (g_set_str (&self->title, title))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);

      if (self->menu_title == NULL)
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MENU_TITLE]);
    }
}

const char *
plugin_devhelp_navigatable_get_menu_title (PluginDevhelpNavigatable *self)
{
  g_return_val_if_fail (PLUGIN_IS_DEVHELP_NAVIGATABLE (self), NULL);

  if (self->menu_title == NULL)
    return self->title;

  return self->menu_title;
}

void
plugin_devhelp_navigatable_set_menu_title (PluginDevhelpNavigatable *self,
                                           const char               *menu_title)
{
  g_return_if_fail (PLUGIN_IS_DEVHELP_NAVIGATABLE (self));

  if (g_set_str (&self->menu_title, menu_title))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MENU_TITLE]);
}

const char *
plugin_devhelp_navigatable_get_uri (PluginDevhelpNavigatable *self)
{
  g_return_val_if_fail (PLUGIN_IS_DEVHELP_NAVIGATABLE (self), NULL);

  return self->uri;
}

void
plugin_devhelp_navigatable_set_uri (PluginDevhelpNavigatable *self,
                                    const char               *uri)
{
  g_return_if_fail (PLUGIN_IS_DEVHELP_NAVIGATABLE (self));

  if (g_set_str (&self->uri, uri))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_URI]);
}

DexFuture *
plugin_devhelp_navigatable_find_children (PluginDevhelpNavigatable *self)
{
  DexFuture *future = NULL;

  g_return_val_if_fail (PLUGIN_IS_DEVHELP_NAVIGATABLE (self), NULL);

  g_signal_emit (self, signals[FIND_CHILDREN], 0, &future);

  return future;
}

gpointer
plugin_devhelp_navigatable_get_item (PluginDevhelpNavigatable *self)
{
  g_return_val_if_fail (PLUGIN_IS_DEVHELP_NAVIGATABLE (self), NULL);

  return self->item;
}

void
plugin_devhelp_navigatable_set_item (PluginDevhelpNavigatable *self,
                                     gpointer                  item)
{
  g_return_if_fail (PLUGIN_IS_DEVHELP_NAVIGATABLE (self));
  g_return_if_fail (!item || G_IS_OBJECT (item));

  if (g_set_object (&self->item, item))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}

static DexFuture *
plugin_devhelp_navigatable_find_parents_children (DexFuture *completed,
                                                  gpointer   user_data)
{
  g_autoptr(PluginDevhelpNavigatable) parent = NULL;

  g_assert (DEX_IS_FUTURE (completed));

  parent = dex_await_object (dex_ref (completed), NULL);

  return plugin_devhelp_navigatable_find_children (parent);
}


DexFuture *
plugin_devhelp_navigatable_find_peers (PluginDevhelpNavigatable *self)
{
  DexFuture *alternates;

  g_return_val_if_fail (PLUGIN_IS_DEVHELP_NAVIGATABLE (self), NULL);

  if (PLUGIN_IS_DEVHELP_HEADING (self->item))
    alternates = plugin_devhelp_heading_list_alternates (PLUGIN_DEVHELP_HEADING (self->item));
  else if (PLUGIN_IS_DEVHELP_KEYWORD (self->item))
    alternates = plugin_devhelp_keyword_list_alternates (PLUGIN_DEVHELP_KEYWORD (self->item));
  else if (PLUGIN_IS_DEVHELP_BOOK (self->item))
    alternates = plugin_devhelp_book_list_alternates (PLUGIN_DEVHELP_BOOK (self->item));
  else
    alternates = dex_future_new_take_object (g_list_store_new (PLUGIN_TYPE_DEVHELP_NAVIGATABLE));

  return dex_future_then (dex_future_all (alternates,
                                          dex_future_then (foundry_documentation_find_parent (FOUNDRY_DOCUMENTATION (self)),
                                                           plugin_devhelp_navigatable_find_parents_children,
                                                           NULL, NULL),
                                          NULL),
                          join_future_models,
                          NULL, NULL);
}

