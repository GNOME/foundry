/* foundry-documentation.c
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

#include "foundry-documentation.h"

G_DEFINE_ABSTRACT_TYPE (FoundryDocumentation, foundry_documentation, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_TITLE,
  PROP_URI,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_documentation_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundryDocumentation *self = FOUNDRY_DOCUMENTATION (object);

  switch (prop_id)
    {
    case PROP_URI:
      g_value_take_string (value, foundry_documentation_dup_uri (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, foundry_documentation_dup_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_documentation_class_init (FoundryDocumentationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_documentation_get_property;

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_documentation_init (FoundryDocumentation *self)
{
}

/**
 * foundry_documentation_dup_uri:
 * @self: a [class@Foundry.Documentation]
 *
 * Returns: (transfer full) (nullable):
 */
char *
foundry_documentation_dup_uri (FoundryDocumentation *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DOCUMENTATION (self), NULL);

  if (FOUNDRY_DOCUMENTATION_GET_CLASS (self)->dup_uri)
    return FOUNDRY_DOCUMENTATION_GET_CLASS (self)->dup_uri (self);

  return NULL;
}

/**
 * foundry_documentation_dup_title:
 * @self: a [class@Foundry.Documentation]
 *
 * Returns: (transfer full) (nullable):
 */
char *
foundry_documentation_dup_title (FoundryDocumentation *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DOCUMENTATION (self), NULL);

  if (FOUNDRY_DOCUMENTATION_GET_CLASS (self)->dup_title)
    return FOUNDRY_DOCUMENTATION_GET_CLASS (self)->dup_title (self);

  return NULL;
}

/**
 * foundry_documentation_find_parent:
 * @self: a [class@Foundry.Documentation]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.Documentation] or rejects with error.
 */
DexFuture *
foundry_documentation_find_parent (FoundryDocumentation *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_DOCUMENTATION (self));

  if (FOUNDRY_DOCUMENTATION_GET_CLASS (self)->find_parent)
    return FOUNDRY_DOCUMENTATION_GET_CLASS (self)->find_parent (self);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_FOUND,
                                "Not found");
}

/**
 * foundry_documentation_dup_icon:
 * @self: a [class@Foundry.Documentation]
 *
 * Returns: (transfer full) (nullable):
 */
GIcon *
foundry_documentation_dup_icon (FoundryDocumentation *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DOCUMENTATION (self), NULL);

  if (FOUNDRY_DOCUMENTATION_GET_CLASS (self)->dup_icon)
    return FOUNDRY_DOCUMENTATION_GET_CLASS (self)->dup_icon (self);

  return NULL;
}
