/* foundry-path-bar-button.h
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

#include <adwaita.h>

#include "foundry-path-navigator.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_PATH_BAR_BUTTON (foundry_path_bar_button_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryPathBarButton, foundry_path_bar_button, FOUNDRY, PATH_BAR_BUTTON, GtkWidget)

FOUNDRY_AVAILABLE_IN_1_1
GtkWidget            *foundry_path_bar_button_new           (FoundryPathNavigator *navigator);
FOUNDRY_AVAILABLE_IN_1_1
FoundryPathNavigator *foundry_path_bar_button_get_navigator (FoundryPathBarButton *self);
FOUNDRY_AVAILABLE_IN_1_1
void                  foundry_path_bar_button_set_navigator (FoundryPathBarButton *self,
                                                             FoundryPathNavigator *navigator);

G_END_DECLS
