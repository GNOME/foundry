/* foundry-operation-button.c
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

#include "foundry-operation-button-private.h"
#include "foundry-progress-icon-private.h"

struct _FoundryOperationButton
{
  GtkWidget parent_instance;

  FoundryRetainedListItem *item;
  FoundryOperation *operation;
  GtkMenuButton *menu_button;
  GtkPopover *popover;
  GtkStack *stack;
  GtkLabel *title_label;
  GtkLabel *popover_title_label;
  GtkLabel *subtitle_label;
  GtkProgressBar *progress_bar;
  GtkButton *cancel_button;
  FoundryProgressIcon *progress_icon;
  guint is_completed : 1;
};

G_DEFINE_FINAL_TYPE (FoundryOperationButton, foundry_operation_button, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_ITEM,
  PROP_OPERATION,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_operation_button_update_stack (FoundryOperationButton *self)
{
  double progress;
  gboolean cancelled;

  g_assert (FOUNDRY_IS_OPERATION_BUTTON (self));

  if (!self->operation)
    {
      gtk_stack_set_visible_child_name (self->stack, "spinning");
      return;
    }

  progress = foundry_operation_get_progress (self->operation);
  cancelled = foundry_operation_is_cancelled (self->operation);

  if (cancelled)
    {
      gtk_stack_set_visible_child_name (self->stack, "cancelled");
      gtk_widget_set_sensitive (GTK_WIDGET (self->cancel_button), FALSE);
    }
  else if (self->is_completed)
    {
      gtk_stack_set_visible_child_name (self->stack, "complete");
    }
  else if (progress > 0.0)
    {
      gtk_stack_set_visible_child_name (self->stack, "progress");
      foundry_progress_icon_set_progress (self->progress_icon, progress);
    }
  else
    {
      gtk_stack_set_visible_child_name (self->stack, "spinning");
    }
}

static void
foundry_operation_button_cancelled_changed (FoundryOperationButton *self)
{
  g_assert (FOUNDRY_IS_OPERATION_BUTTON (self));

  foundry_operation_button_update_stack (self);

  if (foundry_operation_is_cancelled (self->operation))
    {
      gtk_label_set_text (self->subtitle_label, "Cancelled");
    }
}

static void
foundry_operation_button_completed (FoundryOperationButton *self)
{
  g_assert (FOUNDRY_IS_OPERATION_BUTTON (self));

  self->is_completed = TRUE;
  gtk_widget_set_sensitive (GTK_WIDGET (self->cancel_button), FALSE);
  foundry_operation_button_update_stack (self);
}

static void
foundry_operation_button_progress_changed (FoundryOperationButton *self)
{
  double progress;

  g_assert (FOUNDRY_IS_OPERATION_BUTTON (self));

  if (!self->operation)
    return;

  progress = foundry_operation_get_progress (self->operation);
  gtk_progress_bar_set_fraction (self->progress_bar, progress);
  foundry_progress_icon_set_progress (self->progress_icon, progress);

  foundry_operation_button_update_stack (self);
}

static void
foundry_operation_button_title_changed (FoundryOperationButton *self)
{
  g_autofree char *title = NULL;

  g_assert (FOUNDRY_IS_OPERATION_BUTTON (self));

  if (!self->operation)
    return;

  title = foundry_operation_dup_title (self->operation);
  gtk_label_set_text (self->title_label, title);
  gtk_label_set_text (self->popover_title_label, title);
}

static void
foundry_operation_button_subtitle_changed (FoundryOperationButton *self)
{
  g_autofree char *subtitle = NULL;

  g_assert (FOUNDRY_IS_OPERATION_BUTTON (self));

  if (!self->operation)
    return;

  subtitle = foundry_operation_dup_subtitle (self->operation);
  gtk_label_set_text (self->subtitle_label, subtitle ? subtitle : "");
}

static void
foundry_operation_button_cancel_clicked (FoundryOperationButton *self,
                                         GtkButton             *button)
{
  g_assert (FOUNDRY_IS_OPERATION_BUTTON (self));
  g_assert (GTK_IS_BUTTON (button));

  if (self->operation)
    {
      foundry_operation_cancel (self->operation);
      gtk_widget_set_sensitive (GTK_WIDGET (self->cancel_button), FALSE);
    }
}

static void
foundry_operation_button_connect_signals (FoundryOperationButton *self)
{
  g_assert (FOUNDRY_IS_OPERATION_BUTTON (self));
  g_assert (FOUNDRY_IS_OPERATION (self->operation));

  g_signal_connect_object (self->operation,
                          "notify::cancelled",
                          G_CALLBACK (foundry_operation_button_cancelled_changed),
                          self,
                          G_CONNECT_SWAPPED);
  g_signal_connect_object (self->operation,
                          "completed",
                          G_CALLBACK (foundry_operation_button_completed),
                          self,
                          G_CONNECT_SWAPPED);
  g_signal_connect_object (self->operation,
                          "notify::progress",
                          G_CALLBACK (foundry_operation_button_progress_changed),
                          self,
                          G_CONNECT_SWAPPED);
  g_signal_connect_object (self->operation,
                          "notify::title",
                          G_CALLBACK (foundry_operation_button_title_changed),
                          self,
                          G_CONNECT_SWAPPED);
  g_signal_connect_object (self->operation,
                          "notify::subtitle",
                          G_CALLBACK (foundry_operation_button_subtitle_changed),
                          self,
                          G_CONNECT_SWAPPED);
}

static void
foundry_operation_button_dispose (GObject *object)
{
  FoundryOperationButton *self = FOUNDRY_OPERATION_BUTTON (object);

  g_clear_object (&self->progress_icon);
  g_clear_object (&self->operation);
  g_clear_object (&self->item);

  gtk_widget_dispose_template (GTK_WIDGET (object), FOUNDRY_TYPE_OPERATION_BUTTON);

  G_OBJECT_CLASS (foundry_operation_button_parent_class)->dispose (object);
}

static void
foundry_operation_button_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  FoundryOperationButton *self = FOUNDRY_OPERATION_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    case PROP_OPERATION:
      g_value_set_object (value, self->operation);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_operation_button_popover_show (FoundryOperationButton *self)
{
  g_assert (FOUNDRY_IS_OPERATION_BUTTON (self));

  if (self->item)
    foundry_retained_list_item_hold (self->item);
}

static void
foundry_operation_button_popover_close (FoundryOperationButton *self)
{
  g_assert (FOUNDRY_IS_OPERATION_BUTTON (self));

  if (self->item)
    foundry_retained_list_item_release (self->item);
}

static void
foundry_operation_button_update_operation (FoundryOperationButton *self)
{
  FoundryOperation *operation = NULL;

  g_assert (FOUNDRY_IS_OPERATION_BUTTON (self));

  if (self->item)
    operation = FOUNDRY_OPERATION (foundry_retained_list_item_get_item (self->item));

  if (g_set_object (&self->operation, operation))
    {
      if (operation)
        {
          self->is_completed = FALSE;
          foundry_operation_button_connect_signals (self);
          foundry_operation_button_title_changed (self);
          foundry_operation_button_subtitle_changed (self);
          foundry_operation_button_progress_changed (self);
          foundry_operation_button_cancelled_changed (self);
          gtk_widget_set_sensitive (GTK_WIDGET (self->cancel_button), TRUE);
        }
      else
        {
          self->is_completed = FALSE;
          gtk_label_set_text (self->title_label, "");
          gtk_label_set_text (self->popover_title_label, "");
          gtk_label_set_text (self->subtitle_label, "");
          gtk_progress_bar_set_fraction (self->progress_bar, 0.0);
          foundry_progress_icon_set_progress (self->progress_icon, 0.0);
          foundry_operation_button_update_stack (self);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OPERATION]);
    }
}

static void
foundry_operation_button_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  FoundryOperationButton *self = FOUNDRY_OPERATION_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_assert (self->item == NULL);
      self->item = g_value_dup_object (value);
      foundry_operation_button_update_operation (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
      break;

    case PROP_OPERATION:
      /* Read-only */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_operation_button_class_init (FoundryOperationButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = foundry_operation_button_dispose;
  object_class->get_property = foundry_operation_button_get_property;
  object_class->set_property = foundry_operation_button_set_property;

  /**
   * FoundryOperationButton:item:
   *
   * The #FoundryRetainedListItem containing the operation.
   */
  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                        FOUNDRY_TYPE_RETAINED_LIST_ITEM,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * FoundryOperationButton:operation:
   *
   * The #FoundryOperation to display (read-only, derived from item).
   */
  properties[PROP_OPERATION] =
    g_param_spec_object ("operation", NULL, NULL,
                        FOUNDRY_TYPE_OPERATION,
                        (G_PARAM_READABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/foundry-adw/ui/foundry-operation-button.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_bind_template_child (widget_class, FoundryOperationButton, menu_button);
  gtk_widget_class_bind_template_child (widget_class, FoundryOperationButton, popover);
  gtk_widget_class_bind_template_child (widget_class, FoundryOperationButton, stack);
  gtk_widget_class_bind_template_child (widget_class, FoundryOperationButton, title_label);
  gtk_widget_class_bind_template_child (widget_class, FoundryOperationButton, popover_title_label);
  gtk_widget_class_bind_template_child (widget_class, FoundryOperationButton, subtitle_label);
  gtk_widget_class_bind_template_child (widget_class, FoundryOperationButton, progress_bar);
  gtk_widget_class_bind_template_child (widget_class, FoundryOperationButton, cancel_button);
  gtk_widget_class_bind_template_callback (widget_class, foundry_operation_button_cancel_clicked);
  gtk_widget_class_bind_template_callback (widget_class, foundry_operation_button_popover_show);
  gtk_widget_class_bind_template_callback (widget_class, foundry_operation_button_popover_close);
}

static void
foundry_operation_button_init (FoundryOperationButton *self)
{
  GtkWidget *picture;
  GtkStackPage *page;

  self->progress_icon = foundry_progress_icon_new ();

  gtk_widget_init_template (GTK_WIDGET (self));

  picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (self->progress_icon));
  gtk_widget_set_size_request (picture, 16, 16);
  page = gtk_stack_add_child (self->stack, picture);
  gtk_stack_page_set_name (page, "progress");
}

GtkWidget *
foundry_operation_button_new (FoundryRetainedListItem *item)
{
  g_return_val_if_fail (FOUNDRY_IS_RETAINED_LIST_ITEM (item), NULL);

  return g_object_new (FOUNDRY_TYPE_OPERATION_BUTTON,
                       "item", item,
                       NULL);
}
