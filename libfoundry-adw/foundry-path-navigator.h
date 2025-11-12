/* foundry-path-navigator.h
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

#include <libdex.h>

#include "foundry-contextual.h"
#include "foundry-intent.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_PATH_NAVIGATOR (foundry_path_navigator_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryPathNavigator, foundry_path_navigator, FOUNDRY, PATH_NAVIGATOR, FoundryContextual)

struct _FoundryPathNavigatorClass
{
  FoundryContextualClass parent_class;

  char          *(*dup_title)     (FoundryPathNavigator *self);
  GIcon         *(*dup_icon)      (FoundryPathNavigator *self);
  FoundryIntent *(*dup_intent)    (FoundryPathNavigator *self);
  DexFuture     *(*find_parent)   (FoundryPathNavigator *self);
  DexFuture     *(*list_children) (FoundryPathNavigator *self);
  DexFuture     *(*list_siblings) (FoundryPathNavigator *self);

  /*< private >*/
  gpointer _reserved[10];
};

FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_path_navigator_dup_title     (FoundryPathNavigator *self);
FOUNDRY_AVAILABLE_IN_1_1
GIcon     *foundry_path_navigator_dup_icon      (FoundryPathNavigator *self);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_path_navigator_find_parent   (FoundryPathNavigator *self);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_path_navigator_list_children (FoundryPathNavigator *self);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture     *foundry_path_navigator_list_siblings (FoundryPathNavigator *self);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture     *foundry_path_navigator_list_to_root  (FoundryPathNavigator *self);
FOUNDRY_AVAILABLE_IN_1_1
FoundryIntent *foundry_path_navigator_dup_intent    (FoundryPathNavigator *self);

G_END_DECLS
