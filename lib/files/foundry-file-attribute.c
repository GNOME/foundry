/* foundry-file-attribute.c
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

#include "foundry-file-attribute-private.h"

struct _FoundryFileAttribute
{
  GObject  parent_instance;
  gint64   id;
  char    *key;
  char    *uri;
  GBytes  *value;
};

enum {
  PROP_0,
  PROP_ID,
  PROP_KEY,
  PROP_URI,
  PROP_VALUE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryFileAttribute, foundry_file_attribute, GOM_TYPE_RESOURCE)

static GParamSpec *properties[N_PROPS];

static void
foundry_file_attribute_finalize (GObject *object)
{
  FoundryFileAttribute *self = (FoundryFileAttribute *)object;

  g_clear_pointer (&self->key, g_free);
  g_clear_pointer (&self->uri, g_free);
  g_clear_pointer (&self->value, g_bytes_unref);

  G_OBJECT_CLASS (foundry_file_attribute_parent_class)->finalize (object);
}

static void
foundry_file_attribute_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  FoundryFileAttribute *self = FOUNDRY_FILE_ATTRIBUTE (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case PROP_URI:
      g_value_take_string (value, foundry_file_attribute_dup_uri (self));
      break;

    case PROP_KEY:
      g_value_take_string (value, foundry_file_attribute_dup_key (self));
      break;

    case PROP_VALUE:
      g_value_take_boxed (value, foundry_file_attribute_dup_value (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_file_attribute_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  FoundryFileAttribute *self = FOUNDRY_FILE_ATTRIBUTE (object);

  switch (prop_id)
    {
    case PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case PROP_URI:
      foundry_file_attribute_set_uri (self, g_value_get_string (value));
      break;

    case PROP_KEY:
      foundry_file_attribute_set_key (self, g_value_get_string (value));
      break;

    case PROP_VALUE:
      foundry_file_attribute_set_value (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_file_attribute_class_init (FoundryFileAttributeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomResourceClass *resource_class = GOM_RESOURCE_CLASS (klass);

  object_class->finalize = foundry_file_attribute_finalize;
  object_class->get_property = foundry_file_attribute_get_property;
  object_class->set_property = foundry_file_attribute_set_property;

  properties[PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_KEY] =
    g_param_spec_string ("key", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_VALUE] =
    g_param_spec_boxed ("value", NULL, NULL,
                         G_TYPE_BYTES,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gom_resource_class_set_table (resource_class, "attributes");
  gom_resource_class_set_primary_key (resource_class, "id");
  gom_resource_class_set_notnull (resource_class, "uri");
  gom_resource_class_set_notnull (resource_class, "key");
}

static void
foundry_file_attribute_init (FoundryFileAttribute *self)
{
}

char *
foundry_file_attribute_dup_uri (FoundryFileAttribute *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_ATTRIBUTE (self), NULL);

  return g_strdup (self->uri);
}

void
foundry_file_attribute_set_uri (FoundryFileAttribute *self,
                                const char           *uri)
{
  g_return_if_fail (FOUNDRY_IS_FILE_ATTRIBUTE (self));
  g_return_if_fail (uri != NULL);

  if (g_set_str (&self->uri, uri))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_URI]);
}

char *
foundry_file_attribute_dup_key (FoundryFileAttribute *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_ATTRIBUTE (self), NULL);

  return g_strdup (self->key);
}

void
foundry_file_attribute_set_key (FoundryFileAttribute *self,
                                const char           *key)
{
  g_return_if_fail (FOUNDRY_IS_FILE_ATTRIBUTE (self));
  g_return_if_fail (key != NULL);

  if (g_set_str (&self->key, key))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KEY]);
}

GBytes *
foundry_file_attribute_dup_value (FoundryFileAttribute *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_ATTRIBUTE (self), NULL);

  return self->value ? g_bytes_ref (self->value) : NULL;
}

void
foundry_file_attribute_set_value (FoundryFileAttribute *self,
                                  GBytes               *value)
{
  g_return_if_fail (FOUNDRY_IS_FILE_ATTRIBUTE (self));

  if (self->value == value)
    return;

  if (value)
    g_bytes_ref (value);

  if (self->value)
    g_bytes_unref (self->value);

  self->value = value;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE]);
}

char *
foundry_file_attribute_dup_value_string (FoundryFileAttribute *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_ATTRIBUTE (self), NULL);

  if (self->value == NULL)
    return NULL;

  return g_strndup (g_bytes_get_data (self->value, NULL),
                    g_bytes_get_size (self->value));
}

gboolean
foundry_file_attribute_get_value_boolean (FoundryFileAttribute *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_ATTRIBUTE (self), FALSE);

  if (self->value &&
      g_bytes_get_size (self->value) > 0 &&
      ((const guint8 *)g_bytes_get_data (self->value, NULL))[0])
    return TRUE;

  return FALSE;
}

double
foundry_file_attribute_get_value_double (FoundryFileAttribute *self)
{
  double value = 0;

  g_return_val_if_fail (FOUNDRY_IS_FILE_ATTRIBUTE (self), FALSE);

  if (self->value &&
      g_bytes_get_size (self->value) >= sizeof (double))
    memcpy (&value, g_bytes_get_data (self->value, NULL), sizeof (double));

  return value;
}

void
foundry_file_attribute_set_value_string (FoundryFileAttribute *self,
                                         const char           *value)
{
  g_autoptr(GBytes) bytes = NULL;

  g_return_if_fail (FOUNDRY_IS_FILE_ATTRIBUTE (self));

  if (value != NULL)
    bytes = g_bytes_new_take (g_strdup (value), strlen (value));

  foundry_file_attribute_set_value (self, bytes);
}

void
foundry_file_attribute_set_value_double (FoundryFileAttribute *self,
                                         double                value)
{
  g_autoptr(GBytes) bytes = NULL;

  g_return_if_fail (FOUNDRY_IS_FILE_ATTRIBUTE (self));

  if (value != 0.0)
    bytes = g_bytes_new (&value, sizeof value);

  foundry_file_attribute_set_value (self, bytes);
}

void
foundry_file_attribute_set_value_boolean (FoundryFileAttribute *self,
                                          gboolean              value)
{
  g_autoptr(GBytes) bytes = NULL;
  static const guint8 one = 1;

  g_return_if_fail (FOUNDRY_IS_FILE_ATTRIBUTE (self));

  if (value)
    bytes = g_bytes_new (&one, 1);

  foundry_file_attribute_set_value (self, bytes);
}
