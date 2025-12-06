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

#include "foundry-file-manager.h"
#include "foundry-file-search-match.h"

/**
 * FoundryFileSearchMatch:
 *
 * A file search match result.
 *
 * [class@Foundry.FileSearchMatch] represents a search match result containing
 * information about where a search term was found in a file, including the
 * file location, line number, character offset, and text length.
 *
 * Since: 1.1
 */

G_DEFINE_ABSTRACT_TYPE (FoundryFileSearchMatch, foundry_file_search_match, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_AFTER_CONTEXT,
  PROP_BEFORE_CONTEXT,
  PROP_FILE,
  PROP_ICON,
  PROP_LENGTH,
  PROP_LINE,
  PROP_LINE_OFFSET,
  PROP_TEXT,
  PROP_URI,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

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
      g_value_set_uint (value, foundry_file_search_match_get_line (self));
      break;

    case PROP_LINE_OFFSET:
      g_value_set_uint (value, foundry_file_search_match_get_line_offset (self));
      break;

    case PROP_LENGTH:
      g_value_set_uint (value, foundry_file_search_match_get_length (self));
      break;

    case PROP_URI: {
      g_autoptr(GFile) file = foundry_file_search_match_dup_file (self);

      if (file != NULL)
        g_value_take_string (value, g_file_get_uri (file));
      break;
    }

    case PROP_BEFORE_CONTEXT:
      g_value_take_string (value, foundry_file_search_match_dup_before_context (self));
      break;

    case PROP_TEXT:
      g_value_take_string (value, foundry_file_search_match_dup_text (self));
      break;

    case PROP_AFTER_CONTEXT:
      g_value_take_string (value, foundry_file_search_match_dup_after_context (self));
      break;

    case PROP_ICON:
      g_value_take_object (value, foundry_file_search_match_dup_icon (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_file_search_match_class_init (FoundryFileSearchMatchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

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

  properties[PROP_BEFORE_CONTEXT] =
    g_param_spec_string ("before-context", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_TEXT] =
    g_param_spec_string ("text", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_AFTER_CONTEXT] =
    g_param_spec_string ("after-context", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_file_search_match_init (FoundryFileSearchMatch *self)
{
}

/**
 * foundry_file_search_match_dup_file:
 * @self: a #FoundryFileSearchMatch
 *
 * Gets a copy of the file associated with the search match.
 *
 * Returns: (transfer none) (not nullable): a #GFile
 *
 * Since: 1.1
 */
GFile *
foundry_file_search_match_dup_file (FoundryFileSearchMatch *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_MATCH (self), NULL);

  return FOUNDRY_FILE_SEARCH_MATCH_GET_CLASS (self)->dup_file (self);
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

  return FOUNDRY_FILE_SEARCH_MATCH_GET_CLASS (self)->get_line (self);
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

  return FOUNDRY_FILE_SEARCH_MATCH_GET_CLASS (self)->get_line_offset (self);
}

/**
 * foundry_file_search_match_get_length:
 * @self: a #FoundryFileSearchMatch
 *
 * Gets the length of the search text in characters.
 *
 * Returns: the length of the text in characters
 *
 * Since: 1.1
 */
guint
foundry_file_search_match_get_length (FoundryFileSearchMatch *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_MATCH (self), 0);

  return FOUNDRY_FILE_SEARCH_MATCH_GET_CLASS (self)->get_length (self);
}

/**
 * foundry_file_search_match_dup_before_context:
 * @self: a #FoundryFileSearchMatch
 *
 * Gets the text before the search text.
 *
 * Returns: (nullable): the text before the match, or %NULL
 *
 * Since: 1.1
 */
char *
foundry_file_search_match_dup_before_context (FoundryFileSearchMatch *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_MATCH (self), NULL);

  return FOUNDRY_FILE_SEARCH_MATCH_GET_CLASS (self)->dup_before_context (self);
}

/**
 * foundry_file_search_match_dup_text:
 * @self: a #FoundryFileSearchMatch
 *
 * Gets the matched text.
 *
 * Returns: (nullable): the matched text, or %NULL
 *
 * Since: 1.1
 */
char *
foundry_file_search_match_dup_text (FoundryFileSearchMatch *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_MATCH (self), NULL);

  return FOUNDRY_FILE_SEARCH_MATCH_GET_CLASS (self)->dup_text (self);
}

/**
 * foundry_file_search_match_dup_after_context:
 * @self: a #FoundryFileSearchMatch
 *
 * Gets the text after the search text.
 *
 * Returns: (nullable): the text after the match, or %NULL
 *
 * Since: 1.1
 */
char *
foundry_file_search_match_dup_after_context (FoundryFileSearchMatch *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_MATCH (self), NULL);

  return FOUNDRY_FILE_SEARCH_MATCH_GET_CLASS (self)->dup_after_context (self);
}

/**
 * foundry_file_search_match_dup_icon:
 * @self: a #FoundryFileSearchMatch
 *
 * Gets the icon for the file associated with the search match.
 *
 * Returns: (transfer full) (nullable): a #GIcon or %NULL
 *
 * Since: 1.1
 */
GIcon *
foundry_file_search_match_dup_icon (FoundryFileSearchMatch *self)
{
  g_autoptr(GFile) file = NULL;
  g_autofree char *basename = NULL;

  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_MATCH (self), NULL);

  if (!(file = foundry_file_search_match_dup_file (self)))
    return NULL;

  basename = g_file_get_basename (file);

  return foundry_file_manager_find_symbolic_icon (NULL, NULL, basename);
}
