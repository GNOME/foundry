/* foundry-template.c
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

#include "foundry-template.h"
#include "foundry-util.h"

typedef struct
{
  gpointer dummy;
} FoundryTemplatePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryTemplate, foundry_template, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ID,
  PROP_DESCRIPTION,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_template_finalize (GObject *object)
{
  G_OBJECT_CLASS (foundry_template_parent_class)->finalize (object);
}

static void
foundry_template_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  FoundryTemplate *self = FOUNDRY_TEMPLATE (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_take_string (value, foundry_template_dup_id (self));
      break;

    case PROP_DESCRIPTION:
      g_value_take_string (value, foundry_template_dup_description (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_template_class_init (FoundryTemplateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_template_finalize;
  object_class->get_property = foundry_template_get_property;

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
foundry_template_init (FoundryTemplate *self)
{
}

char *
foundry_template_dup_id (FoundryTemplate *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEMPLATE (self), NULL);

  if (FOUNDRY_TEMPLATE_GET_CLASS (self)->dup_id)
    return FOUNDRY_TEMPLATE_GET_CLASS (self)->dup_id (self);

  return NULL;
}

char *
foundry_template_dup_description (FoundryTemplate *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEMPLATE (self), NULL);

  if (FOUNDRY_TEMPLATE_GET_CLASS (self)->dup_description)
    return FOUNDRY_TEMPLATE_GET_CLASS (self)->dup_description (self);

  return NULL;
}

/**
 * foundry_template_expand:
 * @self: a [class@Foundry.Template]
 *
 * Expands the template based on the input parameters provided
 * to the template.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any
 *   value or rejects with error.
 */
DexFuture *
foundry_template_expand (FoundryTemplate *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_TEMPLATE (self));

  if (FOUNDRY_TEMPLATE_GET_CLASS (self)->expand)
    return FOUNDRY_TEMPLATE_GET_CLASS (self)->expand (self);

  return foundry_future_new_not_supported ();
}
