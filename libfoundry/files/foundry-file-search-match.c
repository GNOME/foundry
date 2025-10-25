/* foundry-file-search-match.c
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

#include "config.h"

#include "foundry-file-search-match-private.h"

/**
 * SECTION:foundry-file-search-match
 * @title: FoundryFileSearchMatch
 * @short_description: A search match result
 *
 * #FoundryFileSearchMatch represents a search match result containing
 * information about where a search term was found in a file, including
 * the file location, line number, character offset, and match length.
 *
 * Since: 1.1
 */

struct _FoundryFileSearchMatch
{
  GObject parent_instance;

  GFile *file;
  guint  line;
  guint  line_offset;
  guint  length;
};

G_DEFINE_FINAL_TYPE (FoundryFileSearchMatch, foundry_file_search_match, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_FILE,
  PROP_LINE,
  PROP_LINE_OFFSET,
  PROP_LENGTH,
  PROP_URI,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_file_search_match_finalize (GObject *object)
{
  FoundryFileSearchMatch *self = FOUNDRY_FILE_SEARCH_MATCH (object);

  g_clear_object (&self->file);

  G_OBJECT_CLASS (foundry_file_search_match_parent_class)->finalize (object);
}

static void
foundry_file_search_match_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  FoundryFileSearchMatch *self = FOUNDRY_FILE_SEARCH_MATCH (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_take_object (value, foundry_file_search_match_dup_file (self));
      break;

    case PROP_LINE:
      g_value_set_uint (value, self->line);
      break;

    case PROP_LINE_OFFSET:
      g_value_set_uint (value, self->line_offset);
      break;

    case PROP_LENGTH:
      g_value_set_uint (value, self->length);
      break;

    case PROP_URI:
      if (self->file != NULL)
        g_value_take_string (value, g_file_get_uri (self->file));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_file_search_match_class_init (FoundryFileSearchMatchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_file_search_match_finalize;
  object_class->get_property = foundry_file_search_match_get_property;

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_LINE] =
    g_param_spec_uint ("line", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_LINE_OFFSET] =
    g_param_spec_uint ("line-offset", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_LENGTH] =
    g_param_spec_uint ("length", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_file_search_match_init (FoundryFileSearchMatch *self)
{
}

/**
 * _foundry_file_search_match_new:
 * @file: a #GFile
 * @line: the line number (0-based)
 * @line_offset: the character offset within the line (0-based)
 * @length: the length of the match in characters
 *
 * Creates a new #FoundryFileSearchMatch with the given properties.
 *
 * Returns: (transfer full): a new #FoundryFileSearchMatch
 *
 * Since: 1.1
 */
FoundryFileSearchMatch *
_foundry_file_search_match_new (GFile  *file,
                                guint   line,
                                guint   line_offset,
                                guint   length)
{
  FoundryFileSearchMatch *self;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  self = g_object_new (FOUNDRY_TYPE_FILE_SEARCH_MATCH, NULL);
  self->file = g_object_ref (file);
  self->line = line;
  self->line_offset = line_offset;
  self->length = length;

  return self;
}

/**
 * foundry_file_search_match_dup_file:
 * @self: a #FoundryFileSearchMatch
 *
 * Gets a copy of the file associated with the search match.
 *
 * Returns: (transfer full): a #GFile, or %NULL
 *
 * Since: 1.1
 */
GFile *
foundry_file_search_match_dup_file (FoundryFileSearchMatch *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_MATCH (self), NULL);

  return g_object_ref (self->file);
}

/**
 * foundry_file_search_match_get_line:
 * @self: a #FoundryFileSearchMatch
 *
 * Gets the line number where the search match was found.
 *
 * Returns: the line number (0-based)
 *
 * Since: 1.1
 */
guint
foundry_file_search_match_get_line (FoundryFileSearchMatch *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_MATCH (self), 0);

  return self->line;
}

/**
 * foundry_file_search_match_get_line_offset:
 * @self: a #FoundryFileSearchMatch
 *
 * Gets the character offset within the line where the search match starts.
 *
 * Returns: the character offset within the line (0-based)
 *
 * Since: 1.1
 */
guint
foundry_file_search_match_get_line_offset (FoundryFileSearchMatch *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_MATCH (self), 0);

  return self->line_offset;
}

/**
 * foundry_file_search_match_get_length:
 * @self: a #FoundryFileSearchMatch
 *
 * Gets the length of the search match in characters.
 *
 * Returns: the length of the match in characters
 *
 * Since: 1.1
 */
guint
foundry_file_search_match_get_length (FoundryFileSearchMatch *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_MATCH (self), 0);

  return self->length;
}
