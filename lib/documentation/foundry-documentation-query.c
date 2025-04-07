/* foundry-documentation-query.c
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

#include "foundry-documentation-query.h"

struct _FoundryDocumentationQuery
{
  GObject parent_instance;
  char *keyword;
};

enum {
  PROP_0,
  PROP_KEYWORD,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryDocumentationQuery, foundry_documentation_query, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_documentation_query_finalize (GObject *object)
{
  FoundryDocumentationQuery *self = (FoundryDocumentationQuery *)object;

  g_clear_pointer (&self->keyword, g_free);

  G_OBJECT_CLASS (foundry_documentation_query_parent_class)->finalize (object);
}

static void
foundry_documentation_query_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  FoundryDocumentationQuery *self = FOUNDRY_DOCUMENTATION_QUERY (object);

  switch (prop_id)
    {
    case PROP_KEYWORD:
      g_value_take_string (value, foundry_documentation_query_dup_keyword (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_documentation_query_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  FoundryDocumentationQuery *self = FOUNDRY_DOCUMENTATION_QUERY (object);

  switch (prop_id)
    {
    case PROP_KEYWORD:
      foundry_documentation_query_set_keyword (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_documentation_query_class_init (FoundryDocumentationQueryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_documentation_query_finalize;
  object_class->get_property = foundry_documentation_query_get_property;
  object_class->set_property = foundry_documentation_query_set_property;

  properties[PROP_KEYWORD] =
    g_param_spec_string ("keyword", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_documentation_query_init (FoundryDocumentationQuery *self)
{
}

FoundryDocumentationQuery *
foundry_documentation_query_new (void)
{
  return g_object_new (FOUNDRY_TYPE_DOCUMENTATION_QUERY, NULL);
}

char *
foundry_documentation_query_dup_keyword (FoundryDocumentationQuery *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DOCUMENTATION_QUERY (self), NULL);

  return g_strdup (self->keyword);
}

void
foundry_documentation_query_set_keyword (FoundryDocumentationQuery *self,
                                         const char                *keyword)
{
  g_return_if_fail (FOUNDRY_IS_DOCUMENTATION_QUERY (self));

  if (g_set_str (&self->keyword, keyword))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KEYWORD]);
}
