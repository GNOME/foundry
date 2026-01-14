/* foundry-team-artifact-diff.c
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

#include "foundry-team-artifact-diff.h"
#include "foundry-util-private.h"

/**
 * FoundryTeamArtifactDiff:
 *
 * An artifact representing a unified diff.
 *
 * FoundryTeamArtifactDiff is used to pass diff data between personas
 * in a team workflow.
 */

struct _FoundryTeamArtifactDiff
{
  FoundryTeamArtifact  parent_instance;
  char                *diff;
};

enum {
  PROP_0,
  PROP_DIFF,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static char *
foundry_team_artifact_diff_dup_content_type (FoundryTeamArtifact *artifact)
{
  return g_strdup ("diff");
}

static char *
foundry_team_artifact_diff_dup_label (FoundryTeamArtifact *artifact)
{
  return g_strdup ("Unified Diff");
}

G_DEFINE_FINAL_TYPE (FoundryTeamArtifactDiff, foundry_team_artifact_diff, FOUNDRY_TYPE_TEAM_ARTIFACT)

static void
foundry_team_artifact_diff_finalize (GObject *object)
{
  FoundryTeamArtifactDiff *self = FOUNDRY_TEAM_ARTIFACT_DIFF (object);

  g_clear_pointer (&self->diff, g_free);

  G_OBJECT_CLASS (foundry_team_artifact_diff_parent_class)->finalize (object);
}

static void
foundry_team_artifact_diff_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  FoundryTeamArtifactDiff *self = FOUNDRY_TEAM_ARTIFACT_DIFF (object);

  switch (prop_id)
    {
    case PROP_DIFF:
      g_value_take_string (value, foundry_team_artifact_diff_dup_diff (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_team_artifact_diff_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  FoundryTeamArtifactDiff *self = FOUNDRY_TEAM_ARTIFACT_DIFF (object);

  switch (prop_id)
    {
    case PROP_DIFF:
      g_set_str (&self->diff, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_team_artifact_diff_class_init (FoundryTeamArtifactDiffClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryTeamArtifactClass *artifact_class = FOUNDRY_TEAM_ARTIFACT_CLASS (klass);

  object_class->finalize = foundry_team_artifact_diff_finalize;
  object_class->get_property = foundry_team_artifact_diff_get_property;
  object_class->set_property = foundry_team_artifact_diff_set_property;

  artifact_class->dup_content_type = foundry_team_artifact_diff_dup_content_type;
  artifact_class->dup_label = foundry_team_artifact_diff_dup_label;

  properties[PROP_DIFF] =
    g_param_spec_string ("diff", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_team_artifact_diff_init (FoundryTeamArtifactDiff *self)
{
}

FoundryTeamArtifactDiff *
foundry_team_artifact_diff_new (const char *diff)
{
  return g_object_new (FOUNDRY_TYPE_TEAM_ARTIFACT_DIFF,
                       "diff", diff,
                       NULL);
}

char *
foundry_team_artifact_diff_dup_diff (FoundryTeamArtifactDiff *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEAM_ARTIFACT_DIFF (self), NULL);

  return g_strdup (self->diff);
}
