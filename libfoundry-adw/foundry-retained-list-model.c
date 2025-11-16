/* foundry-retained-list-model.c
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

#include "foundry-retained-list-model-private.h"

struct _FoundryRetainedListItem
{
  GObject parent_instance;

  GList link;

  gpointer item;
  guint hold_count;
  guint has_been_removed : 1;
};

struct _FoundryRetainedListModel
{
  GObject          parent_instance;
  GtkMapListModel *map_model;
  GPtrArray       *retained;
  GQueue           items;
};

enum {
  PROP_0,
  PROP_ITEM,
  N_PROPS
};

enum {
  RELEASED,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static void list_model_iface_init                  (GListModelInterface      *iface);
static void foundry_retained_list_item_released_cb (FoundryRetainedListModel *self,
                                                    FoundryRetainedListItem  *item);

G_DEFINE_FINAL_TYPE (FoundryRetainedListItem, foundry_retained_list_item, G_TYPE_OBJECT)
G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryRetainedListModel, foundry_retained_list_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static FoundryRetainedListItem *
foundry_retained_list_item_new (gpointer item)
{
  FoundryRetainedListItem *self;

  self = g_object_new (FOUNDRY_TYPE_RETAINED_LIST_ITEM, NULL);
  self->item = item;

  return self;
}

static gpointer
map_func (gpointer item,
          gpointer user_data)
{
  return foundry_retained_list_item_new (item);
}

static void
foundry_retained_list_item_finalize (GObject *object)
{
  FoundryRetainedListItem *self = FOUNDRY_RETAINED_LIST_ITEM (object);

  g_clear_object (&self->item);

  g_assert (self->link.data == self);
  g_assert (self->link.prev == NULL);
  g_assert (self->link.next == NULL);

  self->link.data = NULL;

  G_OBJECT_CLASS (foundry_retained_list_item_parent_class)->finalize (object);
}

static void
foundry_retained_list_item_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  FoundryRetainedListItem *self = FOUNDRY_RETAINED_LIST_ITEM (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
foundry_retained_list_item_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  FoundryRetainedListItem *self = FOUNDRY_RETAINED_LIST_ITEM (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_set_object (&self->item, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
foundry_retained_list_item_class_init (FoundryRetainedListItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_retained_list_item_finalize;
  object_class->get_property = foundry_retained_list_item_get_property;
  object_class->set_property = foundry_retained_list_item_set_property;

  properties[PROP_ITEM] =
    g_param_spec_object ("item",
                         NULL,
                         NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[RELEASED] =
    g_signal_new ("released",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void
foundry_retained_list_item_init (FoundryRetainedListItem *self)
{
  self->link.data = self;
}

gpointer
foundry_retained_list_item_get_item (FoundryRetainedListItem *self)
{
  g_return_val_if_fail (FOUNDRY_IS_RETAINED_LIST_ITEM (self), NULL);

  return self->item;
}

void
foundry_retained_list_item_hold (FoundryRetainedListItem *self)
{
  g_return_if_fail (FOUNDRY_IS_RETAINED_LIST_ITEM (self));

  self->hold_count++;
}

void
foundry_retained_list_item_release (FoundryRetainedListItem *self)
{
  g_return_if_fail (FOUNDRY_IS_RETAINED_LIST_ITEM (self));
  g_return_if_fail (self->hold_count > 0);

  self->hold_count--;

  if (self->hold_count == 0)
    g_signal_emit (self, signals[RELEASED], 0);
}

static GType
foundry_retained_list_model_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_RETAINED_LIST_ITEM;
}

static guint
foundry_retained_list_model_get_n_items (GListModel *model)
{
  FoundryRetainedListModel *self = FOUNDRY_RETAINED_LIST_MODEL (model);

  return self->items.length;
}

static gpointer
foundry_retained_list_model_get_item (GListModel *model,
                                      guint       position)
{
  FoundryRetainedListModel *self = FOUNDRY_RETAINED_LIST_MODEL (model);

  if (position >= self->items.length)
    return NULL;

  return g_object_ref (g_queue_peek_nth (&self->items, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_retained_list_model_get_item_type;
  iface->get_n_items = foundry_retained_list_model_get_n_items;
  iface->get_item = foundry_retained_list_model_get_item;
}

static void
foundry_retained_list_item_released_cb (FoundryRetainedListModel *self,
                                        FoundryRetainedListItem  *item)
{
  guint position;

  g_assert (FOUNDRY_IS_RETAINED_LIST_MODEL (self));
  g_assert (FOUNDRY_IS_RETAINED_LIST_ITEM (item));
  g_assert (item->has_been_removed);
  g_assert (!g_ptr_array_find (self->retained, item, NULL));

  position = g_queue_link_index (&self->items, &item->link);
  g_assert (position != (guint)-1);

  g_signal_handlers_disconnect_by_func (item,
                                        G_CALLBACK (foundry_retained_list_item_released_cb),
                                        self);
  g_queue_unlink (&self->items, &item->link);
  g_object_unref (item);

  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
}

static void
foundry_retained_list_model_items_changed_cb (FoundryRetainedListModel *self,
                                              guint                     position,
                                              guint                     removed,
                                              guint                     added,
                                              GListModel               *model)
{
  g_assert (FOUNDRY_IS_RETAINED_LIST_MODEL (self));
  g_assert (G_IS_LIST_MODEL (model));

  if (removed > 0)
    {
      for (guint i = position; i < position+removed; i++)
        {
          g_autoptr(FoundryRetainedListItem) item = g_object_ref (g_ptr_array_index (self->retained, i));

          item->has_been_removed = TRUE;

          g_ptr_array_remove_index (self->retained, i);

          if (item->hold_count > 0)
            g_signal_connect_object (item,
                                     "released",
                                     G_CALLBACK (foundry_retained_list_item_released_cb),
                                     self,
                                     G_CONNECT_SWAPPED);
          else
            foundry_retained_list_item_released_cb (self, item);
        }
    }

  if (added > 0)
    {
      GList *before = NULL;

      if (position > 0)
        {
          guint real_pos = 0;

          for (GList *iter = self->items.head; self->items.head; iter = iter->next)
            {
              FoundryRetainedListItem *item = iter->data;

              if (item->has_been_removed)
                continue;

              real_pos++;
              before = iter;

              if (real_pos == position)
                break;
            }
        }

      for (guint i = position; i < position+added; i++)
        {
          FoundryRetainedListItem *item = g_list_model_get_item (model, i);

          g_assert (item->link.data == item);
          g_assert (item->link.prev == NULL);
          g_assert (item->link.next == NULL);
          g_assert (item->has_been_removed == FALSE);

          g_ptr_array_insert (self->retained, position, g_object_ref (item));

          /* Try to place items _after_ removed (but pending) removals */
          while (before != NULL && before->next != NULL &&
                 ((FoundryRetainedListItem *)before->next->data)->has_been_removed)
            before = before->next;

          if (before == NULL)
            g_queue_push_head_link (&self->items, &item->link);
          else
            g_queue_insert_after_link (&self->items, before, &item->link);

          before = &item->link;

          g_list_model_items_changed (G_LIST_MODEL (self),
                                      g_queue_link_index (&self->items, &item->link),
                                      0, 1);
        }
    }
}

static void
foundry_retained_list_model_dispose (GObject *object)
{
  FoundryRetainedListModel *self = FOUNDRY_RETAINED_LIST_MODEL (object);

  g_clear_object (&self->map_model);

  if (self->retained->len > 0)
    g_ptr_array_remove_range (self->retained, 0, self->retained->len);

  while (self->items.length > 0)
    {
      FoundryRetainedListItem *item = g_queue_pop_head_link (&self->items)->data;

      if (item->has_been_removed)
        g_signal_handlers_disconnect_by_func (item,
                                              G_CALLBACK (foundry_retained_list_item_released_cb),
                                              self);

      g_object_unref (item);
    }

  G_OBJECT_CLASS (foundry_retained_list_model_parent_class)->dispose (object);
}

static void
foundry_retained_list_model_finalize (GObject *object)
{
  FoundryRetainedListModel *self = FOUNDRY_RETAINED_LIST_MODEL (object);

  g_clear_pointer (&self->retained, g_ptr_array_unref);

  g_assert (self->map_model == NULL);
  g_assert (self->items.length == 0);
  g_assert (self->items.head == NULL);
  g_assert (self->items.tail == NULL);
  g_assert (self->retained == NULL);

  G_OBJECT_CLASS (foundry_retained_list_model_parent_class)->finalize (object);
}

static void
foundry_retained_list_model_class_init (FoundryRetainedListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_retained_list_model_dispose;
  object_class->finalize = foundry_retained_list_model_finalize;
}

static void
foundry_retained_list_model_init (FoundryRetainedListModel *self)
{
  self->retained = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * foundry_retained_list_model_new:
 * @model: (transfer full): a [iface@Gio.ListModel].
 *
 * Returns: (transfer full):
 */
FoundryRetainedListModel *
foundry_retained_list_model_new (GListModel *model)
{
  FoundryRetainedListModel *self;
  guint n_items;

  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  self = g_object_new (FOUNDRY_TYPE_RETAINED_LIST_MODEL, NULL);
  self->map_model = GTK_MAP_LIST_MODEL (gtk_map_list_model_new (model, map_func, NULL, NULL));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->map_model));

  for (guint i = 0; i < n_items; i++)
    {
      FoundryRetainedListItem *item = g_list_model_get_item (G_LIST_MODEL (self->map_model), i);

      g_ptr_array_add (self->retained, g_object_ref (item));
      g_queue_push_tail_link (&self->items, &item->link);
    }

  g_signal_connect_object (G_LIST_MODEL (self->map_model),
                           "items-changed",
                           G_CALLBACK (foundry_retained_list_model_items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  return self;
}

GListModel *
foundry_retained_list_model_get_model (FoundryRetainedListModel *self)
{
  g_return_val_if_fail (FOUNDRY_IS_RETAINED_LIST_MODEL (self), NULL);

  return gtk_map_list_model_get_model (self->map_model);
}
