/* foundry-path-bar.c
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

#include "foundry-path-bar.h"
#include "foundry-path-bar-button-private.h"
#include "foundry-util.h"

struct _FoundryPathBar
{
  GtkWidget parent_instance;

  FoundryPathNavigator *selected_item;
  FoundryPathNavigator *root;
  GListModel *path_model;

  GtkBox *outer_box;
  GtkBox *box;
  GtkWidget *root_button;
  GtkScrolledWindow *scroller;

  guint stamp;
};

enum {
  PROP_0,
  PROP_ROOT,
  PROP_SELECTED_ITEM,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryPathBar, foundry_path_bar, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS];

static void
foundry_path_bar_clear_buttons (FoundryPathBar *self)
{
  GtkWidget *child;

  g_assert (FOUNDRY_IS_PATH_BAR (self));

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->box))))
    gtk_widget_unparent (child);
}

static void
foundry_path_bar_update_root_button (FoundryPathBar *self)
{
  g_assert (FOUNDRY_IS_PATH_BAR (self));

  if (self->root_button != NULL)
    {
      gtk_box_remove (self->outer_box, self->root_button);
      self->root_button = NULL;
    }

  if (self->root != NULL)
    {
      self->root_button = foundry_path_bar_button_new (self->root);
      gtk_widget_add_css_class (self->root_button, "root-navigator");
      gtk_box_prepend (self->outer_box, self->root_button);
    }
}

static void
foundry_path_bar_populate_buttons (FoundryPathBar *self)
{
  guint n_items;

  g_assert (FOUNDRY_IS_PATH_BAR (self));
  g_assert (G_IS_LIST_MODEL (self->path_model));

  n_items = g_list_model_get_n_items (self->path_model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryPathNavigator) navigator = NULL;
      GtkWidget *button;

      navigator = g_list_model_get_item (self->path_model, i);
      button = foundry_path_bar_button_new (navigator);

      gtk_box_append (self->box, button);
    }
}

static DexFuture *
foundry_path_bar_update_model_fiber (FoundryPathBar       *self,
                                     FoundryPathNavigator *navigator,
                                     guint                 stamp)
{
  g_autoptr(GListModel) model = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_PATH_BAR (self));
  g_assert (!navigator || FOUNDRY_IS_PATH_NAVIGATOR (navigator));

  if (!(model = dex_await_object (foundry_path_navigator_list_to_root (navigator), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (stamp != self->stamp)
    return NULL;

  if (g_set_object (&self->path_model, model))
    foundry_path_bar_populate_buttons (self);

  return dex_future_new_true ();
}

static void
foundry_path_bar_update_model (FoundryPathBar *self)
{
  g_assert (FOUNDRY_IS_PATH_BAR (self));

  foundry_path_bar_clear_buttons (self);
  g_clear_object (&self->path_model);

  if (self->selected_item == NULL)
    return;

  dex_future_disown (foundry_scheduler_spawn (NULL, 0,
                                              G_CALLBACK (foundry_path_bar_update_model_fiber),
                                              3,
                                              FOUNDRY_TYPE_PATH_BAR, self,
                                              FOUNDRY_TYPE_PATH_NAVIGATOR, self->selected_item,
                                              G_TYPE_UINT, ++self->stamp));
}

static void
foundry_path_bar_dispose (GObject *object)
{
  FoundryPathBar *self = (FoundryPathBar *)object;

  foundry_path_bar_clear_buttons (self);

  gtk_widget_dispose_template (GTK_WIDGET (self), FOUNDRY_TYPE_PATH_BAR);

  g_clear_object (&self->selected_item);
  g_clear_object (&self->root);
  g_clear_object (&self->path_model);

  G_OBJECT_CLASS (foundry_path_bar_parent_class)->dispose (object);
}

static void
foundry_path_bar_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  FoundryPathBar *self = FOUNDRY_PATH_BAR (object);

  switch (prop_id)
    {
    case PROP_SELECTED_ITEM:
      g_value_set_object (value, foundry_path_bar_get_selected_item (self));
      break;

    case PROP_ROOT:
      g_value_take_object (value, foundry_path_bar_dup_root (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_path_bar_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  FoundryPathBar *self = FOUNDRY_PATH_BAR (object);

  switch (prop_id)
    {
    case PROP_SELECTED_ITEM:
      foundry_path_bar_set_selected_item (self, g_value_get_object (value));
      break;

    case PROP_ROOT:
      foundry_path_bar_set_root (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_path_bar_class_init (FoundryPathBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = foundry_path_bar_dispose;
  object_class->get_property = foundry_path_bar_get_property;
  object_class->set_property = foundry_path_bar_set_property;

  properties[PROP_SELECTED_ITEM] =
    g_param_spec_object ("selected-item", NULL, NULL,
                         FOUNDRY_TYPE_PATH_NAVIGATOR,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ROOT] =
    g_param_spec_object ("root", NULL, NULL,
                         FOUNDRY_TYPE_PATH_NAVIGATOR,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "foundrypathbar");
  gtk_widget_class_set_template_from_resource (widget_class, "/app/devsuite/foundry-adw/ui/foundry-path-bar.ui");
  gtk_widget_class_bind_template_child (widget_class, FoundryPathBar, scroller);
  gtk_widget_class_bind_template_child (widget_class, FoundryPathBar, outer_box);
  gtk_widget_class_bind_template_child (widget_class, FoundryPathBar, box);
}

static void
foundry_path_bar_init (FoundryPathBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
foundry_path_bar_new (void)
{
  return g_object_new (FOUNDRY_TYPE_PATH_BAR, NULL);
}

/**
 * foundry_path_bar_get_selected_item:
 * @self: a [class@FoundryAdw.PathBar]
 *
 * Returns: (transfer none) (nullable):
 *
 * Since: 1.1
 */
FoundryPathNavigator *
foundry_path_bar_get_selected_item (FoundryPathBar *self)
{
  g_return_val_if_fail (FOUNDRY_IS_PATH_BAR (self), NULL);

  return self->selected_item;
}

void
foundry_path_bar_set_selected_item (FoundryPathBar       *self,
                                    FoundryPathNavigator *selected_item)
{
  g_return_if_fail (FOUNDRY_IS_PATH_BAR (self));
  g_return_if_fail (!selected_item || FOUNDRY_IS_PATH_NAVIGATOR (selected_item));

  if (g_set_object (&self->selected_item, selected_item))
    {
      foundry_path_bar_update_model (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED_ITEM]);
    }
}

/**
 * foundry_path_bar_dup_root:
 * @self: a [class@FoundryAdw.PathBar]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
FoundryPathNavigator *
foundry_path_bar_dup_root (FoundryPathBar *self)
{
  g_return_val_if_fail (FOUNDRY_IS_PATH_BAR (self), NULL);

  return self->root ? g_object_ref (self->root) : NULL;
}

/**
 * foundry_path_bar_set_root:
 * @self: a [class@FoundryAdw.PathBar]
 * @root: (nullable): a root navigator
 *
 * Set a root navigator that will always be shown regardless of the
 * selected-item in the pathbar. This is useful for synthesized roots
 * that you always want to show.
 *
 * Since: 1.1
 */
void
foundry_path_bar_set_root (FoundryPathBar       *self,
                           FoundryPathNavigator *root)
{
  g_return_if_fail (FOUNDRY_IS_PATH_BAR (self));
  g_return_if_fail (!root || FOUNDRY_IS_PATH_NAVIGATOR (root));

  if (g_set_object (&self->root, root))
    {
      foundry_path_bar_update_root_button (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ROOT]);
    }
}
