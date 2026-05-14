/* foundry-vcs-graph.c
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
#include "foundry-vcs-graph.h"

static GType
foundry_vcs_graph_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_VCS_GRAPH_ENTRY;
}

static guint
foundry_vcs_graph_get_n_items (GListModel *model)
{
  return FOUNDRY_VCS_GRAPH_GET_CLASS (model)->get_n_items (FOUNDRY_VCS_GRAPH (model));
}

static gpointer
foundry_vcs_graph_get_item (GListModel *model,
                            guint       position)
{
  FoundryVcsGraph *self = FOUNDRY_VCS_GRAPH (model);

  return foundry_vcs_graph_dup_entry (self, position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_vcs_graph_get_item_type;
  iface->get_n_items = foundry_vcs_graph_get_n_items;
  iface->get_item = foundry_vcs_graph_get_item;
}

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (FoundryVcsGraph, foundry_vcs_graph, G_TYPE_OBJECT,
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
foundry_vcs_graph_class_init (FoundryVcsGraphClass *klass)
{
}

static void
foundry_vcs_graph_init (FoundryVcsGraph *self)
{
}

/**
 * foundry_vcs_graph_get_n_lanes:
 * @self: a [class@Foundry.VcsGraph]
 *
 * Gets the maximum number of lanes used by the graph.
 *
 * Returns: the number of lanes
 *
 * Since: 1.2
 */
guint
foundry_vcs_graph_get_n_lanes (FoundryVcsGraph *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_GRAPH (self), 0);

  if (FOUNDRY_VCS_GRAPH_GET_CLASS (self)->get_n_lanes)
    return FOUNDRY_VCS_GRAPH_GET_CLASS (self)->get_n_lanes (self);

  return 0;
}

/**
 * foundry_vcs_graph_dup_entry:
 * @self: a [class@Foundry.VcsGraph]
 * @position: the entry position
 *
 * Gets the graph entry at @position.
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.VcsGraphEntry]
 *
 * Since: 1.2
 */
FoundryVcsGraphEntry *
foundry_vcs_graph_dup_entry (FoundryVcsGraph *self,
                             guint            position)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_GRAPH (self), NULL);

  if (FOUNDRY_VCS_GRAPH_GET_CLASS (self)->dup_entry)
    return FOUNDRY_VCS_GRAPH_GET_CLASS (self)->dup_entry (self, position);

  return NULL;
}
