/* foundry-intent-handler.c
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

#include "foundry-intent-handler.h"
#include "foundry-intent.h"
#include "foundry-util.h"

/**
 * FoundryIntentHandler:
 *
 * Abstract base class for handling intents.
 *
 * FoundryIntentHandler provides the core interface for processing intents
 * and handling specific actions within the development environment. Concrete
 * implementations handle specific intent types and provide specialized
 * functionality for different development workflows and user interactions.
 */

G_DEFINE_ABSTRACT_TYPE (FoundryIntentHandler, foundry_intent_handler, FOUNDRY_TYPE_CONTEXTUAL)

static void
foundry_intent_handler_class_init (FoundryIntentHandlerClass *klass)
{
}

static void
foundry_intent_handler_init (FoundryIntentHandler *self)
{
}

/**
 * foundry_intent_handler_dispatch:
 * @self: a [class@Foundry.IntentHandler]
 *
 * Dispatches the intent.
 *
 * If this handler cannot handle this intent type it just reject
 * with `G_IO_ERROR` and `G_IO_ERROR_NOT_SUPPORTED`.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any
 *   value if successful; otherwise rejects with error.
 */
DexFuture *
foundry_intent_handler_dispatch (FoundryIntentHandler *self,
                                 FoundryIntent        *intent)
{
  dex_return_error_if_fail (FOUNDRY_IS_INTENT_HANDLER (self));
  dex_return_error_if_fail (FOUNDRY_IS_INTENT (intent));

  if (FOUNDRY_INTENT_HANDLER_GET_CLASS (self)->dispatch)
    return FOUNDRY_INTENT_HANDLER_GET_CLASS (self)->dispatch (self, intent);

  return foundry_future_new_not_supported ();
}
