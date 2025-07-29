/* foundry-input-text.c
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

#include "foundry-input-text.h"

typedef struct
{
  GRegex *regex;
  char   *value;
} FoundryInputTextPrivate;

enum {
  PROP_0,
  PROP_REGEX,
  PROP_VALUE,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (FoundryInputText, foundry_input_text, FOUNDRY_TYPE_INPUT)

static GParamSpec *properties[N_PROPS];

static void
foundry_input_text_dispose (GObject *object)
{
  FoundryInputText *self = (FoundryInputText *)object;
  FoundryInputTextPrivate *priv = foundry_input_text_get_instance_private (self);

  g_clear_pointer (&priv->regex, g_regex_unref);
  g_clear_pointer (&priv->value, g_free);

  G_OBJECT_CLASS (foundry_input_text_parent_class)->dispose (object);
}

static void
foundry_input_text_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryInputText *self = FOUNDRY_INPUT_TEXT (object);

  switch (prop_id)
    {
    case PROP_REGEX:
      g_value_take_boxed (value, foundry_input_text_dup_regex (self));
      break;

    case PROP_VALUE:
      g_value_take_string (value, foundry_input_text_dup_value (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_input_text_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  FoundryInputText *self = FOUNDRY_INPUT_TEXT (object);
  FoundryInputTextPrivate *priv = foundry_input_text_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_REGEX:
      priv->regex = g_value_dup_boxed (value);
      break;

    case PROP_VALUE:
      foundry_input_text_set_value (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_input_text_class_init (FoundryInputTextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_input_text_dispose;
  object_class->get_property = foundry_input_text_get_property;
  object_class->set_property = foundry_input_text_set_property;

  properties[PROP_REGEX] =
    g_param_spec_boxed ("regex", NULL, NULL,
                        G_TYPE_REGEX,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_VALUE] =
    g_param_spec_string ("value", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_input_text_init (FoundryInputText *self)
{
}

/**
 * foundry_input_text_new:
 * @title: the title of the input
 * @subtitle: (nullable): optional subtitle
 * @regex: (nullable): optional regex
 * @value: (nullable): optional initial value
 *
 * Returns: (transfer full):
 */
FoundryInput *
foundry_input_text_new (const char *title,
                        const char *subtitle,
                        GRegex     *regex,
                        const char *value)
{
  return g_object_new (FOUNDRY_TYPE_INPUT_TEXT,
                       "title", title,
                       "subtitle", subtitle,
                       "regex", regex,
                       "value", value,
                       NULL);
}

/**
 * foundry_input_text_dup_regex:
 * @self: a [class@Foundry.InputText]
 *
 * Returns: (transfer full) (nullable):
 */
GRegex *
foundry_input_text_dup_regex (FoundryInputText *self)
{
  FoundryInputTextPrivate *priv = foundry_input_text_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_INPUT_TEXT (self), NULL);

  if (priv->regex)
    return g_regex_ref (priv->regex);

  return NULL;
}

char *
foundry_input_text_dup_value (FoundryInputText *self)
{
  FoundryInputTextPrivate *priv = foundry_input_text_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_INPUT_TEXT (self), NULL);

  return g_strdup (priv->value);
}

void
foundry_input_text_set_value (FoundryInputText *self,
                              const char       *value)
{
  FoundryInputTextPrivate *priv = foundry_input_text_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_INPUT_TEXT (self));

  if (g_set_str (&priv->value, value))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE]);
}
