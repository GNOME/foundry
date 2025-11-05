/*
 * foundry-panel.c
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-panel-private.h"

typedef struct
{
  GtkEventControllerFocus *focus_controller;
  GtkWidget               *child;
  char                    *id;
  char                    *title;
  GIcon                   *icon;
  guint                    needs_attention : 1;
} FoundryPanelPrivate;

enum {
  PROP_0,
  PROP_CHILD,
  PROP_ICON,
  PROP_ICON_NAME,
  PROP_ID,
  PROP_NEEDS_ATTENTION,
  PROP_TITLE,
  N_PROPS
};

enum {
  PRESENTED,
  RAISE,
  N_SIGNALS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryPanel, foundry_panel, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS];
static guint       signals[N_SIGNALS];

static void
foundry_panel_real_presented (FoundryPanel *panel)
{
}

static void
foundry_panel_focus_controller_notify_cb (FoundryPanel            *self,
                                          GParamSpec              *pspec,
                                          GtkEventControllerFocus *focus)
{
  g_assert (FOUNDRY_IS_PANEL (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_FOCUS (focus));

  if (gtk_event_controller_focus_contains_focus (focus))
    foundry_panel_set_needs_attention (self, FALSE);
}

static void
foundry_panel_dispose (GObject *object)
{
  FoundryPanel *self = (FoundryPanel *)object;
  FoundryPanelPrivate *priv = foundry_panel_get_instance_private (self);
  GtkWidget *child;

  priv->child = NULL;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);

  g_clear_object (&priv->icon);
  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->id, g_free);

  G_OBJECT_CLASS (foundry_panel_parent_class)->dispose (object);
}

static void
foundry_panel_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  FoundryPanel *self = FOUNDRY_PANEL (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, foundry_panel_get_id (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, foundry_panel_get_title (self));
      break;

    case PROP_ICON:
      g_value_set_object (value, foundry_panel_get_icon (self));
      break;

    case PROP_CHILD:
      g_value_set_object (value, foundry_panel_get_child (self));
      break;

    case PROP_NEEDS_ATTENTION:
      g_value_set_boolean (value, foundry_panel_get_needs_attention (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_panel_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  FoundryPanel *self = FOUNDRY_PANEL (object);
  FoundryPanelPrivate *priv = foundry_panel_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ID:
      priv->id = g_value_dup_string (value);
      break;

    case PROP_ICON:
      foundry_panel_set_icon (self, g_value_get_object (value));
      break;

    case PROP_ICON_NAME:
      foundry_panel_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      foundry_panel_set_title (self, g_value_get_string (value));
      break;

    case PROP_CHILD:
      foundry_panel_set_child (self, g_value_get_object (value));
      break;

    case PROP_NEEDS_ATTENTION:
      foundry_panel_set_needs_attention (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_panel_class_init (FoundryPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = foundry_panel_dispose;
  object_class->get_property = foundry_panel_get_property;
  object_class->set_property = foundry_panel_set_property;

  klass->presented = foundry_panel_real_presented;

  /**
   * FoundryPanel:presented:
   *
   * Called when the panel has been raised in the stacking
   * order causing it to be displayed to the user.
   *
   * Since: 1.1
   */
  signals[PRESENTED] =
    g_signal_new ("presented",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (FoundryPanelClass, presented),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  signals[RAISE] =
    g_signal_new ("raise",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_CHILD] =
    g_param_spec_object ("child", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL,
                         NULL,
                         (G_PARAM_WRITABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_NEEDS_ATTENTION] =
    g_param_spec_boolean ("needs-attention", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/app/devsuite/foundry-adw/ui/foundry-panel.ui");
  gtk_widget_class_bind_template_child_private (widget_class, FoundryPanel, focus_controller);
  gtk_widget_class_bind_template_callback (widget_class, foundry_panel_focus_controller_notify_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
foundry_panel_init (FoundryPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

FoundryPanel *
foundry_panel_new (const char *id)
{
  g_return_val_if_fail (id != NULL, NULL);

  return g_object_new (FOUNDRY_TYPE_PANEL,
                       "id", id,
                       NULL);
}

const char *
foundry_panel_get_id (FoundryPanel *self)
{
  FoundryPanelPrivate *priv = foundry_panel_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_PANEL (self), NULL);

  return priv->id;
}

/**
 * foundry_panel_get_title:
 * @self: a [class@FoundryAdw.Panel]
 *
 * Returns: (nullable):
 */
const char *
foundry_panel_get_title (FoundryPanel *self)
{
  FoundryPanelPrivate *priv = foundry_panel_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_PANEL (self), NULL);

  return priv->title;
}

void
foundry_panel_set_title (FoundryPanel *self,
                         const char   *title)
{
  FoundryPanelPrivate *priv = foundry_panel_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_PANEL (self));

  if (g_set_str (&priv->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

/**
 * foundry_panel_get_icon:
 * @self: a [class@FoundryAdw.Panel]
 *
 * Returns: (transfer none) (nullable):
 */
GIcon *
foundry_panel_get_icon (FoundryPanel *self)
{
  FoundryPanelPrivate *priv = foundry_panel_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_PANEL (self), NULL);

  return priv->icon;
}

void
foundry_panel_set_icon (FoundryPanel *self,
                        GIcon        *icon)
{
  FoundryPanelPrivate *priv = foundry_panel_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_PANEL (self));

  if (g_set_object (&priv->icon, icon))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON]);
}

void
foundry_panel_set_icon_name (FoundryPanel *self,
                             const char   *icon_name)
{
  FoundryPanelPrivate *priv = foundry_panel_get_instance_private (self);
  g_autoptr(GIcon) icon = NULL;

  g_return_if_fail (FOUNDRY_IS_PANEL (self));

  if (icon_name != NULL)
    icon = g_themed_icon_new (icon_name);

  if (g_set_object (&priv->icon, icon))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON]);
}

/**
 * foundry_panel_get_child:
 * @self: a [class@FoundryAdw.Panel]
 *
 * Returns: (transfer none) (nullable):
 */
GtkWidget *
foundry_panel_get_child (FoundryPanel *self)
{
  FoundryPanelPrivate *priv = foundry_panel_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_PANEL (self), NULL);

  return priv->child;
}

void
foundry_panel_set_child (FoundryPanel *self,
                         GtkWidget    *child)
{
  FoundryPanelPrivate *priv = foundry_panel_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_PANEL (self));
  g_return_if_fail (!child || GTK_IS_WIDGET (child));

  if (child != NULL)
    gtk_widget_set_parent (child, GTK_WIDGET (self));

  g_clear_pointer (&priv->child, gtk_widget_unparent);
  priv->child = child;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

/**
 * foundry_panel_raise:
 * @self: a [class@FoundryAdw.Panel]
 *
 * Emits the "raise" signal on the panel.
 *
 * This signal can be used to request that the panel be raised to the front
 * or made visible in its container.
 */
void
foundry_panel_raise (FoundryPanel *self)
{
  g_return_if_fail (FOUNDRY_IS_PANEL (self));

  g_signal_emit (self, signals[RAISE], 0);
}

void
_foundry_panel_emit_presented (FoundryPanel *self)
{
  g_return_if_fail (FOUNDRY_IS_PANEL (self));

  g_signal_emit (self, signals[PRESENTED], 0);
}

/**
 * foundry_panel_get_needs_attention:
 * @self: a [class@FoundryAdw.Panel]
 *
 * Gets whether the panel needs attention from the user.
 *
 * Returns: %TRUE if the panel needs attention, %FALSE otherwise
 *
 * Since: 1.1
 */
gboolean
foundry_panel_get_needs_attention (FoundryPanel *self)
{
  FoundryPanelPrivate *priv = foundry_panel_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_PANEL (self), FALSE);

  return priv->needs_attention;
}

/**
 * foundry_panel_set_needs_attention:
 * @self: a [class@FoundryAdw.Panel]
 * @needs_attention: whether the panel needs attention
 *
 * Sets whether the panel needs attention from the user.
 *
 * When set to %TRUE, this property indicates that the panel has
 * something that requires user attention. The property is automatically
 * cleared when focus enters the panel.
 *
 * Since: 1.1
 */
void
foundry_panel_set_needs_attention (FoundryPanel *self,
                                   gboolean      needs_attention)
{
  FoundryPanelPrivate *priv = foundry_panel_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_PANEL (self));

  /* Always ignore request if/when we already have focus */
  if (gtk_event_controller_focus_contains_focus (priv->focus_controller))
    needs_attention = FALSE;

  needs_attention = !!needs_attention;

  if (priv->needs_attention != needs_attention)
    {
      priv->needs_attention = needs_attention;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NEEDS_ATTENTION]);
    }
}
