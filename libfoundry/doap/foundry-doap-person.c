/* foundry-doap-person.c
 *
 * Copyright 2015-2025 Christian Hergert <chergert@redhat.com>
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

#include <glib/gi18n-lib.h>

#include "foundry-doap-person.h"

struct _FoundryDoapPerson
{
  GObject parent_instance;

  char *email;
  char *name;
};

G_DEFINE_FINAL_TYPE (FoundryDoapPerson, foundry_doap_person, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_EMAIL,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

FoundryDoapPerson *
foundry_doap_person_new (void)
{
  return g_object_new (FOUNDRY_TYPE_DOAP_PERSON, NULL);
}

const char *
foundry_doap_person_get_name (FoundryDoapPerson *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DOAP_PERSON (self), NULL);

  return self->name;
}

void
foundry_doap_person_set_name (FoundryDoapPerson *self,
                             const char       *name)
{
  g_return_if_fail (FOUNDRY_IS_DOAP_PERSON (self));

  if (g_set_str (&self->name, name))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
}

const char *
foundry_doap_person_get_email (FoundryDoapPerson *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DOAP_PERSON (self), NULL);

  return self->email;
}

void
foundry_doap_person_set_email (FoundryDoapPerson *self,
                              const char      *email)
{
  g_return_if_fail (FOUNDRY_IS_DOAP_PERSON (self));

  if (g_set_str (&self->email, email))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EMAIL]);
}

static void
foundry_doap_person_finalize (GObject *object)
{
  FoundryDoapPerson *self = (FoundryDoapPerson *)object;

  g_clear_pointer (&self->email, g_free);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (foundry_doap_person_parent_class)->finalize (object);
}

static void
foundry_doap_person_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryDoapPerson *self = FOUNDRY_DOAP_PERSON (object);

  switch (prop_id)
    {
    case PROP_EMAIL:
      g_value_set_string (value, foundry_doap_person_get_email (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, foundry_doap_person_get_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_doap_person_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  FoundryDoapPerson *self = FOUNDRY_DOAP_PERSON (object);

  switch (prop_id)
    {
    case PROP_EMAIL:
      foundry_doap_person_set_email (self, g_value_get_string (value));
      break;

    case PROP_NAME:
      foundry_doap_person_set_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_doap_person_class_init (FoundryDoapPersonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_doap_person_finalize;
  object_class->get_property = foundry_doap_person_get_property;
  object_class->set_property = foundry_doap_person_set_property;

  properties[PROP_EMAIL] =
    g_param_spec_string ("email", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_doap_person_init (FoundryDoapPerson *self)
{
}
