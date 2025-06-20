/* foundry-vcs-object.c
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

#include "foundry-vcs-object.h"

enum {
  PROP_0,
  PROP_ID,
  PROP_NAME,
  PROP_IS_LOCAL,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryVcsObject, foundry_vcs_object, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_vcs_object_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryVcsObject *self = FOUNDRY_VCS_OBJECT (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_take_string (value, foundry_vcs_object_dup_id (self));
      break;

    case PROP_NAME:
      g_value_take_string (value, foundry_vcs_object_dup_name (self));
      break;

    case PROP_IS_LOCAL:
      g_value_set_boolean (value, foundry_vcs_object_is_local (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_object_class_init (FoundryVcsObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_vcs_object_get_property;

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_IS_LOCAL] =
    g_param_spec_boolean ("is-local", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_vcs_object_init (FoundryVcsObject *self)
{
}

/**
 * foundry_vcs_object_dup_id:
 * @self: a [class@Foundry.VcsObject]
 *
 * Returns: (transfer full) (nullable):
 */
char *
foundry_vcs_object_dup_id (FoundryVcsObject *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_OBJECT (self), NULL);

  if (FOUNDRY_VCS_OBJECT_GET_CLASS (self)->dup_id)
    return FOUNDRY_VCS_OBJECT_GET_CLASS (self)->dup_id (self);

  return NULL;
}

/**
 * foundry_vcs_object_dup_name:
 * @self: a [class@Foundry.VcsObject]
 *
 * Returns: (transfer full) (nullable):
 */
char *
foundry_vcs_object_dup_name (FoundryVcsObject *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_OBJECT (self), NULL);

  if (FOUNDRY_VCS_OBJECT_GET_CLASS (self)->dup_name)
    return FOUNDRY_VCS_OBJECT_GET_CLASS (self)->dup_name (self);

  return NULL;
}

/**
 * foundry_vcs_object_is_local:
 * @self: a [class@Foundry.VcsObject]
 *
 * Useful to denote things that have been created locally but are
 * not yet sync'd to the remote or will be sync'd to the remote under
 * a different id/name.
 *
 * Returns: %TRUE if the object is from the local copy of the
 *   version control system as opposed to only existing on the
 *   remote.
 */
gboolean
foundry_vcs_object_is_local (FoundryVcsObject *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_OBJECT (self), FALSE);

  if (FOUNDRY_VCS_OBJECT_GET_CLASS (self)->is_local)
    return FOUNDRY_VCS_OBJECT_GET_CLASS (self)->is_local (self);

  return FALSE;
}
