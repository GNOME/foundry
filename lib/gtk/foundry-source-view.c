/* foundry-source-view.c
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

#include "foundry-source-buffer.h"
#include "foundry-source-view.h"

typedef struct
{
  gpointer dummy;
} FoundrySourceViewPrivate;

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (FoundrySourceView, foundry_source_view, GTK_SOURCE_TYPE_VIEW)

static GParamSpec *properties[N_PROPS];

static void
foundry_source_view_notify_buffer (FoundrySourceView *self)
{
  g_assert (FOUNDRY_IS_SOURCE_VIEW (self));

}

static void
foundry_source_view_finalize (GObject *object)
{
  FoundrySourceView *self = (FoundrySourceView *)object;
  FoundrySourceViewPrivate *priv = foundry_source_view_get_instance_private (self);

  G_OBJECT_CLASS (foundry_source_view_parent_class)->finalize (object);
}

static void
foundry_source_view_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundrySourceView *self = FOUNDRY_SOURCE_VIEW (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_source_view_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  FoundrySourceView *self = FOUNDRY_SOURCE_VIEW (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_source_view_class_init (FoundrySourceViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkTextViewClass *text_view_class = GTK_TEXT_VIEW_CLASS (klass);

  object_class->finalize = foundry_source_view_finalize;
  object_class->get_property = foundry_source_view_get_property;
  object_class->set_property = foundry_source_view_set_property;
}

static void
foundry_source_view_init (FoundrySourceView *self)
{
  g_signal_connect (self,
                    "notify::buffer",
                    G_CALLBACK (foundry_source_view_notify_buffer),
                    NULL);
}

GtkWidget *
foundry_source_view_new (FoundrySourceBuffer *buffer)
{
  g_return_val_if_fail (FOUNDRY_IS_SOURCE_BUFFER (buffer), NULL);

  return g_object_new (FOUNDRY_TYPE_SOURCE_VIEW,
                       "buffer", buffer,
                       NULL);
}
