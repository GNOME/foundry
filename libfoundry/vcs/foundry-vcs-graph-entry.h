/* foundry-vcs-graph-entry.h
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#include <glib-object.h>

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_VCS_GRAPH_ENTRY (foundry_vcs_graph_entry_get_type())
#define FOUNDRY_TYPE_VCS_GRAPH_POINT (foundry_vcs_graph_point_get_type())

typedef struct _FoundryVcsGraphSegment
{
  guint                 from_lane;
  guint                 to_lane;
  FoundryVcsGraphPoint  from_point;
  FoundryVcsGraphPoint  to_point;
  guint                 color_id;
} FoundryVcsGraphSegment;

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_DERIVABLE_TYPE (FoundryVcsGraphEntry, foundry_vcs_graph_entry, FOUNDRY, VCS_GRAPH_ENTRY, GObject)

struct _FoundryVcsGraphEntryClass
{
  GObjectClass parent_class;

  char     *(*dup_commit_id)   (FoundryVcsGraphEntry   *self);
  guint     (*get_commit_lane) (FoundryVcsGraphEntry   *self);
  guint     (*get_n_lanes)     (FoundryVcsGraphEntry   *self);
  guint     (*get_n_segments)  (FoundryVcsGraphEntry   *self);
  gboolean  (*get_segment)     (FoundryVcsGraphEntry   *self,
                                guint                   position,
                                FoundryVcsGraphSegment *segment);

  /*< private >*/
  gpointer _reserved[10];
};

FOUNDRY_AVAILABLE_IN_1_2
GType             foundry_vcs_graph_point_get_type        (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_1_2
char             *foundry_vcs_graph_entry_dup_commit_id   (FoundryVcsGraphEntry   *self);
FOUNDRY_AVAILABLE_IN_1_2
guint             foundry_vcs_graph_entry_get_commit_lane (FoundryVcsGraphEntry   *self);
FOUNDRY_AVAILABLE_IN_1_2
guint             foundry_vcs_graph_entry_get_n_lanes     (FoundryVcsGraphEntry   *self);
FOUNDRY_AVAILABLE_IN_1_2
guint             foundry_vcs_graph_entry_get_n_segments  (FoundryVcsGraphEntry   *self);
FOUNDRY_AVAILABLE_IN_1_2
gboolean          foundry_vcs_graph_entry_get_segment     (FoundryVcsGraphEntry   *self,
                                                           guint                   position,
                                                           FoundryVcsGraphSegment *segment);

G_END_DECLS
