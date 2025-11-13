/* foundry-path-bar-button.c
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

#include "foundry-path-bar-button.h"
#include "foundry-workspace.h"
#include "foundry.h"

struct _FoundryPathBarButton
{
  GtkWidget             parent_instance;

  FoundryPathNavigator *navigator;

  GtkImage             *image;
  GtkLabel             *label;
  GtkMenuButton        *menu_button;
  GtkPopover           *popover;
  GtkScrolledWindow    *popover_scroller;
  GtkListView          *list_view;
  GtkNoSelection       *selection;
};

enum {
  PROP_0,
  PROP_NAVIGATOR,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryPathBarButton, foundry_path_bar_button, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS];

static void
foundry_path_bar_button_activate_cb (FoundryPathBarButton *self,
                                     guint                 position,
                                     GtkListView          *list_view)
{
  g_autoptr(FoundryPathNavigator) navigator = NULL;
  g_autoptr(FoundryIntentManager) intent_manager = NULL;
  g_autoptr(FoundryIntent) intent = NULL;
  FoundryWorkspace *workspace;
  FoundryContext *context;
  GtkWidget *ancestor;

  g_assert (FOUNDRY_IS_PATH_BAR_BUTTON (self));
  g_assert (GTK_IS_LIST_VIEW (list_view));

  if (!(navigator = g_list_model_get_item (G_LIST_MODEL (self->selection), position)))
    return;

  gtk_popover_popdown (self->popover);

  if ((intent = foundry_path_navigator_dup_intent (navigator)) &&
      (ancestor = gtk_widget_get_ancestor (GTK_WIDGET (self), FOUNDRY_TYPE_WORKSPACE)) &&
      (workspace = FOUNDRY_WORKSPACE (ancestor)) &&
      (context = foundry_workspace_get_context (workspace)) &&
      (intent_manager = foundry_context_dup_intent_manager (context)))
    {
      GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));

      if (root != NULL)
        foundry_intent_set_attribute (intent, "window", GTK_TYPE_ROOT, root);

      dex_future_disown (foundry_intent_manager_dispatch (intent_manager, intent));
    }
}

static void
foundry_path_bar_button_button_clicked_cb (FoundryPathBarButton *self,
                                           int                   n_press,
                                           double                x,
                                           double                y,
                                           GtkGestureClick      *gesture)
{
  g_autoptr(FoundryIntentManager) intent_manager = NULL;
  g_autoptr(FoundryIntent) intent = NULL;
  FoundryWorkspace *workspace;
  FoundryContext *context;
  GtkWidget *ancestor;

  g_assert (FOUNDRY_IS_PATH_BAR_BUTTON (self));
  g_assert (GTK_IS_GESTURE_CLICK (gesture));

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  if (self->navigator == NULL)
    return;

  if ((intent = foundry_path_navigator_dup_intent (self->navigator)) &&
      (ancestor = gtk_widget_get_ancestor (GTK_WIDGET (self), FOUNDRY_TYPE_WORKSPACE)) &&
      (workspace = FOUNDRY_WORKSPACE (ancestor)) &&
      (context = foundry_workspace_get_context (workspace)) &&
      (intent_manager = foundry_context_dup_intent_manager (context)))
    {
      GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));

      if (root != NULL)
        foundry_intent_set_attribute (intent, "window", GTK_TYPE_ROOT, root);

      dex_future_disown (foundry_intent_manager_dispatch (intent_manager, intent));
    }
}

static DexFuture *
foundry_path_bar_button_populate_siblings (DexFuture *completed,
                                           gpointer   user_data)
{
  FoundryPathBarButton *self = user_data;
  g_autoptr(GListModel) model = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_PATH_BAR_BUTTON (self));
  g_assert (DEX_IS_FUTURE (completed));

  model = dex_await_object (dex_ref (completed), &error);
  gtk_no_selection_set_model (self->selection, model);

  if (model != NULL && g_list_model_get_n_items (model) > 0)
    gtk_popover_popup (self->popover);

  return dex_future_new_for_boolean (TRUE);
}

static void
foundry_path_bar_button_show_popover (FoundryPathBarButton *self)
{
  DexFuture *future;

  g_assert (FOUNDRY_IS_PATH_BAR_BUTTON (self));

  if (self->navigator == NULL)
    return;

  future = foundry_path_navigator_list_siblings (self->navigator);
  future = dex_future_then (future,
                            foundry_path_bar_button_populate_siblings,
                            g_object_ref (self),
                            g_object_unref);
  dex_future_disown (future);
}

static void
foundry_path_bar_button_gesture_pressed_cb (FoundryPathBarButton *self,
                                            int                   n_press,
                                            double                x,
                                            double                y,
                                            GtkGestureClick      *gesture)
{
  int button;

  g_assert (FOUNDRY_IS_PATH_BAR_BUTTON (self));
  g_assert (GTK_IS_GESTURE_CLICK (gesture));

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button == GDK_BUTTON_SECONDARY)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
      foundry_path_bar_button_show_popover (self);
    }
}

static void
foundry_path_bar_button_popover_closed_cb (FoundryPathBarButton *self,
                                           GtkPopover           *popover)
{
  g_assert (FOUNDRY_IS_PATH_BAR_BUTTON (self));
  g_assert (GTK_IS_POPOVER (popover));

  gtk_no_selection_set_model (self->selection, NULL);
}

static void
foundry_path_bar_button_popover_show_cb (FoundryPathBarButton *self,
                                         GtkPopover           *popover)
{
  g_assert (FOUNDRY_IS_PATH_BAR_BUTTON (self));
  g_assert (GTK_IS_POPOVER (popover));

  foundry_path_bar_button_show_popover (self);
}

static void
foundry_path_bar_button_update_label (FoundryPathBarButton *self)
{
  g_autofree char *title = NULL;
  g_autoptr(GIcon) icon = NULL;

  g_assert (FOUNDRY_IS_PATH_BAR_BUTTON (self));

  if (self->navigator != NULL)
    {
      title = foundry_path_navigator_dup_title (self->navigator);
      icon = foundry_path_navigator_dup_icon (self->navigator);
    }

  gtk_label_set_label (self->label, title);
  gtk_image_set_from_gicon (self->image, icon);
}

static void
foundry_path_bar_button_dispose (GObject *object)
{
  FoundryPathBarButton *self = (FoundryPathBarButton *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), FOUNDRY_TYPE_PATH_BAR_BUTTON);

  g_clear_object (&self->navigator);

  G_OBJECT_CLASS (foundry_path_bar_button_parent_class)->dispose (object);
}

static void
foundry_path_bar_button_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  FoundryPathBarButton *self = FOUNDRY_PATH_BAR_BUTTON (object);

  switch (prop_id)
    {
    case PROP_NAVIGATOR:
      g_value_set_object (value, foundry_path_bar_button_get_navigator (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_path_bar_button_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  FoundryPathBarButton *self = FOUNDRY_PATH_BAR_BUTTON (object);

  switch (prop_id)
    {
    case PROP_NAVIGATOR:
      foundry_path_bar_button_set_navigator (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_path_bar_button_class_init (FoundryPathBarButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = foundry_path_bar_button_dispose;
  object_class->get_property = foundry_path_bar_button_get_property;
  object_class->set_property = foundry_path_bar_button_set_property;

  properties[PROP_NAVIGATOR] =
    g_param_spec_object ("navigator", NULL, NULL,
                         FOUNDRY_TYPE_PATH_NAVIGATOR,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "foundrypathbarbutton");
  gtk_widget_class_set_template_from_resource (widget_class, "/app/devsuite/foundry-adw/ui/foundry-path-bar-button.ui");
  gtk_widget_class_bind_template_child (widget_class, FoundryPathBarButton, image);
  gtk_widget_class_bind_template_child (widget_class, FoundryPathBarButton, label);
  gtk_widget_class_bind_template_child (widget_class, FoundryPathBarButton, menu_button);
  gtk_widget_class_bind_template_child (widget_class, FoundryPathBarButton, popover);
  gtk_widget_class_bind_template_child (widget_class, FoundryPathBarButton, popover_scroller);
  gtk_widget_class_bind_template_child (widget_class, FoundryPathBarButton, list_view);
  gtk_widget_class_bind_template_child (widget_class, FoundryPathBarButton, selection);
  gtk_widget_class_bind_template_callback (widget_class, foundry_path_bar_button_activate_cb);
}

static void
foundry_path_bar_button_init (FoundryPathBarButton *self)
{
  GtkGesture *gesture;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->popover,
                           "show",
                           G_CALLBACK (foundry_path_bar_button_popover_show_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->popover,
                           "closed",
                           G_CALLBACK (foundry_path_bar_button_popover_closed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_PRIMARY);
  g_signal_connect_object (gesture,
                           "pressed",
                           G_CALLBACK (foundry_path_bar_button_button_clicked_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (GTK_WIDGET (self->menu_button), GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect_object (gesture,
                           "pressed",
                           G_CALLBACK (foundry_path_bar_button_gesture_pressed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_add_controller (GTK_WIDGET (self->menu_button), GTK_EVENT_CONTROLLER (gesture));
}

GtkWidget *
foundry_path_bar_button_new (FoundryPathNavigator *navigator)
{
  g_return_val_if_fail (FOUNDRY_IS_PATH_NAVIGATOR (navigator), NULL);

  return g_object_new (FOUNDRY_TYPE_PATH_BAR_BUTTON,
                       "navigator", navigator,
                       NULL);
}

FoundryPathNavigator *
foundry_path_bar_button_get_navigator (FoundryPathBarButton *self)
{
  g_return_val_if_fail (FOUNDRY_IS_PATH_BAR_BUTTON (self), NULL);

  return self->navigator;
}

void
foundry_path_bar_button_set_navigator (FoundryPathBarButton *self,
                                       FoundryPathNavigator *navigator)
{
  g_return_if_fail (FOUNDRY_IS_PATH_BAR_BUTTON (self));
  g_return_if_fail (!navigator || FOUNDRY_IS_PATH_NAVIGATOR (navigator));

  if (g_set_object (&self->navigator, navigator))
    {
      foundry_path_bar_button_update_label (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAVIGATOR]);
    }
}
