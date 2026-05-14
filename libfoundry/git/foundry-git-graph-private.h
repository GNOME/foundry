/* foundry-git-graph-private.h
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

#include <git2.h>

#include "foundry-vcs-graph.h"
#include "foundry-vcs-graph-entry.h"

G_BEGIN_DECLS

typedef struct _FoundryGitGraphBuilder FoundryGitGraphBuilder;

FoundryGitGraphBuilder *foundry_git_graph_builder_new    (void);
void                    foundry_git_graph_builder_add    (FoundryGitGraphBuilder *builder,
                                                          const git_oid          *oid,
                                                          guint                   commit_lane,
                                                          guint                   n_lanes,
                                                          const GArray           *segments);
FoundryVcsGraph        *foundry_git_graph_builder_finish (FoundryGitGraphBuilder *builder,
                                                          guint                   n_lanes);
void                    foundry_git_graph_builder_free   (FoundryGitGraphBuilder *builder);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FoundryGitGraphBuilder, foundry_git_graph_builder_free)

G_END_DECLS
