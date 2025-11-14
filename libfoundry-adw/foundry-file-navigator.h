/* foundry-file-navigator.h
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

#include "foundry-path-navigator.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_FILE_NAVIGATOR (foundry_file_navigator_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryFileNavigator, foundry_file_navigator, FOUNDRY, FILE_NAVIGATOR, FoundryPathNavigator)

FOUNDRY_AVAILABLE_IN_1_1
FoundryFileNavigator *foundry_file_navigator_new       (GFile                *file);
FOUNDRY_AVAILABLE_IN_1_1
GFile                 *foundry_file_navigator_dup_file (FoundryFileNavigator *self);

G_END_DECLS
