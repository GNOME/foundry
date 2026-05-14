/* foundry-future-item.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#include "foundry-future-item.h"

/**
 * FoundryFutureItem:
 *
 * A property-notifying wrapper around a [class@Dex.Future] that resolves to a
 * [class@GObject.Object].
 *
 * `FoundryFutureItem` is useful when a future-backed object needs to be
 * exposed to property expressions or bindings. When the future resolves,
 * [property@Foundry.FutureItem:item] is notified so expressions can re-read
 * the object from the future result.
 *
 * Since: 1.2
 */

struct _FoundryFutureItem
{
  GObject parent_instance;

  DexFuture *future;
  DexFuture *watch;
  guint64 serial;
};

typedef struct
{
  GWeakRef self_wr;
  guint64 serial;
} FutureWatch;

enum {
  PROP_0,
  PROP_FUTURE,
  PROP_ITEM,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryFutureItem, foundry_future_item, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
future_watch_free (FutureWatch *watch)
{
  g_assert (watch != NULL);

  g_weak_ref_clear (&watch->self_wr);
  g_free (watch);
}

static DexFuture *
future_watch_complete (DexFuture *completed,
                       gpointer   user_data)
{
  FutureWatch *watch = user_data;
  g_autoptr(FoundryFutureItem) self = NULL;

  g_assert (watch != NULL);
  g_assert (DEX_IS_FUTURE (completed));

  if (!(self = g_weak_ref_get (&watch->self_wr)))
    return dex_future_new_true ();

  if (watch->serial == self->serial)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);

  return dex_future_new_true ();
}

static void
foundry_future_item_finalize (GObject *object)
{
  FoundryFutureItem *self = (FoundryFutureItem *)object;

  self->serial = 0;

  dex_clear (&self->future);
  dex_clear (&self->watch);

  G_OBJECT_CLASS (foundry_future_item_parent_class)->finalize (object);
}

static void
foundry_future_item_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundryFutureItem *self = FOUNDRY_FUTURE_ITEM (object);

  switch (prop_id)
    {
    case PROP_FUTURE:
      dex_value_take_object (value, (DexObject *)foundry_future_item_dup_future (self));
      break;

    case PROP_ITEM:
      g_value_take_object (value, foundry_future_item_dup_item (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_future_item_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  FoundryFutureItem *self = FOUNDRY_FUTURE_ITEM (object);

  switch (prop_id)
    {
    case PROP_FUTURE:
      foundry_future_item_set_future (self, DEX_FUTURE (dex_value_get_object (value)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_future_item_class_init (FoundryFutureItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_future_item_finalize;
  object_class->get_property = foundry_future_item_get_property;
  object_class->set_property = foundry_future_item_set_property;

  /**
   * FoundryFutureItem:future:
   *
   * The future to observe for an object result.
   *
   * Since: 1.2
   */
  properties[PROP_FUTURE] =
    dex_param_spec_object ("future", NULL, NULL,
                           DEX_TYPE_FUTURE,
                           (G_PARAM_READWRITE |
                            G_PARAM_EXPLICIT_NOTIFY |
                            G_PARAM_STATIC_STRINGS));

  /**
   * FoundryFutureItem:item:
   *
   * The resolved object result from [property@Foundry.FutureItem:future].
   *
   * Since: 1.2
   */
  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_future_item_init (FoundryFutureItem *self)
{
}

/**
 * foundry_future_item_new:
 * @future: (transfer full) (nullable): a future resolving to a [class@GObject.Object]
 *
 * Creates a new [class@Foundry.FutureItem] for @future.
 *
 * This function consumes @future. @future may be %NULL. Use
 * [method@Foundry.FutureItem.set_future] to replace the future after
 * construction.
 *
 * Returns: (transfer full): a new [class@Foundry.FutureItem]
 *
 * Since: 1.2
 */
FoundryFutureItem *
foundry_future_item_new (DexFuture *future)
{
  FoundryFutureItem *self;

  g_return_val_if_fail (!future || DEX_IS_FUTURE (future), NULL);

  self = g_object_new (FOUNDRY_TYPE_FUTURE_ITEM, NULL);
  foundry_future_item_set_future (self, future);
  dex_clear (&future);

  return self;
}

/**
 * foundry_future_item_dup_future:
 * @self: a [class@Foundry.FutureItem]
 *
 * Gets the future currently observed by @self.
 *
 * Returns: (transfer full) (nullable): a [class@Dex.Future] or %NULL
 *
 * Since: 1.2
 */
DexFuture *
foundry_future_item_dup_future (FoundryFutureItem *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_FUTURE_ITEM (self));

  return self->future ? dex_ref (self->future) : NULL;
}

/**
 * foundry_future_item_set_future:
 * @self: a [class@Foundry.FutureItem]
 * @future: (transfer none) (nullable): a future resolving to a [class@GObject.Object]
 *
 * Sets the future observed by @self.
 *
 * [property@Foundry.FutureItem:item] is notified after @future is set and when
 * @future completes. If an older future completes after this call, its result is
 * ignored.
 *
 * Since: 1.2
 */
void
foundry_future_item_set_future (FoundryFutureItem *self,
                                DexFuture         *future)
{
  FutureWatch *watch;

  g_return_if_fail (FOUNDRY_IS_FUTURE_ITEM (self));
  g_return_if_fail (!future || DEX_IS_FUTURE (future));

  if (self->future == future)
    return;

  dex_clear (&self->watch);

  if (future != NULL)
    dex_ref (future);

  self->serial++;

  dex_clear (&self->future);
  self->future = future;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FUTURE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);

  if (future == NULL || !dex_future_is_pending (future))
    return;

  watch = g_new0 (FutureWatch, 1);
  g_weak_ref_init (&watch->self_wr, self);
  watch->serial = self->serial;

  self->watch = dex_future_finally (dex_ref (future),
                                    future_watch_complete,
                                    watch,
                                    (GDestroyNotify) future_watch_free);
}

/**
 * foundry_future_item_dup_item:
 * @self: a [class@Foundry.FutureItem]
 *
 * Gets the object resolved by the observed future.
 *
 * Returns: (transfer full) (nullable) (type GObject.Object): a
 *   [class@GObject.Object] or %NULL
 *
 * Since: 1.2
 */
gpointer
foundry_future_item_dup_item (FoundryFutureItem *self)
{
  const GValue *value;

  g_return_val_if_fail (FOUNDRY_IS_FUTURE_ITEM (self), NULL);

  if (self->future == NULL)
    return NULL;

  if (!(value = dex_future_get_value (self->future, NULL)))
    return NULL;

  if (!G_VALUE_HOLDS_OBJECT (value))
    return NULL;

  return g_value_dup_object (value);
}
