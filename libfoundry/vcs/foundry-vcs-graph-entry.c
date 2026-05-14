/* foundry-vcs-graph-entry.c
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

#include "config.h"

#include "foundry-vcs-graph-entry.h"

enum {
  PROP_0,
  PROP_COMMIT_ID,
  PROP_COMMIT_LANE,
  PROP_N_LANES,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryVcsGraphEntry, foundry_vcs_graph_entry, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_vcs_graph_entry_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  FoundryVcsGraphEntry *self = FOUNDRY_VCS_GRAPH_ENTRY (object);

  switch (prop_id)
    {
    case PROP_COMMIT_ID:
      g_value_take_string (value, foundry_vcs_graph_entry_dup_commit_id (self));
      break;

    case PROP_COMMIT_LANE:
      g_value_set_uint (value, foundry_vcs_graph_entry_get_commit_lane (self));
      break;

    case PROP_N_LANES:
      g_value_set_uint (value, foundry_vcs_graph_entry_get_n_lanes (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_graph_entry_class_init (FoundryVcsGraphEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_vcs_graph_entry_get_property;

  properties[PROP_COMMIT_ID] =
    g_param_spec_string ("commit-id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_COMMIT_LANE] =
    g_param_spec_uint ("commit-lane", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_N_LANES] =
    g_param_spec_uint ("n-lanes", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_vcs_graph_entry_init (FoundryVcsGraphEntry *self)
{
}

/**
 * foundry_vcs_graph_entry_dup_commit_id:
 * @self: a [class@Foundry.VcsGraphEntry]
 *
 * Gets the commit identifier represented by this graph row.
 *
 * Returns: (transfer full): the commit identifier
 *
 * Since: 1.2
 */
char *
foundry_vcs_graph_entry_dup_commit_id (FoundryVcsGraphEntry *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_GRAPH_ENTRY (self), NULL);

  if (FOUNDRY_VCS_GRAPH_ENTRY_GET_CLASS (self)->dup_commit_id)
    return FOUNDRY_VCS_GRAPH_ENTRY_GET_CLASS (self)->dup_commit_id (self);

  return NULL;
}

/**
 * foundry_vcs_graph_entry_get_commit_lane:
 * @self: a [class@Foundry.VcsGraphEntry]
 *
 * Gets the lane containing the commit node.
 *
 * Returns: the commit lane
 *
 * Since: 1.2
 */
guint
foundry_vcs_graph_entry_get_commit_lane (FoundryVcsGraphEntry *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_GRAPH_ENTRY (self), 0);

  if (FOUNDRY_VCS_GRAPH_ENTRY_GET_CLASS (self)->get_commit_lane)
    return FOUNDRY_VCS_GRAPH_ENTRY_GET_CLASS (self)->get_commit_lane (self);

  return 0;
}

/**
 * foundry_vcs_graph_entry_get_n_lanes:
 * @self: a [class@Foundry.VcsGraphEntry]
 *
 * Gets the number of lanes required by this graph row.
 *
 * Returns: the number of lanes
 *
 * Since: 1.2
 */
guint
foundry_vcs_graph_entry_get_n_lanes (FoundryVcsGraphEntry *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_GRAPH_ENTRY (self), 0);

  if (FOUNDRY_VCS_GRAPH_ENTRY_GET_CLASS (self)->get_n_lanes)
    return FOUNDRY_VCS_GRAPH_ENTRY_GET_CLASS (self)->get_n_lanes (self);

  return 0;
}

/**
 * foundry_vcs_graph_entry_get_n_segments:
 * @self: a [class@Foundry.VcsGraphEntry]
 *
 * Gets the number of graph segments in this row.
 *
 * Returns: the number of segments
 *
 * Since: 1.2
 */
guint
foundry_vcs_graph_entry_get_n_segments (FoundryVcsGraphEntry *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_GRAPH_ENTRY (self), 0);

  if (FOUNDRY_VCS_GRAPH_ENTRY_GET_CLASS (self)->get_n_segments)
    return FOUNDRY_VCS_GRAPH_ENTRY_GET_CLASS (self)->get_n_segments (self);

  return 0;
}

/**
 * foundry_vcs_graph_entry_get_segment:
 * @self: a [class@Foundry.VcsGraphEntry]
 * @position: the segment position
 * @segment: (out): location for a [struct@Foundry.VcsGraphSegment]
 *
 * Gets a graph segment for this row.
 *
 * Returns: %TRUE if @segment was set, otherwise %FALSE
 *
 * Since: 1.2
 */
gboolean
foundry_vcs_graph_entry_get_segment (FoundryVcsGraphEntry   *self,
                                     guint                   position,
                                     FoundryVcsGraphSegment *segment)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_GRAPH_ENTRY (self), FALSE);
  g_return_val_if_fail (segment != NULL, FALSE);

  if (FOUNDRY_VCS_GRAPH_ENTRY_GET_CLASS (self)->get_segment)
    return FOUNDRY_VCS_GRAPH_ENTRY_GET_CLASS (self)->get_segment (self, position, segment);

  return FALSE;
}

G_DEFINE_ENUM_TYPE (FoundryVcsGraphPoint, foundry_vcs_graph_point,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_VCS_GRAPH_POINT_TOP, "top"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_VCS_GRAPH_POINT_CENTER, "center"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_VCS_GRAPH_POINT_BOTTOM, "bottom"))
