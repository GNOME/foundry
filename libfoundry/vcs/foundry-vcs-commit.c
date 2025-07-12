/* foundry-vcs-commit.c
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

#include "foundry-vcs-commit.h"

enum {
  PROP_0,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryVcsCommit, foundry_vcs_commit, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_vcs_commit_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryVcsCommit *self = FOUNDRY_VCS_COMMIT (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_take_string (value, foundry_vcs_commit_dup_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_commit_class_init (FoundryVcsCommitClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_vcs_commit_get_property;

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_vcs_commit_init (FoundryVcsCommit *self)
{
}

char *
foundry_vcs_commit_dup_title (FoundryVcsCommit *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_COMMIT (self), NULL);

  if (FOUNDRY_VCS_COMMIT_GET_CLASS (self)->dup_title)
    return FOUNDRY_VCS_COMMIT_GET_CLASS (self)->dup_title (self);

  return NULL;
}
