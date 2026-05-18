/* foundry-git-lanes.c
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

#include "foundry-git-lanes.h"

/**
 * FoundryGitLanes:
 *
 * A widget that displays Git graph lanes for a [class@Foundry.VcsGraphEntry].
 *
 * Since: 1.2
 */

#define LANE_WIDTH 16
#define ROW_HEIGHT 28

#ifndef GDK_RGBA
# define _GDK_RGBA_DECODE(c) ((unsigned)(((c) >= 'A' && (c) <= 'F') ? ((c)-'A'+10) : \
                                         ((c) >= 'a' && (c) <= 'f') ? ((c)-'a'+10) : \
                                         ((c) >= '0' && (c) <= '9') ? ((c)-'0') : \
                                         -1))
# define _GDK_RGBA_SELECT_COLOR(str, index3, index6) (sizeof(str) <= 4 ? \
                                                      _GDK_RGBA_DECODE ((str)[index3]) : \
                                                      _GDK_RGBA_DECODE ((str)[index6]))
# define GDK_RGBA(str) ((GdkRGBA) { \
    ((_GDK_RGBA_SELECT_COLOR (str, 0, 0) << 4) | \
     _GDK_RGBA_SELECT_COLOR (str, 0, 1)) / 255., \
    ((_GDK_RGBA_SELECT_COLOR (str, 1, 2) << 4) | \
     _GDK_RGBA_SELECT_COLOR (str, 1, 3)) / 255., \
    ((_GDK_RGBA_SELECT_COLOR (str, 2, 4) << 4) | \
     _GDK_RGBA_SELECT_COLOR (str, 2, 5)) / 255., \
    ((sizeof(str) % 4 == 1) ? \
     ((_GDK_RGBA_SELECT_COLOR (str, 3, 6) << 4) | \
      _GDK_RGBA_SELECT_COLOR (str, 3, 7)) : 0xFF) / 255. })
#endif

struct _FoundryGitLanes
{
  GtkWidget parent_instance;

  FoundryVcsGraphEntry *entry;
  GdkRGBA *colors;
  guint n_colors;
};

G_DEFINE_FINAL_TYPE (FoundryGitLanes, foundry_git_lanes, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_ENTRY,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static const GdkRGBA default_lane_colors[] = {
  GDK_RGBA ("#3584e4"), GDK_RGBA ("#f5c211"), GDK_RGBA ("#33d17a"),
  GDK_RGBA ("#ff7800"), GDK_RGBA ("#9141ac"), GDK_RGBA ("#e01b24"),
  GDK_RGBA ("#986a44"),
  GDK_RGBA ("#1c71d8"), GDK_RGBA ("#e5a50a"), GDK_RGBA ("#2ec27e"),
  GDK_RGBA ("#e66100"), GDK_RGBA ("#813d9c"), GDK_RGBA ("#c01c28"),
  GDK_RGBA ("#865e3c"),
  GDK_RGBA ("#62a0ea"), GDK_RGBA ("#57e389"),
  GDK_RGBA ("#ffa348"), GDK_RGBA ("#c061cb"), GDK_RGBA ("#ed333b"),
  GDK_RGBA ("#b5835a"),
  GDK_RGBA ("#1a5fb4"), GDK_RGBA ("#26a269"),
  GDK_RGBA ("#c64600"), GDK_RGBA ("#613583"), GDK_RGBA ("#a51d2d"),
  GDK_RGBA ("#63452c"),
};

static const GdkRGBA *
color_for_id (FoundryGitLanes *self,
              guint            color_id)
{
  const GdkRGBA *colors;
  guint n_colors;

  g_assert (FOUNDRY_IS_GIT_LANES (self));

  if (self->colors != NULL)
    {
      colors = self->colors;
      n_colors = self->n_colors;
    }
  else
    {
      colors = default_lane_colors;
      n_colors = G_N_ELEMENTS (default_lane_colors);
    }

  return &colors[color_id % n_colors];
}

static double
lane_x (guint lane)
{
  return lane * LANE_WIDTH + (LANE_WIDTH / 2.0);
}

static double
point_y (FoundryVcsGraphPoint point,
         int                  height)
{
  switch (point)
    {
    case FOUNDRY_VCS_GRAPH_POINT_TOP:
      return 0;

    case FOUNDRY_VCS_GRAPH_POINT_CENTER:
      return height / 2.0;

    case FOUNDRY_VCS_GRAPH_POINT_BOTTOM:
      return height;

    default:
      g_assert_not_reached ();
    }
}

static void
append_segment (GskPathBuilder               *builder,
                const FoundryVcsGraphSegment *segment,
                int                           height)
{
  double from_x;
  double from_y;
  double to_x;
  double to_y;

  g_assert (builder != NULL);
  g_assert (segment != NULL);

  from_x = lane_x (segment->from_lane);
  from_y = point_y (segment->from_point, height);
  to_x = lane_x (segment->to_lane);
  to_y = point_y (segment->to_point, height);

  gsk_path_builder_move_to (builder, from_x, from_y);

  if (segment->from_lane != segment->to_lane)
    gsk_path_builder_cubic_to (builder,
                               from_x, from_y + (height * 0.4),
                               to_x, to_y - (height * 0.4),
                               to_x, to_y);
  else
    gsk_path_builder_line_to (builder, to_x, to_y);
}

static guint
foundry_git_lanes_get_required_lanes (FoundryGitLanes *self)
{
  guint n_lanes;
  guint n_segments;

  g_assert (FOUNDRY_IS_GIT_LANES (self));

  if (self->entry == NULL)
    return 1;

  n_lanes = foundry_vcs_graph_entry_get_commit_lane (self->entry) + 1;
  n_segments = foundry_vcs_graph_entry_get_n_segments (self->entry);

  for (guint i = 0; i < n_segments; i++)
    {
      FoundryVcsGraphSegment segment;

      if (!foundry_vcs_graph_entry_get_segment (self->entry, i, &segment))
        continue;

      n_lanes = MAX (n_lanes, segment.from_lane + 1);
      n_lanes = MAX (n_lanes, segment.to_lane + 1);
    }

  return MAX (1, n_lanes);
}

static guint
foundry_git_lanes_get_commit_color_id (FoundryGitLanes *self)
{
  guint commit_lane;
  guint n_segments;

  g_assert (FOUNDRY_IS_GIT_LANES (self));

  if (self->entry == NULL)
    return 0;

  commit_lane = foundry_vcs_graph_entry_get_commit_lane (self->entry);
  n_segments = foundry_vcs_graph_entry_get_n_segments (self->entry);

  for (guint i = 0; i < n_segments; i++)
    {
      FoundryVcsGraphSegment segment;

      if (!foundry_vcs_graph_entry_get_segment (self->entry, i, &segment))
        continue;

      if (segment.from_lane == commit_lane || segment.to_lane == commit_lane)
        return segment.color_id;
    }

  return commit_lane;
}

static void
foundry_git_lanes_snapshot (GtkWidget   *widget,
                            GtkSnapshot *snapshot)
{
  FoundryGitLanes *self = (FoundryGitLanes *)widget;
  g_autoptr(GskStroke) stroke = NULL;
  g_autoptr(GskPathBuilder) circle_builder = NULL;
  g_autoptr(GskPath) circle_path = NULL;
  graphene_point_t circle_center;
  int height;
  guint n_segments;
  guint commit_lane;
  guint commit_color_id;

  g_assert (FOUNDRY_IS_GIT_LANES (self));

  if (self->entry == NULL)
    return;

  height = gtk_widget_get_height (widget);
  n_segments = foundry_vcs_graph_entry_get_n_segments (self->entry);
  commit_lane = foundry_vcs_graph_entry_get_commit_lane (self->entry);
  commit_color_id = foundry_git_lanes_get_commit_color_id (self);

  stroke = gsk_stroke_new (2.0);
  gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_ROUND);
  gsk_stroke_set_line_join (stroke, GSK_LINE_JOIN_ROUND);

  for (guint i = 0; i < n_segments; i++)
    {
      g_autoptr(GskPathBuilder) builder = NULL;
      g_autoptr(GskPath) path = NULL;
      FoundryVcsGraphSegment segment;
      const GdkRGBA *color;

      if (!foundry_vcs_graph_entry_get_segment (self->entry, i, &segment))
        continue;

      builder = gsk_path_builder_new ();
      append_segment (builder, &segment, height);
      path = gsk_path_builder_free_to_path (g_steal_pointer (&builder));
      color = color_for_id (self, segment.color_id);
      gtk_snapshot_append_stroke (snapshot, path, stroke, color);
    }

  circle_builder = gsk_path_builder_new ();
  graphene_point_init (&circle_center, lane_x (commit_lane), height / 2.0);
  gsk_path_builder_add_circle (circle_builder, &circle_center, 4.5);
  circle_path = gsk_path_builder_free_to_path (g_steal_pointer (&circle_builder));

  gtk_snapshot_append_fill (snapshot,
                            circle_path,
                            GSK_FILL_RULE_WINDING,
                            color_for_id (self, commit_color_id));
}

static void
foundry_git_lanes_measure (GtkWidget      *widget,
                           GtkOrientation  orientation,
                           int             for_size,
                           int            *minimum,
                           int            *natural,
                           int            *minimum_baseline,
                           int            *natural_baseline)
{
  FoundryGitLanes *self = (FoundryGitLanes *)widget;

  g_assert (FOUNDRY_IS_GIT_LANES (self));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      guint n_lanes = foundry_git_lanes_get_required_lanes (self);

      *minimum = n_lanes * LANE_WIDTH;
      *natural = *minimum;
    }
  else
    {
      *minimum = ROW_HEIGHT;
      *natural = ROW_HEIGHT;
    }

  *minimum_baseline = -1;
  *natural_baseline = -1;
}

static void
foundry_git_lanes_dispose (GObject *object)
{
  FoundryGitLanes *self = (FoundryGitLanes *)object;

  g_clear_object (&self->entry);

  G_OBJECT_CLASS (foundry_git_lanes_parent_class)->dispose (object);
}

static void
foundry_git_lanes_finalize (GObject *object)
{
  FoundryGitLanes *self = (FoundryGitLanes *)object;

  g_clear_pointer (&self->colors, g_free);

  G_OBJECT_CLASS (foundry_git_lanes_parent_class)->finalize (object);
}

static void
foundry_git_lanes_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  FoundryGitLanes *self = FOUNDRY_GIT_LANES (object);

  switch (prop_id)
    {
    case PROP_ENTRY:
      g_value_set_object (value, self->entry);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_git_lanes_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  FoundryGitLanes *self = FOUNDRY_GIT_LANES (object);

  switch (prop_id)
    {
    case PROP_ENTRY:
      foundry_git_lanes_set_entry (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_git_lanes_class_init (FoundryGitLanesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = foundry_git_lanes_dispose;
  object_class->finalize = foundry_git_lanes_finalize;
  object_class->get_property = foundry_git_lanes_get_property;
  object_class->set_property = foundry_git_lanes_set_property;

  widget_class->snapshot = foundry_git_lanes_snapshot;
  widget_class->measure = foundry_git_lanes_measure;

  /**
   * FoundryGitLanes:entry:
   *
   * The graph entry to render.
   *
   * Since: 1.2
   */
  properties[PROP_ENTRY] =
    g_param_spec_object ("entry", NULL, NULL,
                         FOUNDRY_TYPE_VCS_GRAPH_ENTRY,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_git_lanes_init (FoundryGitLanes *self)
{
}

/**
 * foundry_git_lanes_new:
 *
 * Creates a new widget to draw Git graph lanes for a [class@Foundry.VcsGraphEntry].
 *
 * Returns: (transfer full): a newly created [class@FoundryGtk.GitLanes]
 * Since: 1.2
 */
GtkWidget *
foundry_git_lanes_new (void)
{
  return g_object_new (FOUNDRY_TYPE_GIT_LANES, NULL);
}

/**
 * foundry_git_lanes_dup_entry:
 * @self: a [class@FoundryGtk.GitLanes]
 *
 * Gets the graph entry displayed by @self.
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.VcsGraphEntry]
 * Since: 1.2
 */
FoundryVcsGraphEntry *
foundry_git_lanes_dup_entry (FoundryGitLanes *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_LANES (self), NULL);

  return self->entry ? g_object_ref (self->entry) : NULL;
}

/**
 * foundry_git_lanes_set_entry:
 * @self: a [class@FoundryGtk.GitLanes]
 * @entry: (nullable): a [class@Foundry.VcsGraphEntry]
 *
 * Sets the graph entry displayed by @self.
 *
 * Since: 1.2
 */
void
foundry_git_lanes_set_entry (FoundryGitLanes     *self,
                             FoundryVcsGraphEntry *entry)
{
  g_return_if_fail (FOUNDRY_IS_GIT_LANES (self));
  g_return_if_fail (!entry || FOUNDRY_IS_VCS_GRAPH_ENTRY (entry));

  if (g_set_object (&self->entry, entry))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENTRY]);
      gtk_widget_queue_resize (GTK_WIDGET (self));
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

/**
 * foundry_git_lanes_set_lane_colors:
 * @self: a [class@FoundryGtk.GitLanes]
 * @colors: (nullable) (array length=n_colors): the lane colors
 * @n_colors: the number of colors in @colors
 *
 * Sets the colors to use when drawing graph lanes.
 *
 * If @colors is %NULL, the default lane colors are used. When @colors is not
 * %NULL, the colors are copied and @n_colors must be greater than zero.
 *
 * Since: 1.2
 */
void
foundry_git_lanes_set_lane_colors (FoundryGitLanes *self,
                                   const GdkRGBA   *colors,
                                   guint            n_colors)
{
  g_return_if_fail (FOUNDRY_IS_GIT_LANES (self));
  g_return_if_fail (colors == NULL || n_colors > 0);

  g_clear_pointer (&self->colors, g_free);
  self->n_colors = 0;

  if (colors != NULL)
    {
      self->colors = g_memdup2 (colors, sizeof *colors * n_colors);
      self->n_colors = n_colors;
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}
