/* foundry-intent-manager.h
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

#pragma once

#include "foundry-service.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_INTENT_MANAGER (foundry_intent_manager_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryIntentManager, foundry_intent_manager, FOUNDRY, INTENT_MANAGER, FoundryService)

FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_intent_manager_dispatch (FoundryIntentManager *self,
                                            FoundryIntent        *intent);

G_END_DECLS
