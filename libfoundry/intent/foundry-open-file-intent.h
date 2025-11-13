/* foundry-open-file-intent.h
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

#include "foundry-intent.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_OPEN_FILE_INTENT (foundry_open_file_intent_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryOpenFileIntent, foundry_open_file_intent, FOUNDRY, OPEN_FILE_INTENT, FoundryIntent)

FOUNDRY_AVAILABLE_IN_1_1
FoundryIntent *foundry_open_file_intent_new              (GFile                 *file,
                                                          const char            *content_type);
FOUNDRY_AVAILABLE_IN_1_1
GFile         *foundry_open_file_intent_dup_file         (FoundryOpenFileIntent *self);
FOUNDRY_AVAILABLE_IN_1_1
char          *foundry_open_file_intent_dup_content_type (FoundryOpenFileIntent *self);

G_END_DECLS
