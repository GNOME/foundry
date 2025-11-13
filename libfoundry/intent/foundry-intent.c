/* foundry-intent.c
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

#include <gobject/gvaluecollector.h>

#include "foundry-intent.h"

/**
 * FoundryIntent:
 *
 * Abstract base class for representing intents.
 *
 * FoundryIntent provides the core interface for representing user intents
 * and actions within the development environment. It supports attribute
 * storage and provides a unified interface for intent handling across
 * different parts of the development environment.
 */

typedef struct
{
  GHashTable *attributes;
} FoundryIntentPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryIntent, foundry_intent, G_TYPE_OBJECT)

static void
_g_value_free (gpointer data)
{
  GValue *value = data;

  g_value_unset (value);
  g_free (value);
}

static void
foundry_intent_finalize (GObject *object)
{
  FoundryIntent *self = (FoundryIntent *)object;
  FoundryIntentPrivate *priv = foundry_intent_get_instance_private (self);

  g_clear_pointer (&priv->attributes, g_hash_table_unref);

  G_OBJECT_CLASS (foundry_intent_parent_class)->finalize (object);
}

static void
foundry_intent_class_init (FoundryIntentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_intent_finalize;
}

static void
foundry_intent_init (FoundryIntent *self)
{
  FoundryIntentPrivate *priv = foundry_intent_get_instance_private (self);

  priv->attributes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, _g_value_free);
}

/**
 * foundry_intent_has_attribute:
 * @self: a [class@Foundry.Intent]
 * @attribute: the attribute name to check
 *
 * Checks if the intent has an attribute with the given name.
 *
 * Returns: %TRUE if the attribute exists, %FALSE otherwise
 *
 * Since: 1.1
 */
gboolean
foundry_intent_has_attribute (FoundryIntent *self,
                              const char    *attribute)
{
  FoundryIntentPrivate *priv = foundry_intent_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_INTENT (self), FALSE);
  g_return_val_if_fail (attribute != NULL, FALSE);

  return g_hash_table_contains (priv->attributes, attribute);
}

/**
 * foundry_intent_set_attribute_value:
 * @self: a [class@Foundry.Intent]
 * @attribute: the attribute name
 * @value: the #GValue to set
 *
 * Sets an attribute value for the intent. The value is copied.
 *
 * Since: 1.1
 */
void
foundry_intent_set_attribute_value (FoundryIntent *self,
                                    const char    *attribute,
                                    const GValue  *value)
{
  FoundryIntentPrivate *priv = foundry_intent_get_instance_private (self);
  GValue *copy;

  g_return_if_fail (FOUNDRY_IS_INTENT (self));
  g_return_if_fail (attribute != NULL);
  g_return_if_fail (value != NULL);

  copy = g_new0 (GValue, 1);
  g_value_init (copy, G_VALUE_TYPE (value));
  g_value_copy (value, copy);

  g_hash_table_insert (priv->attributes, g_strdup (attribute), copy);
}

/**
 * foundry_intent_get_attribute_type:
 * @self: a [class@Foundry.Intent]
 * @attribute: the attribute name
 *
 * Gets the #GType of the attribute value.
 *
 * Returns: the #GType of the attribute, or %G_TYPE_INVALID if not found
 *
 * Since: 1.1
 */
GType
foundry_intent_get_attribute_type (FoundryIntent *self,
                                   const char    *attribute)
{
  FoundryIntentPrivate *priv = foundry_intent_get_instance_private (self);
  GValue *value;

  g_return_val_if_fail (FOUNDRY_IS_INTENT (self), G_TYPE_INVALID);
  g_return_val_if_fail (attribute != NULL, G_TYPE_INVALID);

  if (!(value = g_hash_table_lookup (priv->attributes, attribute)))
    return G_TYPE_INVALID;

  return G_VALUE_TYPE (value);
}

/**
 * foundry_intent_get_attribute_value:
 * @self: a [class@Foundry.Intent]
 * @attribute: the attribute name
 *
 * Gets the attribute value. The returned value is owned by the intent
 * and should not be modified or freed.
 *
 * Returns: (transfer none): the #GValue, or %NULL if not found
 *
 * Since: 1.1
 */
const GValue *
foundry_intent_get_attribute_value (FoundryIntent *self,
                                    const char    *attribute)
{
  FoundryIntentPrivate *priv = foundry_intent_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_INTENT (self), NULL);
  g_return_val_if_fail (attribute != NULL, NULL);

  return g_hash_table_lookup (priv->attributes, attribute);
}

/**
 * foundry_intent_dup_attribute_string:
 * @self: a [class@Foundry.Intent]
 * @attribute: the attribute name
 *
 * Gets the attribute value as a string. The returned string is owned
 * by the intent and should not be modified or freed.
 *
 * Returns: (transfer full) (nullable): the string value, or %NULL if not found or not a string
 *
 * Since: 1.1
 */
char *
foundry_intent_dup_attribute_string (FoundryIntent *self,
                                     const char    *attribute)
{
  const GValue *value;

  g_return_val_if_fail (FOUNDRY_IS_INTENT (self), NULL);
  g_return_val_if_fail (attribute != NULL, NULL);

  value = foundry_intent_get_attribute_value (self, attribute);
  if (value == NULL || !G_VALUE_HOLDS_STRING (value))
    return NULL;

  return g_value_dup_string (value);
}

/**
 * foundry_intent_dup_attribute_strv:
 * @self: a [class@Foundry.Intent]
 * @attribute: the attribute name
 *
 * Gets the attribute value as a string. The returned string is owned
 * by the intent and should not be modified or freed.
 *
 * Returns: (transfer full): the string value, or %NULL if not found or not a string
 *
 * Since: 1.1
 */
char **
foundry_intent_dup_attribute_strv (FoundryIntent *self,
                                   const char    *attribute)
{
  const GValue *value;

  g_return_val_if_fail (FOUNDRY_IS_INTENT (self), NULL);
  g_return_val_if_fail (attribute != NULL, NULL);

  value = foundry_intent_get_attribute_value (self, attribute);
  if (value == NULL || !G_VALUE_HOLDS (value, G_TYPE_STRV))
    return NULL;

  return g_value_dup_boxed (value);
}

/**
 * foundry_intent_get_attribute_boolean:
 * @self: a [class@Foundry.Intent]
 * @attribute: the attribute name
 *
 * Gets the attribute value as a boolean.
 *
 * Returns: the boolean value, or %FALSE if not found or not a boolean
 *
 * Since: 1.1
 */
gboolean
foundry_intent_get_attribute_boolean (FoundryIntent *self,
                                      const char    *attribute)
{
  const GValue *value;

  g_return_val_if_fail (FOUNDRY_IS_INTENT (self), FALSE);
  g_return_val_if_fail (attribute != NULL, FALSE);

  value = foundry_intent_get_attribute_value (self, attribute);
  if (value == NULL || !G_VALUE_HOLDS_BOOLEAN (value))
    return FALSE;

  return g_value_get_boolean (value);
}

/**
 * foundry_intent_dup_attribute_object:
 * @self: a [class@Foundry.Intent]
 * @attribute: the attribute name
 *
 * Gets the attribute value as a [class@GObject.Object]. The returned object is
 * owned by the intent and should not be unref'd.
 *
 * Returns: (transfer full) (nullable): the [class@GObject.Object], or %NULL
 *   if not found or not an object
 *
 * Since: 1.1
 */
gpointer
foundry_intent_dup_attribute_object (FoundryIntent *self,
                                     const char    *attribute)
{
  const GValue *value;

  g_return_val_if_fail (FOUNDRY_IS_INTENT (self), NULL);
  g_return_val_if_fail (attribute != NULL, NULL);

  value = foundry_intent_get_attribute_value (self, attribute);
  if (value == NULL || !G_VALUE_HOLDS_OBJECT (value))
    return NULL;

  return g_value_dup_object (value);
}

void
foundry_intent_set_attribute (FoundryIntent *self,
                              const char    *attribute,
                              GType          type,
                              ...)
{
  FoundryIntentPrivate *priv = foundry_intent_get_instance_private (self);
  g_autofree char *errmsg = NULL;
  GValue *value;
  va_list args;

  g_return_if_fail (FOUNDRY_IS_INTENT (self));
  g_return_if_fail (attribute != NULL);
  g_return_if_fail (type != G_TYPE_INVALID);

  value = g_new0 (GValue, 1);
  g_value_init (value, type);

  va_start (args, type);
  G_VALUE_COLLECT (value, args, 0, &errmsg);
  va_end (args);

  if (errmsg != NULL)
    {
      g_warning ("Failed to collect value of type `%s`: %s",
                 g_type_name (type), errmsg);
      g_value_unset (value);
      g_free (value);
      return;
    }

  g_hash_table_replace (priv->attributes,
                        g_strdup (attribute),
                        g_steal_pointer (&value));
}
