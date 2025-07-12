/* foundry-vcs-branch.c
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

#include "foundry-vcs-branch.h"

G_DEFINE_ABSTRACT_TYPE (FoundryVcsBranch, foundry_vcs_branch, FOUNDRY_TYPE_VCS_REFERENCE)

enum {
  PROP_0,
  PROP_IS_LOCAL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_vcs_branch_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryVcsBranch *self = FOUNDRY_VCS_BRANCH (object);

  switch (prop_id)
    {
    case PROP_IS_LOCAL:
      g_value_set_boolean (value, foundry_vcs_branch_is_local (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_branch_class_init (FoundryVcsBranchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_vcs_branch_get_property;

  properties[PROP_IS_LOCAL] =
    g_param_spec_boolean ("is-local", NULL, NULL,
                          TRUE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_vcs_branch_init (FoundryVcsBranch *self)
{
}

gboolean
foundry_vcs_branch_is_local (FoundryVcsBranch *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_BRANCH (self), FALSE);

  if (FOUNDRY_VCS_BRANCH_GET_CLASS (self)->is_local)
    return FOUNDRY_VCS_BRANCH_GET_CLASS (self)->is_local (self);

  return TRUE;
}
