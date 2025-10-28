/* foundry-open-file-intent.c
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

#include "foundry-open-file-intent.h"
#include "foundry-util.h"

/**
 * FoundryOpenFileIntent:
 *
 * Represents an intent to open a file.
 *
 * FoundryOpenFileIntent provides functionality for opening files with
 * specific content types and handling file opening operations. It
 * integrates with the intent system to provide a unified interface
 * for file opening operations across different parts of the development
 * environment.
 */

struct _FoundryOpenFileIntent
{
  FoundryIntent parent_instance;
};

enum {
  PROP_0,
  PROP_CONTENT_TYPE,
  PROP_FILE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryOpenFileIntent, foundry_open_file_intent, FOUNDRY_TYPE_INTENT)

static GParamSpec *properties[N_PROPS];

static void
foundry_open_file_intent_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  FoundryOpenFileIntent *self = FOUNDRY_OPEN_FILE_INTENT (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_take_object (value, foundry_open_file_intent_dup_file (self));
      break;

    case PROP_CONTENT_TYPE:
      g_value_take_string (value, foundry_open_file_intent_dup_content_type (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_open_file_intent_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  FoundryOpenFileIntent *self = FOUNDRY_OPEN_FILE_INTENT (object);

  switch (prop_id)
    {
    case PROP_FILE:
      foundry_intent_set_attribute (FOUNDRY_INTENT (self),
                                    "file",
                                    G_TYPE_FILE,
                                    g_value_get_object (value));
      break;

    case PROP_CONTENT_TYPE:
      foundry_intent_set_attribute (FOUNDRY_INTENT (self),
                                    "content-type",
                                    G_TYPE_STRING,
                                    g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_open_file_intent_class_init (FoundryOpenFileIntentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_open_file_intent_get_property;
  object_class->set_property = foundry_open_file_intent_set_property;

  properties[PROP_CONTENT_TYPE] =
    g_param_spec_string ("content-type", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_open_file_intent_init (FoundryOpenFileIntent *self)
{
}

FoundryIntent *
foundry_open_file_intent_new (GFile      *file,
                              const char *content_type)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (FOUNDRY_TYPE_OPEN_FILE_INTENT,
                       "file", file,
                       "content-type", content_type,
                       NULL);
}

/**
 * foundry_open_file_intent_dup_file:
 * @self: a [class@Foundry.OpenFileIntent]
 *
 * Returns: (transfer full) (nullable):
 */
GFile *
foundry_open_file_intent_dup_file (FoundryOpenFileIntent *self)
{
  const GValue *value;

  g_return_val_if_fail (FOUNDRY_IS_OPEN_FILE_INTENT (self), NULL);

  if ((value = foundry_intent_get_attribute_value (FOUNDRY_INTENT (self), "file")) &&
      G_VALUE_HOLDS (value, G_TYPE_FILE))
    return g_value_dup_object (value);

  return NULL;
}

/**
 * foundry_open_file_intent_dup_content_type:
 * @self: a [class@Foundry.OpenFileIntent]
 *
 * Returns: (transfer full) (nullable):
 */
char *
foundry_open_file_intent_dup_content_type (FoundryOpenFileIntent *self)
{
  const GValue *value;

  g_return_val_if_fail (FOUNDRY_IS_OPEN_FILE_INTENT (self), NULL);

  if ((value = foundry_intent_get_attribute_value (FOUNDRY_INTENT (self), "content-type")) &&
      G_VALUE_HOLDS_STRING (value))
    return g_value_dup_string (value);

  return NULL;
}
