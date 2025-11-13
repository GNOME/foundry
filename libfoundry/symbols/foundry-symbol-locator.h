/* foundry-symbol-locator.h
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

#include <gio/gio.h>

#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_SYMBOL_LOCATOR (foundry_symbol_locator_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundrySymbolLocator, foundry_symbol_locator, FOUNDRY, SYMBOL_LOCATOR, GObject)

FOUNDRY_AVAILABLE_IN_1_1
FoundrySymbolLocator *foundry_symbol_locator_new_for_file                 (GFile                *file);
FOUNDRY_AVAILABLE_IN_1_1
FoundrySymbolLocator *foundry_symbol_locator_new_for_file_and_line        (GFile                *file,
                                                                           guint                 line);
FOUNDRY_AVAILABLE_IN_1_1
FoundrySymbolLocator *foundry_symbol_locator_new_for_file_and_line_offset (GFile                *file,
                                                                           guint                 line,
                                                                           guint                 line_offset);
FOUNDRY_AVAILABLE_IN_1_1
FoundrySymbolLocator *foundry_symbol_locator_new_for_file_and_pattern     (GFile                *file,
                                                                           const char           *pattern);
FOUNDRY_AVAILABLE_IN_1_1
const char           *foundry_symbol_locator_get_pattern                  (FoundrySymbolLocator *self);
FOUNDRY_AVAILABLE_IN_1_1
FoundrySymbolLocator *foundry_symbol_locator_locate                       (FoundrySymbolLocator *self,
                                                                           GBytes               *contents);
FOUNDRY_AVAILABLE_IN_1_1
GFile                *foundry_symbol_locator_dup_file                     (FoundrySymbolLocator *self);
FOUNDRY_AVAILABLE_IN_1_1
gboolean              foundry_symbol_locator_get_line_set                 (FoundrySymbolLocator *self);
FOUNDRY_AVAILABLE_IN_1_1
guint                 foundry_symbol_locator_get_line                     (FoundrySymbolLocator *self);
FOUNDRY_AVAILABLE_IN_1_1
gboolean              foundry_symbol_locator_get_line_offset_set          (FoundrySymbolLocator *self);
FOUNDRY_AVAILABLE_IN_1_1
guint                 foundry_symbol_locator_get_line_offset              (FoundrySymbolLocator *self);

G_END_DECLS
