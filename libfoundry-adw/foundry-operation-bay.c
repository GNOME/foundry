/* foundry-operation-bay.c
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

#include "foundry-operation-bay-private.h"
#include "foundry-operation-button-private.h"
#include "foundry-retained-list-model-private.h"

struct _FoundryOperationBay
{
  GtkWidget                 parent_instance;

  FoundryContext           *context;
  FoundryOperationManager  *operation_manager;
  FoundryRetainedListModel *retained_model;

  GtkBox                   *vbox;
  GtkRevealer              *revealer;

  gulong                    items_changed_id;
};

G_DEFINE_FINAL_TYPE (FoundryOperationBay, foundry_operation_bay, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean
uint_to_boolean (GBinding     *binding,
                 const GValue *from_value,
                 GValue       *to_value,
                 gpointer      user_data)
{
  guint n_items = g_value_get_uint (from_value);

  g_value_set_boolean (to_value, n_items > 0);

  return TRUE;
}

static void
foundry_operation_bay_dispose (GObject *object)
{
  FoundryOperationBay *self = FOUNDRY_OPERATION_BAY (object);
  GtkWidget *child;

  g_clear_signal_handler (&self->items_changed_id, self->retained_model);

  g_clear_object (&self->retained_model);
  g_clear_object (&self->operation_manager);
  g_clear_object (&self->context);

  gtk_widget_dispose_template (GTK_WIDGET (object), FOUNDRY_TYPE_OPERATION_BAY);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (foundry_operation_bay_parent_class)->dispose (object);
}

static void
foundry_operation_bay_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundryOperationBay *self = FOUNDRY_OPERATION_BAY (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_operation_bay_items_changed (FoundryOperationBay *self,
                                     guint                position,
                                     guint                removed,
                                     guint                added,
                                     GListModel          *model)
{
  GtkWidget *child;

  g_assert (FOUNDRY_IS_OPERATION_BAY (self));
  g_assert (G_IS_LIST_MODEL (model));

  if (removed > 0)
    {
      child = gtk_widget_get_first_child (GTK_WIDGET (self->vbox));
      for (guint j = 0; j < position; j++)
        child = gtk_widget_get_next_sibling (child);

      for (guint i = 0; i < removed && child; i++)
        {
          GtkWidget *next = gtk_widget_get_next_sibling (child);
          gtk_box_remove (self->vbox, child);
          child = next;
        }
    }

  if (added > 0)
    {
      GtkWidget *sibling = NULL;

      if (position > 0)
        {
          sibling = gtk_widget_get_first_child (GTK_WIDGET (self->vbox));

          for (guint j = 1; j < position; j++)
            sibling = gtk_widget_get_next_sibling (sibling);
        }

      for (guint i = 0; i < added; i++)
        {
          g_autoptr(FoundryRetainedListItem) item = NULL;
          GtkWidget *button;

          item = g_list_model_get_item (model, position + i);
          button = foundry_operation_button_new (item);
          gtk_box_insert_child_after (self->vbox, button, sibling);

          sibling = button;
        }
    }
}

static void
foundry_operation_bay_set_context (FoundryOperationBay *self,
                                   FoundryContext      *context)
{
  g_assert (FOUNDRY_IS_OPERATION_BAY (self));
  g_assert (!context || FOUNDRY_IS_CONTEXT (context));

  if (g_set_object (&self->context, context))
    {
      g_clear_signal_handler (&self->items_changed_id, self->retained_model);

      g_clear_object (&self->retained_model);
      g_clear_object (&self->operation_manager);

      if (context != NULL)
        {
          GListModel *model;

          self->operation_manager = foundry_context_dup_operation_manager (context);
          model = G_LIST_MODEL (self->operation_manager);
          self->retained_model = foundry_retained_list_model_new (model);
          self->items_changed_id =
            g_signal_connect_object (self->retained_model,
                                     "items-changed",
                                     G_CALLBACK (foundry_operation_bay_items_changed),
                                     self,
                                     G_CONNECT_SWAPPED);

          g_object_bind_property_full (self->retained_model,
                                       "n-items",
                                       self->revealer,
                                       "reveal-child",
                                       G_BINDING_SYNC_CREATE,
                                       uint_to_boolean,
                                       NULL,
                                       NULL,
                                       NULL);

          {
            guint n_items = g_list_model_get_n_items (G_LIST_MODEL (self->retained_model));

            for (guint i = 0; i < n_items; i++)
              {
                g_autoptr(FoundryRetainedListItem) item = NULL;
                GtkWidget *button;

                item = g_list_model_get_item (G_LIST_MODEL (self->retained_model), i);
                button = foundry_operation_button_new (item);
                gtk_box_append (self->vbox, button);
              }
          }
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTEXT]);
    }
}

static void
foundry_operation_bay_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  FoundryOperationBay *self = FOUNDRY_OPERATION_BAY (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      foundry_operation_bay_set_context (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_operation_bay_class_init (FoundryOperationBayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = foundry_operation_bay_dispose;
  object_class->get_property = foundry_operation_bay_get_property;
  object_class->set_property = foundry_operation_bay_set_property;

  /**
   * FoundryOperationBay:context:
   *
   * The #FoundryContext to monitor for operations.
   */
  properties[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                        FOUNDRY_TYPE_CONTEXT,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/foundry-adw/ui/foundry-operation-bay.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_bind_template_child (widget_class, FoundryOperationBay, vbox);
  gtk_widget_class_bind_template_child (widget_class, FoundryOperationBay, revealer);
}

static void
foundry_operation_bay_init (FoundryOperationBay *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
