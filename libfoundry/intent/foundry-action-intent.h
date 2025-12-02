/* foundry-action-intent.h
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

#include <glib-object.h>

#include "foundry-intent.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_ACTION_INTENT (foundry_action_intent_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryActionIntent, foundry_action_intent, FOUNDRY, ACTION_INTENT, FoundryIntent)

FOUNDRY_AVAILABLE_IN_1_1
FoundryIntent *foundry_action_intent_new               (const char          *action_name,
                                                        GVariant            *action_target);
FOUNDRY_AVAILABLE_IN_1_1
char          *foundry_action_intent_dup_action_name   (FoundryActionIntent *self);
FOUNDRY_AVAILABLE_IN_1_1
GVariant      *foundry_action_intent_dup_action_target (FoundryActionIntent *self);

G_END_DECLS
