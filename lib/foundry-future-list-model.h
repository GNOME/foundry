/* foundry-future-list-model.h
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

#pragma once

#include <gio/gio.h>
#include <libdex.h>

#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_FUTURE_LIST_MODEL (foundry_future_list_model_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (FoundryFutureListModel, foundry_future_list_model, FOUNDRY, FUTURE_LIST_MODEL, GObject)

FOUNDRY_AVAILABLE_IN_ALL
FoundryFutureListModel *foundry_future_list_model_new   (GListModel             *model,
                                                         DexFuture              *future);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture              *foundry_future_list_model_await (FoundryFutureListModel *self)
  G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
