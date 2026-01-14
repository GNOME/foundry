/* foundry-team-artifact.h
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

#pragma once

#include <gio/gio.h>

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_TEAM_ARTIFACT (foundry_team_artifact_get_type())

FOUNDRY_AVAILABLE_IN_1_1
GType foundry_team_artifact_get_type (void);

typedef struct _FoundryTeamArtifact FoundryTeamArtifact;
typedef struct _FoundryTeamArtifactClass FoundryTeamArtifactClass;

struct _FoundryTeamArtifact
{
  GObject parent_instance;
};

struct _FoundryTeamArtifactClass
{
  GObjectClass parent_class;

  char *(*dup_content_type) (FoundryTeamArtifact *self);
  char *(*dup_label)        (FoundryTeamArtifact *self);

  /*< private >*/
  gpointer _reserved[13];
};

_GLIB_DEFINE_AUTOPTR_CHAINUP (FoundryTeamArtifact, GObject)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (FoundryTeamArtifactClass, g_type_class_unref)

#define FOUNDRY_TEAM_ARTIFACT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), FOUNDRY_TYPE_TEAM_ARTIFACT, FoundryTeamArtifact))
#define FOUNDRY_TEAM_ARTIFACT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), FOUNDRY_TYPE_TEAM_ARTIFACT, FoundryTeamArtifactClass))
#define FOUNDRY_IS_TEAM_ARTIFACT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FOUNDRY_TYPE_TEAM_ARTIFACT))
#define FOUNDRY_IS_TEAM_ARTIFACT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FOUNDRY_TYPE_TEAM_ARTIFACT))
#define FOUNDRY_TEAM_ARTIFACT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), FOUNDRY_TYPE_TEAM_ARTIFACT, FoundryTeamArtifactClass))

FOUNDRY_AVAILABLE_IN_1_1
char *foundry_team_artifact_dup_content_type (FoundryTeamArtifact *self);
FOUNDRY_AVAILABLE_IN_1_1
char *foundry_team_artifact_dup_label        (FoundryTeamArtifact *self);

G_END_DECLS
