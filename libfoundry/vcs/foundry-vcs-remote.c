/* foundry-vcs-remote.c
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

#include "foundry-vcs-remote.h"

/**
 * FoundryVcsRemote:
 *
 * Abstract base class for version control system remote repository implementations.
 *
 * FoundryVcsRemote provides the core interface for interacting with remote
 * repositories in version control systems. Concrete implementations handle
 * specific VCS protocols and provide unified access to remote repository
 * operations such as fetching, pushing, and cloning.
 */

enum {
  PROP_0,
  PROP_NAME,
  PROP_URI,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryVcsRemote, foundry_vcs_remote, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_vcs_remote_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryVcsRemote *self = FOUNDRY_VCS_REMOTE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_take_string (value, foundry_vcs_remote_dup_name (self));
      break;

    case PROP_URI:
      g_value_take_string (value, foundry_vcs_remote_dup_uri (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_remote_class_init (FoundryVcsRemoteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_vcs_remote_get_property;

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
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
foundry_vcs_remote_init (FoundryVcsRemote *self)
{
}

/**
 * foundry_vcs_remote_dup_name:
 * @self: a [class@Foundry.VcsRemote]
 *
 * Returns: (transfer full) (nullable):
 */
char *
foundry_vcs_remote_dup_name (FoundryVcsRemote *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_REMOTE (self), NULL);

  if (FOUNDRY_VCS_REMOTE_GET_CLASS (self)->dup_name)
    return FOUNDRY_VCS_REMOTE_GET_CLASS (self)->dup_name (self);

  return NULL;
}

/**
 * foundry_vcs_remote_dup_uri:
 * @self: a [class@Foundry.VcsRemote]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_vcs_remote_dup_uri (FoundryVcsRemote *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_REMOTE (self), NULL);

  if (FOUNDRY_VCS_REMOTE_GET_CLASS (self)->dup_uri)
    return FOUNDRY_VCS_REMOTE_GET_CLASS (self)->dup_uri (self);

  return NULL;
}
