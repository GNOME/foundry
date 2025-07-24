/* foundry-symbol.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "foundry-symbol.h"
#include "foundry-util.h"

enum {
  PROP_0,
  PROP_NAME,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundrySymbol, foundry_symbol, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_symbol_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  FoundrySymbol *self = FOUNDRY_SYMBOL (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_take_string (value, foundry_symbol_dup_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_symbol_class_init (FoundrySymbolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_symbol_get_property;

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_symbol_init (FoundrySymbol *self)
{
}

/**
 * foundry_symbol_dup_name:
 * @self: a #FoundrySymbol
 *
 * Gets the name of the symbol.
 *
 * Returns: (transfer full) (nullable): a string or %NULL
 */
char *
foundry_symbol_dup_name (FoundrySymbol *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SYMBOL (self), NULL);

  return FOUNDRY_SYMBOL_GET_CLASS (self)->dup_name (self);
}

/**
 * foundry_symbol_list_children:
 * @self: a [class@Foundry.Symbol]
 *
 * List all of the children of this symbol.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *  [iface@Gio.ListModel] of [class@Foundry.Symbol] or rejects with error.
 */
DexFuture *
foundry_symbol_list_children (FoundrySymbol *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_SYMBOL (self));

  if (FOUNDRY_SYMBOL_GET_CLASS (self)->list_children)
    return FOUNDRY_SYMBOL_GET_CLASS (self)->list_children (self);

  return foundry_future_new_not_supported ();
}

/**
 * foundry_symbol_find_parent:
 * @self: a [class@Foundry.Symbol]
 *
 * Find the parent symbol, if any.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *  a [class@Foundry.Symbol] or rejects with error.
 */
DexFuture *
foundry_symbol_find_parent (FoundrySymbol *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_SYMBOL (self));

  if (FOUNDRY_SYMBOL_GET_CLASS (self)->find_parent)
    return FOUNDRY_SYMBOL_GET_CLASS (self)->find_parent (self);

  return foundry_future_new_not_supported ();
}
