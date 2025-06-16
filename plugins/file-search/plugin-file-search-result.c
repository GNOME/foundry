/* plugin-file-search-result.c
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

#include "plugin-file-search-result.h"

struct _PluginFileSearchResult
{
  GObject parent_instance;
  char *filename;
  gdouble score;
};

G_DEFINE_FINAL_TYPE (PluginFileSearchResult, plugin_file_search_result, FOUNDRY_TYPE_SEARCH_RESULT)

static char *
plugin_file_search_result_dup_title (FoundrySearchResult *result)
{
  return g_strdup (PLUGIN_FILE_SEARCH_RESULT (result)->filename);
}

static void
plugin_file_search_result_finalize (GObject *object)
{
  PluginFileSearchResult *self = (PluginFileSearchResult *)object;

  g_clear_pointer (&self->filename, g_free);

  G_OBJECT_CLASS (plugin_file_search_result_parent_class)->finalize (object);
}

static void
plugin_file_search_result_class_init (PluginFileSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundrySearchResultClass *search_result_class = FOUNDRY_SEARCH_RESULT_CLASS (klass);

  object_class->finalize = plugin_file_search_result_finalize;

  search_result_class->dup_title = plugin_file_search_result_dup_title;
}

static void
plugin_file_search_result_init (PluginFileSearchResult *self)
{
}

PluginFileSearchResult *
plugin_file_search_result_new (const char *filename,
                               gdouble     score)
{
  PluginFileSearchResult *self;

  self = g_object_new (PLUGIN_TYPE_FILE_SEARCH_RESULT, NULL);
  self->filename = g_strdup (filename);
  self->score = score;

  return self;
}
