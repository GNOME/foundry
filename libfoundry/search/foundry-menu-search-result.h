/* foundry-menu-search-result.h
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

#pragma once

#include <gio/gio.h>

#include "foundry-search-result.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_MENU_SEARCH_RESULT (foundry_menu_search_result_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryMenuSearchResult, foundry_menu_search_result, FOUNDRY, MENU_SEARCH_RESULT, FoundrySearchResult)

FOUNDRY_AVAILABLE_IN_1_1
FoundryMenuSearchResult *foundry_menu_search_result_new            (GMenuModel              *menu_model,
                                                                    guint                    index);
FOUNDRY_AVAILABLE_IN_1_1
GMenuModel              *foundry_menu_search_result_dup_menu_model (FoundryMenuSearchResult *self);
FOUNDRY_AVAILABLE_IN_1_1
guint                    foundry_menu_search_result_get_index      (FoundryMenuSearchResult *self);

G_END_DECLS
