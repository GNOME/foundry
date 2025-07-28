/* foundry-project-template.c
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

#include "foundry-project-template.h"

G_DEFINE_ABSTRACT_TYPE (FoundryProjectTemplate, foundry_project_template, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ID,
  PROP_DESCRIPTION,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_project_template_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  FoundryProjectTemplate *self = FOUNDRY_PROJECT_TEMPLATE (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_take_string (value, foundry_project_template_dup_id (self));
      break;

    case PROP_DESCRIPTION:
      g_value_take_string (value, foundry_project_template_dup_description (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_project_template_class_init (FoundryProjectTemplateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_project_template_get_property;

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_DESCRIPTION] =
    g_param_spec_string ("description", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_project_template_init (FoundryProjectTemplate *self)
{
}

char *
foundry_project_template_dup_id (FoundryProjectTemplate *self)
{
  g_return_val_if_fail (FOUNDRY_IS_PROJECT_TEMPLATE (self), NULL);

  if (FOUNDRY_PROJECT_TEMPLATE_GET_CLASS (self)->dup_id)
    return FOUNDRY_PROJECT_TEMPLATE_GET_CLASS (self)->dup_id (self);

  return NULL;
}

char *
foundry_project_template_dup_description (FoundryProjectTemplate *self)
{
  g_return_val_if_fail (FOUNDRY_IS_PROJECT_TEMPLATE (self), NULL);

  if (FOUNDRY_PROJECT_TEMPLATE_GET_CLASS (self)->dup_description)
    return FOUNDRY_PROJECT_TEMPLATE_GET_CLASS (self)->dup_description (self);

  return NULL;
}
