/* foundry-symbol-intent.c
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

/**
 * FoundrySymbolIntent:
 *
 * Represents an intent to navigate to a symbol location.
 *
 * FoundrySymbolIntent provides functionality for navigating to a symbol
 * location using a [class@Foundry.SymbolLocator] object. It integrates with
 * the intent system to provide a unified interface for symbol navigation
 * operations across different parts of the development environment.
 *
 * Since: 1.1
 */

struct _FoundrySymbolIntent
{
  FoundryIntent         parent_instance;
  FoundrySymbolLocator *locator;
};

enum {
  PROP_0,
  PROP_LOCATOR,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundrySymbolIntent, foundry_symbol_intent, FOUNDRY_TYPE_INTENT)

static GParamSpec *properties[N_PROPS];

static void
foundry_symbol_intent_finalize (GObject *object)
{
  FoundrySymbolIntent *self = FOUNDRY_SYMBOL_INTENT (object);

  g_clear_object (&self->locator);

  G_OBJECT_CLASS (foundry_symbol_intent_parent_class)->finalize (object);
}

static void
foundry_symbol_intent_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundrySymbolIntent *self = FOUNDRY_SYMBOL_INTENT (object);

  switch (prop_id)
    {
    case PROP_LOCATOR:
      g_value_take_object (value, foundry_symbol_intent_dup_locator (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_symbol_intent_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  FoundrySymbolIntent *self = FOUNDRY_SYMBOL_INTENT (object);

  switch (prop_id)
    {
    case PROP_LOCATOR:
      self->locator = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_symbol_intent_class_init (FoundrySymbolIntentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_symbol_intent_finalize;
  object_class->get_property = foundry_symbol_intent_get_property;
  object_class->set_property = foundry_symbol_intent_set_property;

  properties[PROP_LOCATOR] =
    g_param_spec_object ("locator", NULL, NULL,
                         FOUNDRY_TYPE_SYMBOL_LOCATOR,
                         (G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_symbol_intent_init (FoundrySymbolIntent *self)
{
}

/**
 * foundry_symbol_intent_new:
 * @context: (nullable): a [class@Foundry.Context]
 * @locator: a [class@Foundry.SymbolLocator]
 *
 * Creates a new [class@Foundry.SymbolIntent] for navigating to the
 * symbol location specified by @locator.
 *
 * Returns: (transfer full): a new [class@Foundry.SymbolIntent]
 *
 * Since: 1.1
 */
FoundryIntent *
foundry_symbol_intent_new (FoundryContext       *context,
                           FoundrySymbolLocator *locator)
{
  g_return_val_if_fail (!context || FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (FOUNDRY_IS_SYMBOL_LOCATOR (locator), NULL);

  return g_object_new (FOUNDRY_TYPE_SYMBOL_INTENT,
                       "context", context,
                       "locator", locator,
                       NULL);
}

/**
 * foundry_symbol_intent_dup_locator:
 * @self: a [class@Foundry.SymbolIntent]
 *
 * Gets a copy of the symbol locator object.
 *
 * Returns: (transfer full) (not nullable): a [class@Foundry.SymbolLocator]
 *
 * Since: 1.1
 */
FoundrySymbolLocator *
foundry_symbol_intent_dup_locator (FoundrySymbolIntent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SYMBOL_INTENT (self), NULL);

  return g_object_ref (self->locator);
}
