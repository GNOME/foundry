/* foundry-search-request.c
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

#include "foundry-search-request.h"

struct _FoundrySearchRequest
{
  FoundryContextual parent_instance;
  char *search_text;
};

enum {
  PROP_0,
  PROP_SEARCH_TEXT,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundrySearchRequest, foundry_search_request, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static void
foundry_search_request_finalize (GObject *object)
{
  FoundrySearchRequest *self = (FoundrySearchRequest *)object;

  g_clear_pointer (&self->search_text, g_free);

  G_OBJECT_CLASS (foundry_search_request_parent_class)->finalize (object);
}

static void
foundry_search_request_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  FoundrySearchRequest *self = FOUNDRY_SEARCH_REQUEST (object);

  switch (prop_id)
    {
    case PROP_SEARCH_TEXT:
      g_value_take_string (value, foundry_search_request_dup_search_text (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_search_request_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  FoundrySearchRequest *self = FOUNDRY_SEARCH_REQUEST (object);

  switch (prop_id)
    {
    case PROP_SEARCH_TEXT:
      self->search_text = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_search_request_class_init (FoundrySearchRequestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_search_request_finalize;
  object_class->get_property = foundry_search_request_get_property;
  object_class->set_property = foundry_search_request_set_property;

  properties[PROP_SEARCH_TEXT] =
    g_param_spec_string ("search-text", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_search_request_init (FoundrySearchRequest *self)
{
}

FoundrySearchRequest *
foundry_search_request_new (FoundryContext *context,
                            const char     *search_text)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (search_text != NULL, NULL);

  return g_object_new (FOUNDRY_TYPE_SEARCH_REQUEST,
                       "context", context,
                       "search-text", search_text,
                       NULL);
}

/**
 * foundry_search_request_dup_search_text:
 * @self: a [class@Foundry.SearchRequest]
 *
 * Returns: (transfer full):
 */
char *
foundry_search_request_dup_search_text (FoundrySearchRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SEARCH_REQUEST (self), NULL);

  return g_strdup (self->search_text);
}
