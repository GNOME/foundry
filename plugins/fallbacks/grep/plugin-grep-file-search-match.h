/* plugin-grep-file-search-match.h
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

#include <foundry.h>

G_BEGIN_DECLS

#define PLUGIN_TYPE_GREP_FILE_SEARCH_MATCH (plugin_grep_file_search_match_get_type())

G_DECLARE_FINAL_TYPE (PluginGrepFileSearchMatch, plugin_grep_file_search_match, PLUGIN, GREP_FILE_SEARCH_MATCH, FoundryFileSearchMatch)

FoundryFileSearchMatch *plugin_grep_file_search_match_new (GFile *file,
                                                           guint  line,
                                                           guint  line_offset,
                                                           guint  length,
                                                           char  *before_context,
                                                           char  *text,
                                                           char  *after_context);

G_END_DECLS
