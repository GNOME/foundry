/* foundry-panel-bar.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-panel-bar.h"

struct _FoundryPanelBar
{
  GtkWidget parent_instance;
  FoundryWorkspace *workspace;
  guint show_start : 1;
  guint show_bottom : 1;
};

G_DEFINE_FINAL_TYPE (FoundryPanelBar, foundry_panel_bar, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_WORKSPACE,
  PROP_SHOW_START,
  PROP_SHOW_BOTTOM,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_panel_bar_dispose (GObject *object)
{
  FoundryPanelBar *self = (FoundryPanelBar *)object;

  gtk_widget_dispose_template (GTK_WIDGET (object), FOUNDRY_TYPE_PANEL_BAR);

  g_clear_weak_pointer (&self->workspace);

  G_OBJECT_CLASS (foundry_panel_bar_parent_class)->dispose (object);
}

static void
foundry_panel_bar_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  FoundryPanelBar *self = FOUNDRY_PANEL_BAR (object);

  switch (prop_id)
    {
    case PROP_SHOW_BOTTOM:
      g_value_set_boolean (value, foundry_panel_bar_get_show_bottom (self));
      break;

    case PROP_SHOW_START:
      g_value_set_boolean (value, foundry_panel_bar_get_show_start (self));
      break;

    case PROP_WORKSPACE:
      g_value_set_object (value, foundry_panel_bar_get_workspace (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_panel_bar_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  FoundryPanelBar *self = FOUNDRY_PANEL_BAR (object);

  switch (prop_id)
    {
    case PROP_SHOW_BOTTOM:
      foundry_panel_bar_set_show_bottom (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_START:
      foundry_panel_bar_set_show_start (self, g_value_get_boolean (value));
      break;

    case PROP_WORKSPACE:
      foundry_panel_bar_set_workspace (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_panel_bar_class_init (FoundryPanelBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = foundry_panel_bar_dispose;
  object_class->get_property = foundry_panel_bar_get_property;
  object_class->set_property = foundry_panel_bar_set_property;

  properties[PROP_SHOW_START] =
    g_param_spec_boolean ("show-start", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SHOW_BOTTOM] =
    g_param_spec_boolean ("show-bottom", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_WORKSPACE] =
    g_param_spec_object ("workspace", NULL, NULL,
                         FOUNDRY_TYPE_WORKSPACE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/app/devsuite/foundry-adw/ui/foundry-panel-bar.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
foundry_panel_bar_init (FoundryPanelBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
foundry_panel_bar_new (void)
{
  return g_object_new (FOUNDRY_TYPE_PANEL_BAR, NULL);
}

gboolean
foundry_panel_bar_get_show_bottom (FoundryPanelBar *self)
{
  g_return_val_if_fail (FOUNDRY_IS_PANEL_BAR (self), FALSE);

  return self->show_bottom;
}

gboolean
foundry_panel_bar_get_show_start (FoundryPanelBar *self)
{
  g_return_val_if_fail (FOUNDRY_IS_PANEL_BAR (self), FALSE);

  return self->show_start;
}

void
foundry_panel_bar_set_show_bottom (FoundryPanelBar *self,
                                   gboolean         show_bottom)
{
  g_return_if_fail (FOUNDRY_IS_PANEL_BAR (self));

  show_bottom = !!show_bottom;

  if (show_bottom != self->show_bottom)
    {
      self->show_bottom = !!show_bottom;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_BOTTOM]);
    }
}

void
foundry_panel_bar_set_show_start (FoundryPanelBar *self,
                                  gboolean         show_start)
{
  g_return_if_fail (FOUNDRY_IS_PANEL_BAR (self));

  show_start = !!show_start;

  if (show_start != self->show_start)
    {
      self->show_start = !!show_start;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_START]);
    }
}

/**
 * foundry_panel_bar_get_workspace:
 * @self: a [class@Foundry.PanelBar]
 *
 * Returns: (transfer none) (nullable):
 */
FoundryWorkspace *
foundry_panel_bar_get_workspace (FoundryPanelBar *self)
{
  g_return_val_if_fail (FOUNDRY_IS_PANEL_BAR (self), NULL);

  return self->workspace;
}

void
foundry_panel_bar_set_workspace (FoundryPanelBar  *self,
                                 FoundryWorkspace *workspace)
{
  g_return_if_fail (FOUNDRY_IS_PANEL_BAR (self));
  g_return_if_fail (FOUNDRY_IS_WORKSPACE (workspace));

  if (g_set_weak_pointer (&self->workspace, workspace))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WORKSPACE]);
}
