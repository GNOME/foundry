/* foundry-git-graph.c
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

#include "foundry-git-graph-private.h"
#include "foundry-git-private.h"

typedef struct _FoundryGitGraph FoundryGitGraph;
typedef struct _FoundryGitGraphClass FoundryGitGraphClass;
typedef struct _FoundryGitGraphEntry FoundryGitGraphEntry;
typedef struct _FoundryGitGraphEntryClass FoundryGitGraphEntryClass;

typedef struct
{
  git_oid oid;
  FoundryGitGraphEntry *entry;
  guint segment_offset;
  guint n_segments;
  guint commit_lane;
  guint n_lanes;
} FoundryGitGraphRow;

#define EGG_ARRAY_NAME rows
#define EGG_ARRAY_TYPE_NAME Rows
#define EGG_ARRAY_ELEMENT_TYPE FoundryGitGraphRow
#define EGG_ARRAY_BY_VALUE 1
#include "eggarrayimpl.c"

#define EGG_ARRAY_NAME segments
#define EGG_ARRAY_TYPE_NAME Segments
#define EGG_ARRAY_ELEMENT_TYPE FoundryVcsGraphSegment
#define EGG_ARRAY_BY_VALUE 1
#include "eggarrayimpl.c"

struct _FoundryGitGraph
{
  FoundryVcsGraph parent_instance;

  Rows rows;
  Segments segments;
  GQueue inflated;
  guint n_lanes;
};

struct _FoundryGitGraphClass
{
  FoundryVcsGraphClass parent_class;
};

struct _FoundryGitGraphEntry
{
  FoundryVcsGraphEntry parent_instance;

  GList link;
  FoundryGitGraph *graph;
  guint position;
};

struct _FoundryGitGraphEntryClass
{
  FoundryVcsGraphEntryClass parent_class;
};

struct _FoundryGitGraphBuilder
{
  Rows rows;
  Segments segments;
};

#define FOUNDRY_TYPE_GIT_GRAPH (foundry_git_graph_get_type())
#define FOUNDRY_GIT_GRAPH(obj) (G_TYPE_CHECK_INSTANCE_CAST (obj, FOUNDRY_TYPE_GIT_GRAPH, FoundryGitGraph))
#define FOUNDRY_IS_GIT_GRAPH(obj) (G_TYPE_CHECK_INSTANCE_TYPE (obj, FOUNDRY_TYPE_GIT_GRAPH))

#define FOUNDRY_TYPE_GIT_GRAPH_ENTRY (foundry_git_graph_entry_get_type())
#define FOUNDRY_GIT_GRAPH_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_CAST (obj, FOUNDRY_TYPE_GIT_GRAPH_ENTRY, FoundryGitGraphEntry))
#define FOUNDRY_IS_GIT_GRAPH_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_TYPE (obj, FOUNDRY_TYPE_GIT_GRAPH_ENTRY))

static GType foundry_git_graph_get_type (void) G_GNUC_CONST;
static GType foundry_git_graph_entry_get_type (void) G_GNUC_CONST;

G_DEFINE_FINAL_TYPE (FoundryGitGraph, foundry_git_graph, FOUNDRY_TYPE_VCS_GRAPH)
G_DEFINE_FINAL_TYPE (FoundryGitGraphEntry, foundry_git_graph_entry, FOUNDRY_TYPE_VCS_GRAPH_ENTRY)

static FoundryGitGraphRow *
foundry_git_graph_get_row (FoundryGitGraph *self,
                           guint            position)
{
  g_assert (FOUNDRY_IS_GIT_GRAPH (self));

  if (position >= rows_get_size (&self->rows))
    return NULL;

  return rows_get (&self->rows, position);
}

static FoundryGitGraphRow *
foundry_git_graph_entry_get_row (FoundryGitGraphEntry *self)
{
  g_assert (FOUNDRY_IS_GIT_GRAPH_ENTRY (self));

  if (self->graph == NULL)
    return NULL;

  return foundry_git_graph_get_row (self->graph, self->position);
}

static guint
foundry_git_graph_get_n_lanes (FoundryVcsGraph *graph)
{
  FoundryGitGraph *self = FOUNDRY_GIT_GRAPH (graph);

  return self->n_lanes;
}

static guint
foundry_git_graph_get_n_items (FoundryVcsGraph *graph)
{
  FoundryGitGraph *self = FOUNDRY_GIT_GRAPH (graph);

  return rows_get_size (&self->rows);
}

static FoundryVcsGraphEntry *
foundry_git_graph_dup_entry (FoundryVcsGraph *graph,
                             guint            position)
{
  FoundryGitGraph *self = FOUNDRY_GIT_GRAPH (graph);
  FoundryGitGraphRow *row;
  FoundryGitGraphEntry *entry;

  if (!(row = foundry_git_graph_get_row (self, position)))
    return NULL;

  if (row->entry != NULL)
    return FOUNDRY_VCS_GRAPH_ENTRY (g_object_ref (row->entry));

  entry = g_object_new (FOUNDRY_TYPE_GIT_GRAPH_ENTRY, NULL);
  entry->graph = self;
  entry->position = position;
  entry->link.data = entry;
  row->entry = entry;
  g_queue_push_tail_link (&self->inflated, &entry->link);

  return FOUNDRY_VCS_GRAPH_ENTRY (entry);
}

static void
foundry_git_graph_finalize (GObject *object)
{
  FoundryGitGraph *self = (FoundryGitGraph *)object;

  while (self->inflated.head != NULL)
    {
      FoundryGitGraphEntry *entry = self->inflated.head->data;

      entry->graph = NULL;
      g_queue_unlink (&self->inflated, &entry->link);
      entry->link.data = NULL;
    }

  segments_clear (&self->segments);
  rows_clear (&self->rows);

  G_OBJECT_CLASS (foundry_git_graph_parent_class)->finalize (object);
}

static void
foundry_git_graph_class_init (FoundryGitGraphClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsGraphClass *graph_class = FOUNDRY_VCS_GRAPH_CLASS (klass);

  object_class->finalize = foundry_git_graph_finalize;

  graph_class->get_n_lanes = foundry_git_graph_get_n_lanes;
  graph_class->get_n_items = foundry_git_graph_get_n_items;
  graph_class->dup_entry = foundry_git_graph_dup_entry;
}

static void
foundry_git_graph_init (FoundryGitGraph *self)
{
  rows_init (&self->rows);
  segments_init (&self->segments);
}

static char *
foundry_git_graph_entry_dup_commit_id (FoundryVcsGraphEntry *entry)
{
  FoundryGitGraphEntry *self = FOUNDRY_GIT_GRAPH_ENTRY (entry);
  FoundryGitGraphRow *row;

  if (!(row = foundry_git_graph_entry_get_row (self)))
    return NULL;

  return _foundry_git_oid_dup_string (&row->oid);
}

static guint
foundry_git_graph_entry_get_commit_lane (FoundryVcsGraphEntry *entry)
{
  FoundryGitGraphEntry *self = FOUNDRY_GIT_GRAPH_ENTRY (entry);
  FoundryGitGraphRow *row;

  if (!(row = foundry_git_graph_entry_get_row (self)))
    return 0;

  return row->commit_lane;
}

static guint
foundry_git_graph_entry_get_n_lanes (FoundryVcsGraphEntry *entry)
{
  FoundryGitGraphEntry *self = FOUNDRY_GIT_GRAPH_ENTRY (entry);
  FoundryGitGraphRow *row;

  if (!(row = foundry_git_graph_entry_get_row (self)))
    return 0;

  return row->n_lanes;
}

static guint
foundry_git_graph_entry_get_n_segments (FoundryVcsGraphEntry *entry)
{
  FoundryGitGraphEntry *self = FOUNDRY_GIT_GRAPH_ENTRY (entry);
  FoundryGitGraphRow *row;

  if (!(row = foundry_git_graph_entry_get_row (self)))
    return 0;

  return row->n_segments;
}

static gboolean
foundry_git_graph_entry_get_segment (FoundryVcsGraphEntry   *entry,
                                     guint                   position,
                                     FoundryVcsGraphSegment *segment)
{
  FoundryGitGraphEntry *self = FOUNDRY_GIT_GRAPH_ENTRY (entry);
  FoundryGitGraphRow *row;

  g_assert (segment != NULL);

  if (!(row = foundry_git_graph_entry_get_row (self)))
    return FALSE;

  if (position >= row->n_segments)
    return FALSE;

  *segment = *segments_get (&self->graph->segments, row->segment_offset + position);

  return TRUE;
}

static void
foundry_git_graph_entry_finalize (GObject *object)
{
  FoundryGitGraphEntry *self = (FoundryGitGraphEntry *)object;
  FoundryGitGraphRow *row;

  if (self->graph != NULL)
    {
      row = foundry_git_graph_get_row (self->graph, self->position);

      if (row != NULL && row->entry == self)
        row->entry = NULL;

      g_queue_unlink (&self->graph->inflated, &self->link);
      self->link.data = NULL;
      self->graph = NULL;
    }

  G_OBJECT_CLASS (foundry_git_graph_entry_parent_class)->finalize (object);
}

static void
foundry_git_graph_entry_class_init (FoundryGitGraphEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsGraphEntryClass *entry_class = FOUNDRY_VCS_GRAPH_ENTRY_CLASS (klass);

  object_class->finalize = foundry_git_graph_entry_finalize;

  entry_class->dup_commit_id = foundry_git_graph_entry_dup_commit_id;
  entry_class->get_commit_lane = foundry_git_graph_entry_get_commit_lane;
  entry_class->get_n_lanes = foundry_git_graph_entry_get_n_lanes;
  entry_class->get_n_segments = foundry_git_graph_entry_get_n_segments;
  entry_class->get_segment = foundry_git_graph_entry_get_segment;
}

static void
foundry_git_graph_entry_init (FoundryGitGraphEntry *self)
{
}

FoundryGitGraphBuilder *
foundry_git_graph_builder_new (void)
{
  FoundryGitGraphBuilder *builder;

  builder = g_new0 (FoundryGitGraphBuilder, 1);
  rows_init (&builder->rows);
  segments_init (&builder->segments);

  return builder;
}

void
foundry_git_graph_builder_add (FoundryGitGraphBuilder *builder,
                               const git_oid          *oid,
                               guint                   commit_lane,
                               guint                   n_lanes,
                               const GArray           *row_segments)
{
  FoundryGitGraphRow row;

  g_return_if_fail (builder != NULL);
  g_return_if_fail (oid != NULL);
  g_return_if_fail (row_segments != NULL);

  row.oid = *oid;
  row.entry = NULL;
  row.segment_offset = segments_get_size (&builder->segments);
  row.n_segments = row_segments->len;
  row.commit_lane = commit_lane;
  row.n_lanes = n_lanes;

  if (row_segments->len > 0)
    segments_splice (&builder->segments,
                     segments_get_size (&builder->segments),
                     0,
                     FALSE,
                     (const FoundryVcsGraphSegment *)row_segments->data,
                     row_segments->len);

  rows_append (&builder->rows, &row);
}

FoundryVcsGraph *
foundry_git_graph_builder_finish (FoundryGitGraphBuilder *builder,
                                  guint                   n_lanes)
{
  FoundryGitGraph *self;

  g_return_val_if_fail (builder != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_GRAPH, NULL);
  self->rows = builder->rows;
  self->segments = builder->segments;
  self->n_lanes = n_lanes;

  rows_init (&builder->rows);
  segments_init (&builder->segments);
  foundry_git_graph_builder_free (builder);

  return FOUNDRY_VCS_GRAPH (self);
}

void
foundry_git_graph_builder_free (FoundryGitGraphBuilder *builder)
{
  if (builder == NULL)
    return;

  rows_clear (&builder->rows);
  segments_clear (&builder->segments);
  g_free (builder);
}
