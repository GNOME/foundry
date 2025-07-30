/* foundry-input-file.c
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

#include "foundry-input-file.h"
#include "foundry-input-validator.h"
#include "foundry-util-private.h"

struct _FoundryInputFile
{
  FoundryInput parent_instance;
  GMutex mutex;
  GFile *value;
};

enum {
  PROP_0,
  PROP_VALUE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryInputFile, foundry_input_file, FOUNDRY_TYPE_INPUT)

static GParamSpec *properties[N_PROPS];

static void
foundry_input_file_finalize (GObject *object)
{
  FoundryInputFile *self = (FoundryInputFile *)object;

  g_clear_object (&self->value);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (foundry_input_file_parent_class)->finalize (object);
}

static void
foundry_input_file_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryInputFile *self = FOUNDRY_INPUT_FILE (object);

  switch (prop_id)
    {
    case PROP_VALUE:
      g_value_take_object (value, foundry_input_file_dup_value (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_input_file_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  FoundryInputFile *self = FOUNDRY_INPUT_FILE (object);

  switch (prop_id)
    {
    case PROP_VALUE:
      foundry_input_file_set_value (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_input_file_class_init (FoundryInputFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_input_file_finalize;
  object_class->get_property = foundry_input_file_get_property;
  object_class->set_property = foundry_input_file_set_property;

  properties[PROP_VALUE] =
    g_param_spec_object ("value", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_input_file_init (FoundryInputFile *self)
{
  g_mutex_init (&self->mutex);
}

/**
 * foundry_input_file_new:
 * @validator: (transfer full) (nullable): optional validator
 * @value: (nullable): a [iface@Gio.File]
 *
 * Returns: (transfer full):
 */
FoundryInput *
foundry_input_file_new (const char            *title,
                        const char            *subtitle,
                        FoundryInputValidator *validator,
                        GFile                 *value)
{
  g_autoptr(FoundryInputValidator) stolen = NULL;

  g_return_val_if_fail (!value || G_IS_FILE (value), NULL);
  g_return_val_if_fail (!validator || FOUNDRY_IS_INPUT_VALIDATOR (validator), NULL);

  stolen = validator;

  return g_object_new (FOUNDRY_TYPE_INPUT_FILE,
                       "title", title,
                       "subtitle", subtitle,
                       "validator", validator,
                       "value", value,
                       NULL);
}

/**
 * foundry_input_file_dup_value:
 * @self: a [class@Foundry.InputFile]
 *
 * Returns: (transfer full) (nullable):
 */
GFile *
foundry_input_file_dup_value (FoundryInputFile *self)
{
  GFile *value = NULL;

  g_return_val_if_fail (FOUNDRY_IS_INPUT_FILE (self), NULL);

  g_mutex_lock (&self->mutex);
  g_set_object (&value, self->value);
  g_mutex_unlock (&self->mutex);

  return g_steal_pointer (&value);
}

void
foundry_input_file_set_value (FoundryInputFile *self,
                              GFile            *value)
{
  g_autoptr(GFile) old = NULL;

  g_return_if_fail (FOUNDRY_IS_INPUT_FILE (self));
  g_return_if_fail (!value || G_IS_FILE (value));

  g_mutex_lock (&self->mutex);
  if (self->value != value)
    {
      old = g_steal_pointer (&self->value);
      self->value = value ? g_object_ref (value) : NULL;
    }
  g_mutex_unlock (&self->mutex);

  if (old != NULL)
    foundry_notify_pspec_in_main (G_OBJECT (self), properties[PROP_VALUE]);
}
