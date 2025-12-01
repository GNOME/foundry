/* foundry-web-intent.c
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

#include "foundry-web-intent.h"
#include "foundry-util.h"

/**
 * FoundryWebIntent:
 *
 * Represents an intent to open a URI with a web browser.
 *
 * FoundryWebIntent provides functionality for opening URIs in web browsers.
 * The URI is stored as a plain UTF-8 string and is up to the caller to
 * ensure that the URI is valid.
 */

struct _FoundryWebIntent
{
  FoundryIntent parent_instance;
};

enum {
  PROP_0,
  PROP_URI,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryWebIntent, foundry_web_intent, FOUNDRY_TYPE_INTENT)

static GParamSpec *properties[N_PROPS];

static void
foundry_web_intent_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryWebIntent *self = FOUNDRY_WEB_INTENT (object);

  switch (prop_id)
    {
    case PROP_URI:
      g_value_take_string (value, foundry_web_intent_dup_uri (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_web_intent_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  FoundryWebIntent *self = FOUNDRY_WEB_INTENT (object);

  switch (prop_id)
    {
    case PROP_URI:
      foundry_intent_set_attribute (FOUNDRY_INTENT (self),
                                    "uri",
                                    G_TYPE_STRING,
                                    g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_web_intent_class_init (FoundryWebIntentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_web_intent_get_property;
  object_class->set_property = foundry_web_intent_set_property;

  properties[PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_web_intent_init (FoundryWebIntent *self)
{
}

/**
 * foundry_web_intent_new:
 * @uri: a UTF-8 encoded URI string
 *
 * Creates a new [class@Foundry.WebIntent] for opening a URI in a web browser.
 *
 * The URI is stored as a plain UTF-8 string without any parsing or processing.
 *
 * Returns: (transfer full): a new [class@Foundry.WebIntent]
 */
FoundryIntent *
foundry_web_intent_new (const char *uri)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return g_object_new (FOUNDRY_TYPE_WEB_INTENT,
                       "uri", uri,
                       NULL);
}

/**
 * foundry_web_intent_dup_uri:
 * @self: a [class@Foundry.WebIntent]
 *
 * Gets a copy of the URI string.
 *
 * Returns: (transfer full) (nullable): a newly allocated copy of the URI string,
 *   or %NULL if no URI is set
 */
char *
foundry_web_intent_dup_uri (FoundryWebIntent *self)
{
  const GValue *value;

  g_return_val_if_fail (FOUNDRY_IS_WEB_INTENT (self), NULL);

  if ((value = foundry_intent_get_attribute_value (FOUNDRY_INTENT (self), "uri")) &&
      G_VALUE_HOLDS_STRING (value))
    return g_value_dup_string (value);

  return NULL;
}
