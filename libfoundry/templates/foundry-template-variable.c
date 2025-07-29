/* foundry-template-variable.c
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

#include "foundry-template-variable.h"

G_DEFINE_ABSTRACT_TYPE (FoundryTemplateVariable, foundry_template_variable, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SUBTITLE,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_template_variable_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  FoundryTemplateVariable *self = FOUNDRY_TEMPLATE_VARIABLE (object);

  switch (prop_id)
    {
    case PROP_SUBTITLE:
      g_value_take_string (value, foundry_template_variable_dup_subtitle (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, foundry_template_variable_dup_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_template_variable_class_init (FoundryTemplateVariableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_template_variable_get_property;

  properties[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_template_variable_init (FoundryTemplateVariable *self)
{
}

/**
 * foundry_template_variable_validate:
 * @self: a [class@Foundry.TemplateVariable]
 *
 * Checks if the variable contains valid input.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE or
 *   rejects with error.
 */
DexFuture *
foundry_template_variable_validate (FoundryTemplateVariable *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_TEMPLATE_VARIABLE (self));

  if (FOUNDRY_TEMPLATE_VARIABLE_GET_CLASS (self)->validate)
    return FOUNDRY_TEMPLATE_VARIABLE_GET_CLASS (self)->validate (self);

  return dex_future_new_true ();
}

/**
 * foundry_template_variable_dup_subtitle:
 * @self: a [class@Foundry.TemplateVariable]
 *
 * Returns: (nullable):
 */
char *
foundry_template_variable_dup_subtitle (FoundryTemplateVariable *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEMPLATE_VARIABLE (self), NULL);

  if (FOUNDRY_TEMPLATE_VARIABLE_GET_CLASS (self)->dup_subtitle)
    return FOUNDRY_TEMPLATE_VARIABLE_GET_CLASS (self)->dup_subtitle (self);

  return NULL;
}

/**
 * foundry_template_variable_dup_title:
 * @self: a [class@Foundry.TemplateVariable]
 *
 * Returns: (nullable):
 */
char *
foundry_template_variable_dup_title (FoundryTemplateVariable *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEMPLATE_VARIABLE (self), NULL);

  if (FOUNDRY_TEMPLATE_VARIABLE_GET_CLASS (self)->dup_title)
    return FOUNDRY_TEMPLATE_VARIABLE_GET_CLASS (self)->dup_title (self);

  return NULL;
}
