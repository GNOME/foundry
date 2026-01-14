/* foundry-team-artifact-file.c
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

#include "foundry-team-artifact-file.h"
#include "foundry-util-private.h"

/**
 * FoundryTeamArtifactFile:
 *
 * An artifact representing a file with filename and contents.
 *
 * FoundryTeamArtifactFile is used to pass file data between personas
 * in a team workflow.
 */

struct _FoundryTeamArtifactFile
{
  FoundryTeamArtifact parent_instance;
  char               *filename;
  char               *contents;
};

enum {
  PROP_0,
  PROP_CONTENTS,
  PROP_FILENAME,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static char *
foundry_team_artifact_file_dup_content_type (FoundryTeamArtifact *artifact)
{
  return g_strdup ("file");
}

static char *
foundry_team_artifact_file_dup_label (FoundryTeamArtifact *artifact)
{
  FoundryTeamArtifactFile *self = FOUNDRY_TEAM_ARTIFACT_FILE (artifact);
  return g_strdup (self->filename);
}

G_DEFINE_FINAL_TYPE (FoundryTeamArtifactFile, foundry_team_artifact_file, FOUNDRY_TYPE_TEAM_ARTIFACT)

static void
foundry_team_artifact_file_finalize (GObject *object)
{
  FoundryTeamArtifactFile *self = FOUNDRY_TEAM_ARTIFACT_FILE (object);

  g_clear_pointer (&self->filename, g_free);
  g_clear_pointer (&self->contents, g_free);

  G_OBJECT_CLASS (foundry_team_artifact_file_parent_class)->finalize (object);
}

static void
foundry_team_artifact_file_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  FoundryTeamArtifactFile *self = FOUNDRY_TEAM_ARTIFACT_FILE (object);

  switch (prop_id)
    {
    case PROP_CONTENTS:
      g_value_take_string (value, foundry_team_artifact_file_dup_contents (self));
      break;

    case PROP_FILENAME:
      g_value_take_string (value, foundry_team_artifact_file_dup_filename (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_team_artifact_file_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  FoundryTeamArtifactFile *self = FOUNDRY_TEAM_ARTIFACT_FILE (object);

  switch (prop_id)
    {
    case PROP_CONTENTS:
      g_set_str (&self->contents, g_value_get_string (value));
      break;

    case PROP_FILENAME:
      g_set_str (&self->filename, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_team_artifact_file_class_init (FoundryTeamArtifactFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryTeamArtifactClass *artifact_class = FOUNDRY_TEAM_ARTIFACT_CLASS (klass);

  object_class->finalize = foundry_team_artifact_file_finalize;
  object_class->get_property = foundry_team_artifact_file_get_property;
  object_class->set_property = foundry_team_artifact_file_set_property;

  artifact_class->dup_content_type = foundry_team_artifact_file_dup_content_type;
  artifact_class->dup_label = foundry_team_artifact_file_dup_label;

  properties[PROP_CONTENTS] =
    g_param_spec_string ("contents", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_FILENAME] =
    g_param_spec_string ("filename", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_team_artifact_file_init (FoundryTeamArtifactFile *self)
{
}

FoundryTeamArtifactFile *
foundry_team_artifact_file_new (const char *filename,
                                 const char *contents)
{
  return g_object_new (FOUNDRY_TYPE_TEAM_ARTIFACT_FILE,
                       "filename", filename,
                       "contents", contents,
                       NULL);
}

char *
foundry_team_artifact_file_dup_filename (FoundryTeamArtifactFile *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEAM_ARTIFACT_FILE (self), NULL);

  return g_strdup (self->filename);
}

char *
foundry_team_artifact_file_dup_contents (FoundryTeamArtifactFile *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEAM_ARTIFACT_FILE (self), NULL);

  return g_strdup (self->contents);
}
