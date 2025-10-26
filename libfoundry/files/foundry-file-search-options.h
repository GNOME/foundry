/* foundry-file-search-options.h
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

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_FILE_SEARCH_OPTIONS (foundry_file_search_options_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryFileSearchOptions, foundry_file_search_options, FOUNDRY, FILE_SEARCH_OPTIONS, GObject)

FOUNDRY_AVAILABLE_IN_1_1
FoundryFileSearchOptions  *foundry_file_search_options_new                   (void);
FOUNDRY_AVAILABLE_IN_1_1
void                       foundry_file_search_options_add_target            (FoundryFileSearchOptions *self,
                                                                              GFile                    *file);
FOUNDRY_AVAILABLE_IN_1_1
void                       foundry_file_search_options_remove_target         (FoundryFileSearchOptions *self,
                                                                              GFile                    *file);
FOUNDRY_AVAILABLE_IN_1_1
GListModel                *foundry_file_search_options_list_targets          (FoundryFileSearchOptions *self);
FOUNDRY_AVAILABLE_IN_1_1
char                      *foundry_file_search_options_dup_search_text       (FoundryFileSearchOptions *self);
FOUNDRY_AVAILABLE_IN_1_1
void                       foundry_file_search_options_set_search_text       (FoundryFileSearchOptions *self,
                                                                              const char               *search_text);
FOUNDRY_AVAILABLE_IN_1_1
gboolean                   foundry_file_search_options_get_recursive         (FoundryFileSearchOptions *self);
FOUNDRY_AVAILABLE_IN_1_1
void                       foundry_file_search_options_set_recursive         (FoundryFileSearchOptions *self,
                                                                              gboolean                  recursive);
FOUNDRY_AVAILABLE_IN_1_1
gboolean                   foundry_file_search_options_get_case_sensitive    (FoundryFileSearchOptions *self);
FOUNDRY_AVAILABLE_IN_1_1
void                       foundry_file_search_options_set_case_sensitive    (FoundryFileSearchOptions *self,
                                                                              gboolean                  case_sensitive);
FOUNDRY_AVAILABLE_IN_1_1
gboolean                   foundry_file_search_options_get_match_whole_words (FoundryFileSearchOptions *self);
FOUNDRY_AVAILABLE_IN_1_1
void                       foundry_file_search_options_set_match_whole_words (FoundryFileSearchOptions *self,
                                                                              gboolean                  match_whole_words);
FOUNDRY_AVAILABLE_IN_1_1
gboolean                   foundry_file_search_options_get_use_regex         (FoundryFileSearchOptions *self);
FOUNDRY_AVAILABLE_IN_1_1
void                       foundry_file_search_options_set_use_regex         (FoundryFileSearchOptions *self,
                                                                              gboolean                  use_regex);
FOUNDRY_AVAILABLE_IN_1_1
guint                      foundry_file_search_options_get_max_matches       (FoundryFileSearchOptions *self);
FOUNDRY_AVAILABLE_IN_1_1
void                       foundry_file_search_options_set_max_matches       (FoundryFileSearchOptions *self,
                                                                              guint                     max_matches);
FOUNDRY_AVAILABLE_IN_1_1
guint                      foundry_file_search_options_get_context_lines     (FoundryFileSearchOptions *self);
FOUNDRY_AVAILABLE_IN_1_1
void                       foundry_file_search_options_set_context_lines     (FoundryFileSearchOptions *self,
                                                                              guint                     context_lines);
FOUNDRY_AVAILABLE_IN_1_1
char                     **foundry_file_search_options_dup_required_patterns (FoundryFileSearchOptions *self);
FOUNDRY_AVAILABLE_IN_1_1
void                       foundry_file_search_options_set_required_patterns (FoundryFileSearchOptions *self,
                                                                              const char * const       *required_patterns);
FOUNDRY_AVAILABLE_IN_1_1
char                     **foundry_file_search_options_dup_excluded_patterns (FoundryFileSearchOptions *self);
FOUNDRY_AVAILABLE_IN_1_1
void                       foundry_file_search_options_set_excluded_patterns (FoundryFileSearchOptions *self,
                                                                              const char * const       *excluded_patterns);
FOUNDRY_AVAILABLE_IN_1_1
FoundryFileSearchOptions  *foundry_file_search_options_copy                  (FoundryFileSearchOptions *self);

G_END_DECLS
