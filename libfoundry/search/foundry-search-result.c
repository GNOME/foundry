/* foundry-search-result.c
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

#include "foundry-search-result.h"

G_DEFINE_ABSTRACT_TYPE (FoundrySearchResult, foundry_search_result, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_search_result_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundrySearchResult *self = FOUNDRY_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_take_string (value, foundry_search_result_dup_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_search_result_class_init (FoundrySearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_search_result_get_property;

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_search_result_init (FoundrySearchResult *self)
{
}

char *
foundry_search_result_dup_title (FoundrySearchResult *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SEARCH_RESULT (self), NULL);

  if (FOUNDRY_SEARCH_RESULT_GET_CLASS (self)->dup_title)
    return FOUNDRY_SEARCH_RESULT_GET_CLASS (self)->dup_title (self);

  return g_strdup (G_OBJECT_TYPE_NAME (self));
}
