/* foundry-file-search-match.h
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

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_FILE_SEARCH_MATCH (foundry_file_search_match_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryFileSearchMatch, foundry_file_search_match, FOUNDRY, FILE_SEARCH_MATCH, GObject)

struct _FoundryFileSearchMatchClass
{
  GObject parent_instance;

  GFile *(*dup_file)           (FoundryFileSearchMatch *self);
  guint  (*get_line)           (FoundryFileSearchMatch *self);
  guint  (*get_line_offset)    (FoundryFileSearchMatch *self);
  guint  (*get_length)         (FoundryFileSearchMatch *self);
  char  *(*dup_before_context) (FoundryFileSearchMatch *self);
  char  *(*dup_text)           (FoundryFileSearchMatch *self);
  char  *(*dup_after_context)  (FoundryFileSearchMatch *self);

  gpointer _reserved[8];
};

FOUNDRY_AVAILABLE_IN_1_1
GFile *foundry_file_search_match_dup_file           (FoundryFileSearchMatch *self);
FOUNDRY_AVAILABLE_IN_1_1
guint  foundry_file_search_match_get_line           (FoundryFileSearchMatch *self);
FOUNDRY_AVAILABLE_IN_1_1
guint  foundry_file_search_match_get_line_offset    (FoundryFileSearchMatch *self);
FOUNDRY_AVAILABLE_IN_1_1
guint  foundry_file_search_match_get_length         (FoundryFileSearchMatch *self);
FOUNDRY_AVAILABLE_IN_1_1
char  *foundry_file_search_match_dup_before_context (FoundryFileSearchMatch *self);
FOUNDRY_AVAILABLE_IN_1_1
char  *foundry_file_search_match_dup_text           (FoundryFileSearchMatch *self);
FOUNDRY_AVAILABLE_IN_1_1
char  *foundry_file_search_match_dup_after_context  (FoundryFileSearchMatch *self);

G_END_DECLS
