/* foundry-future-item.h
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

#pragma once

#include <libdex.h>

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_FUTURE_ITEM (foundry_future_item_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_FINAL_TYPE (FoundryFutureItem, foundry_future_item, FOUNDRY, FUTURE_ITEM, GObject)

FOUNDRY_AVAILABLE_IN_1_2
FoundryFutureItem *foundry_future_item_new        (DexFuture         *future);
FOUNDRY_AVAILABLE_IN_1_2
DexFuture         *foundry_future_item_dup_future (FoundryFutureItem *self);
FOUNDRY_AVAILABLE_IN_1_2
void               foundry_future_item_set_future (FoundryFutureItem *self,
                                                   DexFuture         *future);
FOUNDRY_AVAILABLE_IN_1_2
gpointer           foundry_future_item_dup_item   (FoundryFutureItem *self);

G_END_DECLS
