/* foundry-diagnostics-gutter-renderer.c
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

#include "foundry-diagnostics-gutter-renderer.h"

struct _FoundryDiagnosticsGutterRenderer
{
  GtkSourceGutterRenderer   parent_instance;
  FoundryOnTypeDiagnostics *diagnostics;
};

enum {
  PROP_0,
  PROP_DIAGNOSTICS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryDiagnosticsGutterRenderer, foundry_diagnostics_gutter_renderer, GTK_SOURCE_TYPE_GUTTER_RENDERER)

static GParamSpec *properties[N_PROPS];

static void
foundry_diagnostics_gutter_renderer_dispose (GObject *object)
{
  FoundryDiagnosticsGutterRenderer *self = (FoundryDiagnosticsGutterRenderer *)object;

  g_clear_object (&self->diagnostics);

  G_OBJECT_CLASS (foundry_diagnostics_gutter_renderer_parent_class)->dispose (object);
}

static void
foundry_diagnostics_gutter_renderer_get_property (GObject    *object,
                                                  guint       prop_id,
                                                  GValue     *value,
                                                  GParamSpec *pspec)
{
  FoundryDiagnosticsGutterRenderer *self = FOUNDRY_DIAGNOSTICS_GUTTER_RENDERER (object);

  switch (prop_id)
    {
    case PROP_DIAGNOSTICS:
      g_value_take_object (value, foundry_diagnostics_gutter_renderer_dup_diagnostics (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_diagnostics_gutter_renderer_set_property (GObject      *object,
                                                  guint         prop_id,
                                                  const GValue *value,
                                                  GParamSpec   *pspec)
{
  FoundryDiagnosticsGutterRenderer *self = FOUNDRY_DIAGNOSTICS_GUTTER_RENDERER (object);

  switch (prop_id)
    {
    case PROP_DIAGNOSTICS:
      foundry_diagnostics_gutter_renderer_set_diagnostics (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_diagnostics_gutter_renderer_class_init (FoundryDiagnosticsGutterRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_diagnostics_gutter_renderer_dispose;
  object_class->get_property = foundry_diagnostics_gutter_renderer_get_property;
  object_class->set_property = foundry_diagnostics_gutter_renderer_set_property;

  properties[PROP_DIAGNOSTICS] =
    g_param_spec_object ("diagnostics", NULL, NULL,
                         FOUNDRY_TYPE_ON_TYPE_DIAGNOSTICS,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_diagnostics_gutter_renderer_init (FoundryDiagnosticsGutterRenderer *self)
{
}

GtkSourceGutterRenderer *
foundry_diagnostics_gutter_renderer_new (FoundryOnTypeDiagnostics *diagnostics)
{
  g_return_val_if_fail (!diagnostics || FOUNDRY_IS_ON_TYPE_DIAGNOSTICS (diagnostics), NULL);

  return g_object_new (FOUNDRY_TYPE_DIAGNOSTICS_GUTTER_RENDERER,
                       "diagnostics", diagnostics,
                       NULL);
}

/**
 * foundry_diagnostics_gutter_renderer_dup_diagnostics:
 * @self: a [class@FoundryGtk.DiagnosticsGutterRenderer]
 *
 * Returns: (transfer full) (nullable):
 */
FoundryOnTypeDiagnostics *
foundry_diagnostics_gutter_renderer_dup_diagnostics (FoundryDiagnosticsGutterRenderer *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DIAGNOSTICS_GUTTER_RENDERER (self), NULL);

  if (self->diagnostics)
    return g_object_ref (self->diagnostics);

  return NULL;
}

void
foundry_diagnostics_gutter_renderer_set_diagnostics (FoundryDiagnosticsGutterRenderer *self,
                                                     FoundryOnTypeDiagnostics         *diagnostics)
{
  g_return_if_fail (FOUNDRY_IS_DIAGNOSTICS_GUTTER_RENDERER (self));

  if (g_set_object (&self->diagnostics, diagnostics))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DIAGNOSTICS]);
}
