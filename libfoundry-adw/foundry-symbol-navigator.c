/* foundry-symbol-navigator.c
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

#include "foundry-symbol-intent.h"
#include "foundry-symbol-navigator.h"
#include "foundry-util.h"

struct _FoundrySymbolNavigator
{
  FoundryContextual parent_instance;

  FoundrySymbol *symbol;
};

enum {
  PROP_0,
  PROP_SYMBOL,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundrySymbolNavigator, foundry_symbol_navigator, FOUNDRY_TYPE_PATH_NAVIGATOR)

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_symbol_navigator_find_parent_fiber (gpointer data)
{
  FoundrySymbolNavigator *self = data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundrySymbol) parent_symbol = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_SYMBOL_NAVIGATOR (self));

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  parent_symbol = dex_await_object (foundry_symbol_find_parent (self->symbol), &error);

  if (error)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (parent_symbol == NULL)
    return dex_future_new_take_object (NULL);

  return dex_future_new_take_object (foundry_symbol_navigator_new (context, parent_symbol));
}

static DexFuture *
foundry_symbol_navigator_find_parent (FoundryPathNavigator *navigator)
{
  FoundrySymbolNavigator *self = FOUNDRY_SYMBOL_NAVIGATOR (navigator);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_symbol_navigator_find_parent_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static DexFuture *
foundry_symbol_navigator_list_children_fiber (gpointer data)
{
  FoundrySymbolNavigator *self = data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GListModel) children_model = NULL;
  g_autoptr(GListStore) navigator_store = NULL;
  g_autoptr(GError) error = NULL;
  guint n_items;
  guint i;

  g_assert (FOUNDRY_IS_SYMBOL_NAVIGATOR (self));

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(children_model = dex_await_object (foundry_symbol_list_children (self->symbol), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  navigator_store = g_list_store_new (FOUNDRY_TYPE_SYMBOL_NAVIGATOR);
  n_items = g_list_model_get_n_items (children_model);

  for (i = 0; i < n_items; i++)
    {
      g_autoptr(FoundrySymbol) child_symbol = NULL;
      FoundrySymbolNavigator *child_navigator = NULL;

      child_symbol = g_list_model_get_item (children_model, i);
      child_navigator = foundry_symbol_navigator_new (context, child_symbol);
      g_list_store_append (navigator_store, child_navigator);
    }

  return dex_future_new_take_object (g_steal_pointer (&navigator_store));
}

static DexFuture *
foundry_symbol_navigator_list_children (FoundryPathNavigator *navigator)
{
  FoundrySymbolNavigator *self = FOUNDRY_SYMBOL_NAVIGATOR (navigator);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_symbol_navigator_list_children_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static DexFuture *
foundry_symbol_navigator_list_siblings_fiber (gpointer data)
{
  FoundrySymbolNavigator *self = data;
  g_autoptr(FoundrySymbolNavigator) parent_navigator = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GListModel) children_model = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_SYMBOL_NAVIGATOR (self));

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  parent_navigator = dex_await_object (foundry_path_navigator_find_parent (FOUNDRY_PATH_NAVIGATOR (self)), &error);

  if (error)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (parent_navigator == NULL)
    {
      g_autoptr(GListStore) store = NULL;

      store = g_list_store_new (FOUNDRY_TYPE_SYMBOL_NAVIGATOR);
      g_list_store_append (store, g_object_ref (self));
      return dex_future_new_take_object (g_steal_pointer (&store));
    }

  children_model = dex_await_object (foundry_path_navigator_list_children (FOUNDRY_PATH_NAVIGATOR (parent_navigator)), &error);

  if (error)
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&children_model));
}

static DexFuture *
foundry_symbol_navigator_list_siblings (FoundryPathNavigator *navigator)
{
  FoundrySymbolNavigator *self = FOUNDRY_SYMBOL_NAVIGATOR (navigator);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_symbol_navigator_list_siblings_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static char *
foundry_symbol_navigator_dup_title (FoundryPathNavigator *navigator)
{
  FoundrySymbolNavigator *self = FOUNDRY_SYMBOL_NAVIGATOR (navigator);

  if (self->symbol == NULL)
    return NULL;

  return foundry_symbol_dup_name (self->symbol);
}

static FoundryIntent *
foundry_symbol_navigator_dup_intent (FoundryPathNavigator *navigator)
{
  FoundrySymbolNavigator *self = FOUNDRY_SYMBOL_NAVIGATOR (navigator);
  g_autoptr(FoundrySymbolLocator) locator = NULL;
  g_autoptr(FoundryContext) context = NULL;

  if (self->symbol == NULL ||
      !(locator = foundry_symbol_dup_locator (self->symbol)) ||
      !(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))))
    return NULL;

  return foundry_symbol_intent_new (context, locator);
}

static void
foundry_symbol_navigator_dispose (GObject *object)
{
  FoundrySymbolNavigator *self = FOUNDRY_SYMBOL_NAVIGATOR (object);

  g_clear_object (&self->symbol);

  G_OBJECT_CLASS (foundry_symbol_navigator_parent_class)->dispose (object);
}

static void
foundry_symbol_navigator_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  FoundrySymbolNavigator *self = FOUNDRY_SYMBOL_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_SYMBOL:
      g_value_take_object (value, foundry_symbol_navigator_dup_symbol (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_symbol_navigator_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  FoundrySymbolNavigator *self = FOUNDRY_SYMBOL_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_SYMBOL:
      self->symbol = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_symbol_navigator_class_init (FoundrySymbolNavigatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryPathNavigatorClass *path_navigator_class = FOUNDRY_PATH_NAVIGATOR_CLASS (klass);

  object_class->dispose = foundry_symbol_navigator_dispose;
  object_class->get_property = foundry_symbol_navigator_get_property;
  object_class->set_property = foundry_symbol_navigator_set_property;

  path_navigator_class->list_siblings = foundry_symbol_navigator_list_siblings;
  path_navigator_class->find_parent = foundry_symbol_navigator_find_parent;
  path_navigator_class->list_children = foundry_symbol_navigator_list_children;
  path_navigator_class->dup_title = foundry_symbol_navigator_dup_title;
  path_navigator_class->dup_intent = foundry_symbol_navigator_dup_intent;

  properties[PROP_SYMBOL] =
    g_param_spec_object ("symbol", NULL, NULL,
                         FOUNDRY_TYPE_SYMBOL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_symbol_navigator_init (FoundrySymbolNavigator *self)
{
}

/**
 * foundry_symbol_navigator_new:
 * @context: a [class@Foundry.Context]
 * @symbol: a [class@Foundry.Symbol]
 *
 * Creates a new symbol navigator for the given symbol.
 *
 * Returns: (transfer full): a new [class@Foundry.SymbolNavigator]
 *
 * Since: 1.1
 */
FoundrySymbolNavigator *
foundry_symbol_navigator_new (FoundryContext *context,
                              FoundrySymbol  *symbol)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (FOUNDRY_IS_SYMBOL (symbol), NULL);

  return g_object_new (FOUNDRY_TYPE_SYMBOL_NAVIGATOR,
                       "context", context,
                       "symbol", symbol,
                       NULL);
}

/**
 * foundry_symbol_navigator_dup_symbol:
 * @self: a [class@Foundry.SymbolNavigator]
 *
 * Gets the symbol for this navigator.
 *
 * Returns: (transfer full): a [class@Foundry.Symbol]
 *
 * Since: 1.1
 */
FoundrySymbol *
foundry_symbol_navigator_dup_symbol (FoundrySymbolNavigator *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SYMBOL_NAVIGATOR (self), NULL);

  return g_object_ref (self->symbol);
}
