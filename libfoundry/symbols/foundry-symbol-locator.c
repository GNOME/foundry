/* foundry-symbol-locator.c
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

#include "line-reader-private.h"

#include "foundry-symbol-locator.h"

struct _FoundrySymbolLocator
{
  GObject parent_instance;

  GFile *file;
  guint line;
  guint line_offset;
  char *pattern;
  guint line_set : 1;
  guint line_offset_set : 1;
};

G_DEFINE_FINAL_TYPE (FoundrySymbolLocator, foundry_symbol_locator, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_FILE,
  PROP_LINE,
  PROP_LINE_OFFSET,
  PROP_PATTERN,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_symbol_locator_dispose (GObject *object)
{
  FoundrySymbolLocator *self = FOUNDRY_SYMBOL_LOCATOR (object);

  g_clear_object (&self->file);
  g_clear_pointer (&self->pattern, g_free);

  G_OBJECT_CLASS (foundry_symbol_locator_parent_class)->dispose (object);
}

static void
foundry_symbol_locator_finalize (GObject *object)
{
  G_OBJECT_CLASS (foundry_symbol_locator_parent_class)->finalize (object);
}

static void
foundry_symbol_locator_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  FoundrySymbolLocator *self = FOUNDRY_SYMBOL_LOCATOR (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_take_object (value, foundry_symbol_locator_dup_file (self));
      break;

    case PROP_LINE:
      g_value_set_uint (value, foundry_symbol_locator_get_line (self));
      break;

    case PROP_LINE_OFFSET:
      g_value_set_uint (value, foundry_symbol_locator_get_line_offset (self));
      break;

    case PROP_PATTERN:
      g_value_set_string (value, foundry_symbol_locator_get_pattern (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_symbol_locator_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  FoundrySymbolLocator *self = FOUNDRY_SYMBOL_LOCATOR (object);

  switch (prop_id)
    {
    case PROP_FILE:
      self->file = g_value_dup_object (value);
      break;

    case PROP_LINE:
      self->line = g_value_get_uint (value);
      self->line_set = TRUE;
      break;

    case PROP_LINE_OFFSET:
      self->line_offset = g_value_get_uint (value);
      self->line_offset_set = TRUE;
      break;

    case PROP_PATTERN:
      self->pattern = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_symbol_locator_class_init (FoundrySymbolLocatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_symbol_locator_dispose;
  object_class->finalize = foundry_symbol_locator_finalize;
  object_class->get_property = foundry_symbol_locator_get_property;
  object_class->set_property = foundry_symbol_locator_set_property;

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY|
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_LINE] =
    g_param_spec_uint ("line", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_LINE_OFFSET] =
    g_param_spec_uint ("line-offset", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_PATTERN] =
    g_param_spec_string ("pattern", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_symbol_locator_init (FoundrySymbolLocator *self)
{
}

static void
calculate_line_and_offset (const char *contents,
                           gsize       contents_len,
                           gsize       byte_offset,
                           guint      *line,
                           guint      *line_offset)
{
  LineReader reader;
  const char *line_str;
  gsize line_len;
  guint current_line = 0;

  line_reader_init (&reader, (char *)contents, contents_len);

  while ((line_str = line_reader_next (&reader, &line_len)))
    {
      gsize line_start_offset = line_str - contents;
      gsize line_end_offset = line_start_offset + line_len;

      if (byte_offset >= line_start_offset && byte_offset <= line_end_offset)
        {
          const char *byte_ptr = contents + byte_offset;
          gsize char_offset = g_utf8_pointer_to_offset (line_str, byte_ptr);

          *line = current_line;
          *line_offset = (guint)char_offset;
          return;
        }

      current_line++;
    }

  *line = current_line;
  *line_offset = 0;
}

FoundrySymbolLocator *
foundry_symbol_locator_new_for_file (GFile *file)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (FOUNDRY_TYPE_SYMBOL_LOCATOR,
                       "file", file,
                       NULL);
}

FoundrySymbolLocator *
foundry_symbol_locator_new_for_file_and_line (GFile *file,
                                              guint  line)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (FOUNDRY_TYPE_SYMBOL_LOCATOR,
                       "file", file,
                       "line", line,
                       NULL);
}

FoundrySymbolLocator *
foundry_symbol_locator_new_for_file_and_line_offset (GFile *file,
                                                     guint  line,
                                                     guint  line_offset)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (FOUNDRY_TYPE_SYMBOL_LOCATOR,
                       "file", file,
                       "line", line,
                       "line-offset", line_offset,
                       NULL);
}

FoundrySymbolLocator *
foundry_symbol_locator_new_for_file_and_pattern (GFile      *file,
                                                 const char *pattern)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (FOUNDRY_TYPE_SYMBOL_LOCATOR,
                       "file", file,
                       "pattern", pattern,
                       NULL);
}

const char *
foundry_symbol_locator_get_pattern (FoundrySymbolLocator *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SYMBOL_LOCATOR (self), NULL);

  return self->pattern;
}

FoundrySymbolLocator *
foundry_symbol_locator_locate (FoundrySymbolLocator *self,
                               GBytes             *contents)
{
  g_autoptr(GMatchInfo) match_info = NULL;
  g_autoptr(GRegex) regex = NULL;
  g_autoptr(GError) error = NULL;
  const char *text;
  gsize text_len;
  gint start_pos;
  gint end_pos;
  guint line;
  guint line_offset;

  g_return_val_if_fail (FOUNDRY_IS_SYMBOL_LOCATOR (self), NULL);
  g_return_val_if_fail (contents != NULL, NULL);

  if (!foundry_symbol_locator_get_pattern (self))
    return g_object_ref (self);

  if (self->pattern == NULL || self->pattern[0] == '\0')
    return NULL;

  regex = g_regex_new (self->pattern, G_REGEX_OPTIMIZE, 0, &error);
  if (regex == NULL)
    return NULL;

  text = (const char *)g_bytes_get_data (contents, &text_len);
  if (!g_regex_match_full (regex, text, text_len, 0, 0, &match_info, &error))
    return NULL;

  if (!g_match_info_matches (match_info))
    return NULL;

  g_match_info_fetch_pos (match_info, 0, &start_pos, &end_pos);
  calculate_line_and_offset (text, text_len, start_pos, &line, &line_offset);

  return foundry_symbol_locator_new_for_file_and_line_offset (self->file, line, line_offset);
}

GFile *
foundry_symbol_locator_dup_file (FoundrySymbolLocator *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SYMBOL_LOCATOR (self), NULL);

  return self->file ? g_object_ref (self->file) : NULL;
}

gboolean
foundry_symbol_locator_get_line_set (FoundrySymbolLocator *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SYMBOL_LOCATOR (self), FALSE);

  return self->line_set;
}

guint
foundry_symbol_locator_get_line (FoundrySymbolLocator *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SYMBOL_LOCATOR (self), 0);

  return self->line;
}

gboolean
foundry_symbol_locator_get_line_offset_set (FoundrySymbolLocator *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SYMBOL_LOCATOR (self), FALSE);

  return self->line_offset_set;
}

guint
foundry_symbol_locator_get_line_offset (FoundrySymbolLocator *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SYMBOL_LOCATOR (self), 0);

  return self->line_offset;
}
