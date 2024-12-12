/* foundry-vcs.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "foundry-vcs.h"
#include "foundry-vcs-manager.h"

G_DEFINE_ABSTRACT_TYPE (FoundryVcs, foundry_vcs, FOUNDRY_TYPE_CONTEXTUAL)

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_ID,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_vcs_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  FoundryVcs *self = FOUNDRY_VCS (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, foundry_vcs_get_active (self));
      break;

    case PROP_ID:
      g_value_take_string (value, foundry_vcs_dup_id (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_class_init (FoundryVcsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_vcs_get_property;

  properties[PROP_ACTIVE] =
    g_param_spec_boolean ("active", NULL, NULL,
                         FALSE,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_vcs_init (FoundryVcs *self)
{
}

gboolean
foundry_vcs_get_active (FoundryVcs *self)
{
  g_autoptr(FoundryContext) context = NULL;

  g_return_val_if_fail (FOUNDRY_IS_VCS (self), FALSE);

  if ((context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))))
    {
      g_autoptr(FoundryVcsManager) vcs_manager = foundry_context_dup_vcs_manager (context);
      g_autoptr(FoundryVcs) vcs = foundry_vcs_manager_dup_vcs (vcs_manager);

      return vcs == self;
    }

  return FALSE;
}

/**
 * foundry_vcs_dup_id:
 * @self: a #FoundryVcs
 *
 * Gets the identifier for the VCS such as "git" or "none".
 *
 * Returns: (transfer full): a string containing the identifier
 */
char *
foundry_vcs_dup_id (FoundryVcs *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS (self), NULL);

  return FOUNDRY_VCS_GET_CLASS (self)->dup_id (self);
}
