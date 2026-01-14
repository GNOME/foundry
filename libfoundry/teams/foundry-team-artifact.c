/* foundry-team-artifact.c
 *
 * Copyright 2026 Christian Hergert
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

#include "foundry-team-artifact.h"

/**
 * FoundryTeamArtifact:
 *
 * Abstract base class for artifacts passed between personas in a team.
 *
 * FoundryTeamArtifact provides the core interface for typed data objects
 * that can be passed between personas in a FoundryTeam. Each artifact has
 * a type identifier and an optional label for display purposes.
 *
 * Concrete implementations provide specific data structures such as files,
 * diffs, or other structured information needed for agent communication.
 */

G_DEFINE_ABSTRACT_TYPE (FoundryTeamArtifact, foundry_team_artifact, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_LABEL,
  PROP_TYPE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_team_artifact_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundryTeamArtifact *self = FOUNDRY_TEAM_ARTIFACT (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_take_string (value, foundry_team_artifact_dup_label (self));
      break;

    case PROP_TYPE:
      g_value_take_string (value, foundry_team_artifact_dup_content_type (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_team_artifact_class_init (FoundryTeamArtifactClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_team_artifact_get_property;

  properties[PROP_LABEL] =
    g_param_spec_string ("label", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TYPE] =
    g_param_spec_string ("type", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_team_artifact_init (FoundryTeamArtifact *self)
{
}

/**
 * foundry_team_artifact_dup_content_type:
 * @self: a [class@Foundry.TeamArtifact]
 *
 * Returns: (transfer full) (nullable):
 */
char *
foundry_team_artifact_dup_content_type (FoundryTeamArtifact *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEAM_ARTIFACT (self), NULL);

  if (FOUNDRY_TEAM_ARTIFACT_GET_CLASS (self)->dup_content_type)
    return FOUNDRY_TEAM_ARTIFACT_GET_CLASS (self)->dup_content_type (self);

  return NULL;
}

/**
 * foundry_team_artifact_dup_label:
 * @self: a [class@Foundry.TeamArtifact]
 *
 * Returns: (transfer full) (nullable):
 */
char *
foundry_team_artifact_dup_label (FoundryTeamArtifact *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEAM_ARTIFACT (self), NULL);

  if (FOUNDRY_TEAM_ARTIFACT_GET_CLASS (self)->dup_label)
    return FOUNDRY_TEAM_ARTIFACT_GET_CLASS (self)->dup_label (self);

  return NULL;
}
