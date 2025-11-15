/* foundry-progress-icon.c
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
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

#include "foundry-progress-icon-private.h"

#include <math.h>

/**
 * FoundryProgressIcon:
 *
 * A `GdkPaintable` that renders a symbolic pie progress circle.
 *
 * `FoundryProgressIcon` displays a circular progress indicator with
 * a pie slice representing the progress value from 0.0 to 1.0.
 *
 * Since: 1.1
 */

struct _FoundryProgressIcon
{
  GObject parent_instance;

  double progress;
};

static void foundry_progress_icon_iface_init (GdkPaintableInterface *iface);
static void foundry_progress_icon_symbolic_iface_init (GtkSymbolicPaintableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryProgressIcon, foundry_progress_icon, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, foundry_progress_icon_iface_init)
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_SYMBOLIC_PAINTABLE, foundry_progress_icon_symbolic_iface_init))

enum {
  PROP_0,
  PROP_PROGRESS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_progress_icon_snapshot_symbolic (GtkSymbolicPaintable *paintable,
                                         GtkSnapshot          *snapshot,
                                         double                width,
                                         double                height,
                                         const GdkRGBA        *colors,
                                         gsize                 n_colors)
{
  FoundryProgressIcon *self = FOUNDRY_PROGRESS_ICON (paintable);
  GskPathBuilder *builder;
  GskPath *path;
  GdkRGBA color;
  float radius;

  g_assert (FOUNDRY_IS_PROGRESS_ICON (self));
  g_assert (snapshot != NULL);
  g_assert (colors != NULL);
  g_assert (n_colors > 0);

  radius = MIN (width, height) / 2.0f;

  gtk_snapshot_translate (snapshot,
                          &GRAPHENE_POINT_INIT (roundf (width / 2.0f),
                                                roundf (height / 2.0f)));
  gtk_snapshot_rotate (snapshot, -90.0);

  /* gtk_snapshot_push_mask() requires 2 pop below */
  gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_LUMINANCE);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder,
                               &GRAPHENE_POINT_INIT (0, 0),
                               radius);
  path = gsk_path_builder_free_to_path (builder);

  color.red = color.green = color.blue = 0.15f;
  color.alpha = 1.0f;
  gtk_snapshot_append_fill (snapshot, path, GSK_FILL_RULE_WINDING, &color);
  gsk_path_unref (path);

  if (self->progress > 0.0)
    {
      GskStroke *stroke;

      color.red = color.green = color.blue = 1.0f;
      color.alpha = 1.0f;

      stroke = gsk_stroke_new (radius);
      gsk_stroke_set_dash (stroke,
                           (float[2]) { radius * G_PI * self->progress, radius * G_PI },
                           2);

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_circle (builder,
                                   &GRAPHENE_POINT_INIT (0, 0),
                                   radius / 2.0f);

      path = gsk_path_builder_free_to_path (builder);
      gtk_snapshot_append_stroke (snapshot, path, stroke, &color);
      gsk_path_unref (path);
      gsk_stroke_free (stroke);
    }

  gtk_snapshot_pop (snapshot);

  gtk_snapshot_append_color (snapshot,
                             &colors[0],
                             &GRAPHENE_RECT_INIT (-radius, -radius, radius * 2.0f, radius * 2.0f));
  gtk_snapshot_pop (snapshot);
}

static void
foundry_progress_icon_symbolic_iface_init (GtkSymbolicPaintableInterface *iface)
{
  iface->snapshot_symbolic = foundry_progress_icon_snapshot_symbolic;
}

static void
foundry_progress_icon_snapshot (GdkPaintable *paintable,
                                GtkSnapshot  *snapshot,
                                double        width,
                                double        height)
{
  FoundryProgressIcon *self = FOUNDRY_PROGRESS_ICON (paintable);
  GdkRGBA color = {0,0,0,1};

  g_assert (FOUNDRY_IS_PROGRESS_ICON (self));

  gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (self),
                                            snapshot, width, height, &color, 1);
}

static GdkPaintableFlags
foundry_progress_icon_get_flags (GdkPaintable *paintable)
{
  return GDK_PAINTABLE_STATIC_SIZE;
}

static double
foundry_progress_icon_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  return 1.0;
}

static void
foundry_progress_icon_iface_init (GdkPaintableInterface *iface)
{
  iface->snapshot = foundry_progress_icon_snapshot;
  iface->get_flags = foundry_progress_icon_get_flags;
  iface->get_intrinsic_aspect_ratio = foundry_progress_icon_get_intrinsic_aspect_ratio;
}

static void
foundry_progress_icon_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundryProgressIcon *self = FOUNDRY_PROGRESS_ICON (object);

  switch (prop_id)
    {
    case PROP_PROGRESS:
      g_value_set_double (value, foundry_progress_icon_get_progress (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_progress_icon_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  FoundryProgressIcon *self = FOUNDRY_PROGRESS_ICON (object);

  switch (prop_id)
    {
    case PROP_PROGRESS:
      foundry_progress_icon_set_progress (self, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_progress_icon_class_init (FoundryProgressIconClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_progress_icon_get_property;
  object_class->set_property = foundry_progress_icon_set_property;

  /**
   * FoundryProgressIcon:progress:
   *
   * The progress value from 0.0 to 1.0.
   *
   * Since: 1.1
   */
  properties[PROP_PROGRESS] =
    g_param_spec_double ("progress", NULL, NULL,
                         0.0, 1.0, 0.0,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_progress_icon_init (FoundryProgressIcon *self)
{
}

/**
 * foundry_progress_icon_new: (constructor)
 *
 * Creates a new `FoundryProgressIcon`.
 *
 * Returns: (transfer full): the newly created `FoundryProgressIcon`
 *
 * Since: 1.1
 */
FoundryProgressIcon *
foundry_progress_icon_new (void)
{
  return g_object_new (FOUNDRY_TYPE_PROGRESS_ICON, NULL);
}

/**
 * foundry_progress_icon_get_progress:
 * @self: a progress icon
 *
 * Gets the progress value.
 *
 * Returns: the progress value from 0.0 to 1.0
 *
 * Since: 1.1
 */
double
foundry_progress_icon_get_progress (FoundryProgressIcon *self)
{
  g_return_val_if_fail (FOUNDRY_IS_PROGRESS_ICON (self), 0.0);

  return self->progress;
}

/**
 * foundry_progress_icon_set_progress:
 * @self: a progress icon
 * @progress: the progress value from 0.0 to 1.0
 *
 * Sets the progress value.
 *
 * Since: 1.1
 */
void
foundry_progress_icon_set_progress (FoundryProgressIcon *self,
                                    double               progress)
{
  g_return_if_fail (FOUNDRY_IS_PROGRESS_ICON (self));

  progress = CLAMP (progress, 0.0, 1.0);

  if (self->progress != progress)
    {
      self->progress = progress;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROGRESS]);
      gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
    }
}
