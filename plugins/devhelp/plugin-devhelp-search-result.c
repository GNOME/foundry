/* plugin-devhelp-search-result.c
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

#include "plugin-devhelp-navigatable.h"
#include "plugin-devhelp-search-result.h"

G_DEFINE_FINAL_TYPE (PluginDevhelpSearchResult, plugin_devhelp_search_result, FOUNDRY_TYPE_DOCUMENTATION)

enum {
  PROP_0,
  PROP_ITEM,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

PluginDevhelpSearchResult *
plugin_devhelp_search_result_new (guint position)
{
  PluginDevhelpSearchResult *self;

  self = g_object_new (PLUGIN_TYPE_DEVHELP_SEARCH_RESULT, NULL);
  self->position = position;

  return self;
}

static char *
plugin_devhelp_search_result_dup_title (FoundryDocumentation *documentation)
{
  PluginDevhelpSearchResult *self = PLUGIN_DEVHELP_SEARCH_RESULT (documentation);

  if (PLUGIN_IS_DEVHELP_NAVIGATABLE (self->item))
    return g_strdup (plugin_devhelp_navigatable_get_title (PLUGIN_DEVHELP_NAVIGATABLE (self->item)));

  return NULL;
}

static char *
plugin_devhelp_search_result_dup_uri (FoundryDocumentation *documentation)
{
  PluginDevhelpSearchResult *self = PLUGIN_DEVHELP_SEARCH_RESULT (documentation);

  if (PLUGIN_IS_DEVHELP_NAVIGATABLE (self->item))
    return g_strdup (plugin_devhelp_navigatable_get_uri (PLUGIN_DEVHELP_NAVIGATABLE (self->item)));

  return NULL;
}

static void
plugin_devhelp_search_result_dispose (GObject *object)
{
  PluginDevhelpSearchResult *self = (PluginDevhelpSearchResult *)object;

  if (self->model)
    plugin_devhelp_search_model_release (self->model, self);

  self->model = NULL;
  self->link.data = NULL;

  g_clear_object (&self->item);

  G_OBJECT_CLASS (plugin_devhelp_search_result_parent_class)->dispose (object);
}

static void
plugin_devhelp_search_result_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  PluginDevhelpSearchResult *self = PLUGIN_DEVHELP_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, plugin_devhelp_search_result_get_item (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_devhelp_search_result_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  PluginDevhelpSearchResult *self = PLUGIN_DEVHELP_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      plugin_devhelp_search_result_set_item (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_devhelp_search_result_class_init (PluginDevhelpSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryDocumentationClass *documentation_class = FOUNDRY_DOCUMENTATION_CLASS (klass);

  object_class->dispose = plugin_devhelp_search_result_dispose;
  object_class->get_property = plugin_devhelp_search_result_get_property;
  object_class->set_property = plugin_devhelp_search_result_set_property;

  documentation_class->dup_title = plugin_devhelp_search_result_dup_title;
  documentation_class->dup_uri = plugin_devhelp_search_result_dup_uri;

  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_devhelp_search_result_init (PluginDevhelpSearchResult *self)
{
  self->link.data = self;
}

gpointer
plugin_devhelp_search_result_get_item (PluginDevhelpSearchResult *self)
{
  g_return_val_if_fail (PLUGIN_IS_DEVHELP_SEARCH_RESULT (self), NULL);

  return self->item;
}

void
plugin_devhelp_search_result_set_item (PluginDevhelpSearchResult *self,
                                       gpointer                   item)
{
  g_return_if_fail (PLUGIN_IS_DEVHELP_SEARCH_RESULT (self));
  g_return_if_fail (!item || G_IS_OBJECT (item));

  if (g_set_object (&self->item, item))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}

guint
plugin_devhelp_search_result_get_position (PluginDevhelpSearchResult *self)
{
  g_return_val_if_fail (PLUGIN_IS_DEVHELP_SEARCH_RESULT (self), 0);

  return self->position;
}
