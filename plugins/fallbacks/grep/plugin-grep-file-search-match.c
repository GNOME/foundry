/* plugin-grep-file-search-match.c
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

#include "config.h"

#include "plugin-grep-file-search-match.h"

struct _PluginGrepFileSearchMatch
{
  FoundryFileSearchMatch parent_instance;
  GFile *file;
  char  *before_context;
  char  *text;
  char  *after_context;
  guint  line;
  guint  line_offset;
  guint  length;
};

G_DEFINE_FINAL_TYPE (PluginGrepFileSearchMatch, plugin_grep_file_search_match, FOUNDRY_TYPE_FILE_SEARCH_MATCH)

static GFile *
plugin_grep_file_search_match_dup_file (FoundryFileSearchMatch *match)
{
  return g_object_ref (PLUGIN_GREP_FILE_SEARCH_MATCH (match)->file);
}

static char *
plugin_grep_file_search_match_dup_before_context (FoundryFileSearchMatch *match)
{
  return g_strdup (PLUGIN_GREP_FILE_SEARCH_MATCH (match)->before_context);
}

static char *
plugin_grep_file_search_match_dup_text (FoundryFileSearchMatch *match)
{
  return g_strdup (PLUGIN_GREP_FILE_SEARCH_MATCH (match)->text);
}

static char *
plugin_grep_file_search_match_dup_after_context (FoundryFileSearchMatch *match)
{
  return g_strdup (PLUGIN_GREP_FILE_SEARCH_MATCH (match)->after_context);
}

static guint
plugin_grep_file_search_match_get_line (FoundryFileSearchMatch *match)
{
  return PLUGIN_GREP_FILE_SEARCH_MATCH (match)->line;
}

static guint
plugin_grep_file_search_match_get_line_offset (FoundryFileSearchMatch *match)
{
  return PLUGIN_GREP_FILE_SEARCH_MATCH (match)->line_offset;
}

static guint
plugin_grep_file_search_match_get_length (FoundryFileSearchMatch *match)
{
  return PLUGIN_GREP_FILE_SEARCH_MATCH (match)->length;
}

static void
plugin_grep_file_search_match_finalize (GObject *object)
{
  PluginGrepFileSearchMatch *self = (PluginGrepFileSearchMatch *)object;

  g_clear_object (&self->file);
  g_clear_pointer (&self->before_context, g_free);
  g_clear_pointer (&self->text, g_free);
  g_clear_pointer (&self->after_context, g_free);

  G_OBJECT_CLASS (plugin_grep_file_search_match_parent_class)->finalize (object);
}

static void
plugin_grep_file_search_match_class_init (PluginGrepFileSearchMatchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryFileSearchMatchClass *file_search_match_class = FOUNDRY_FILE_SEARCH_MATCH_CLASS (klass);

  object_class->finalize = plugin_grep_file_search_match_finalize;

  file_search_match_class->dup_file = plugin_grep_file_search_match_dup_file;
  file_search_match_class->get_line = plugin_grep_file_search_match_get_line;
  file_search_match_class->get_line_offset = plugin_grep_file_search_match_get_line_offset;
  file_search_match_class->get_length = plugin_grep_file_search_match_get_length;
  file_search_match_class->dup_before_context = plugin_grep_file_search_match_dup_before_context;
  file_search_match_class->dup_after_context = plugin_grep_file_search_match_dup_after_context;
  file_search_match_class->dup_text = plugin_grep_file_search_match_dup_text;
}

static void
plugin_grep_file_search_match_init (PluginGrepFileSearchMatch *self)
{
}

/**
 * plugin_grep_file_search_match_new:
 * @file: a #GFile
 * @line: the line number (0-based)
 * @line_offset: the character offset within the line (0-based)
 *   where the match begins
 * @length: the length of the match in characters
 * @before_context: (transfer full): the text before the match
 * @text: (transfer full): the text line containing the match
 * @after_context: (transfer full): the text after the match
 *
 * Creates a new #FoundryFileSearchMatch with the given properties.
 *
 * Returns: (transfer full): a new #FoundryFileSearchMatch
 *
 * Since: 1.1
 */
FoundryFileSearchMatch *
plugin_grep_file_search_match_new (GFile *file,
                                   guint  line,
                                   guint  line_offset,
                                   guint  length,
                                   char  *before_context,
                                   char  *text,
                                   char  *after_context)
{
  PluginGrepFileSearchMatch *self;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  self = g_object_new (PLUGIN_TYPE_GREP_FILE_SEARCH_MATCH, NULL);
  self->file = g_object_ref (file);
  self->line = line;
  self->line_offset = line_offset;
  self->length = length;
  self->before_context = before_context;
  self->text = text;
  self->after_context = after_context;

  return FOUNDRY_FILE_SEARCH_MATCH (self);
}
