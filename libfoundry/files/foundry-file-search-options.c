/* foundry-file-search-options.c
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

#include "foundry-file-search-options.h"

struct _FoundryFileSearchOptions
{
  GObject parent_instance;

  char *search_text;
  GListStore *targets;
  guint max_matches;
  guint context_lines;
  guint recursive : 1;
  guint case_sensitive : 1;
  guint match_whole_words : 1;
  guint use_regex : 1;
};

G_DEFINE_FINAL_TYPE (FoundryFileSearchOptions, foundry_file_search_options, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SEARCH_TEXT,
  PROP_RECURSIVE,
  PROP_CASE_SENSITIVE,
  PROP_MATCH_WHOLE_WORDS,
  PROP_USE_REGEX,
  PROP_MAX_MATCHES,
  PROP_CONTEXT_LINES,
  PROP_TARGETS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_file_search_options_finalize (GObject *object)
{
  FoundryFileSearchOptions *self = FOUNDRY_FILE_SEARCH_OPTIONS (object);

  g_clear_pointer (&self->search_text, g_free);
  g_clear_object (&self->targets);

  G_OBJECT_CLASS (foundry_file_search_options_parent_class)->finalize (object);
}

static void
foundry_file_search_options_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  FoundryFileSearchOptions *self = FOUNDRY_FILE_SEARCH_OPTIONS (object);

  switch (prop_id)
    {
    case PROP_SEARCH_TEXT:
      g_value_take_string (value, foundry_file_search_options_dup_search_text (self));
      break;

    case PROP_RECURSIVE:
      g_value_set_boolean (value, self->recursive);
      break;

    case PROP_CASE_SENSITIVE:
      g_value_set_boolean (value, self->case_sensitive);
      break;

    case PROP_MATCH_WHOLE_WORDS:
      g_value_set_boolean (value, self->match_whole_words);
      break;

    case PROP_USE_REGEX:
      g_value_set_boolean (value, self->use_regex);
      break;

    case PROP_MAX_MATCHES:
      g_value_set_uint (value, self->max_matches);
      break;

    case PROP_CONTEXT_LINES:
      g_value_set_uint (value, self->context_lines);
      break;

    case PROP_TARGETS:
      g_value_take_object (value, foundry_file_search_options_list_targets (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_file_search_options_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  FoundryFileSearchOptions *self = FOUNDRY_FILE_SEARCH_OPTIONS (object);

  switch (prop_id)
    {
    case PROP_SEARCH_TEXT:
      foundry_file_search_options_set_search_text (self, g_value_get_string (value));
      break;

    case PROP_RECURSIVE:
      foundry_file_search_options_set_recursive (self, g_value_get_boolean (value));
      break;

    case PROP_CASE_SENSITIVE:
      foundry_file_search_options_set_case_sensitive (self, g_value_get_boolean (value));
      break;

    case PROP_MATCH_WHOLE_WORDS:
      foundry_file_search_options_set_match_whole_words (self, g_value_get_boolean (value));
      break;

    case PROP_USE_REGEX:
      foundry_file_search_options_set_use_regex (self, g_value_get_boolean (value));
      break;

    case PROP_MAX_MATCHES:
      foundry_file_search_options_set_max_matches (self, g_value_get_uint (value));
      break;

    case PROP_CONTEXT_LINES:
      foundry_file_search_options_set_context_lines (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_file_search_options_class_init (FoundryFileSearchOptionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_file_search_options_finalize;
  object_class->get_property = foundry_file_search_options_get_property;
  object_class->set_property = foundry_file_search_options_set_property;

  properties[PROP_SEARCH_TEXT] =
    g_param_spec_string ("search-text", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_RECURSIVE] =
    g_param_spec_boolean ("recursive", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_CASE_SENSITIVE] =
    g_param_spec_boolean ("case-sensitive", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_MATCH_WHOLE_WORDS] =
    g_param_spec_boolean ("match-whole-words", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_USE_REGEX] =
    g_param_spec_boolean ("use-regex", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_MAX_MATCHES] =
    g_param_spec_uint ("max-matches", NULL, NULL,
                       0,
                       G_MAXUINT,
                       10000,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_CONTEXT_LINES] =
    g_param_spec_uint ("context-lines", NULL, NULL,
                       0,
                       G_MAXUINT,
                       1,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_TARGETS] =
    g_param_spec_object ("targets", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_file_search_options_init (FoundryFileSearchOptions *self)
{
  self->targets = g_list_store_new (G_TYPE_FILE);
  self->max_matches = 10000;
  self->context_lines = 1;
  self->recursive = FALSE;
  self->case_sensitive = FALSE;
  self->match_whole_words = FALSE;
  self->use_regex = FALSE;
}

/**
 * foundry_file_search_options_new:
 *
 * Creates a new [class@Foundry.FileSearchOptions] instance.
 *
 * Returns: (transfer full): a new [class@Foundry.FileSearchOptions]
 *
 * Since: 1.1
 */
FoundryFileSearchOptions *
foundry_file_search_options_new (void)
{
  return g_object_new (FOUNDRY_TYPE_FILE_SEARCH_OPTIONS, NULL);
}

/**
 * foundry_file_search_options_add_target:
 * @self: a [class@Foundry.FileSearchOptions]
 * @file: a [iface@Gio.File] to add as a search target
 *
 * Adds a file as a target for searching.
 *
 * Since: 1.1
 */
void
foundry_file_search_options_add_target (FoundryFileSearchOptions *self,
                                        GFile                    *file)
{
  g_return_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self));
  g_return_if_fail (G_IS_FILE (file));

  g_list_store_append (self->targets, file);
}

/**
 * foundry_file_search_options_remove_target:
 * @self: a [class@Foundry.FileSearchOptions]
 * @file: a [iface@Gio.File] to remove from search targets
 *
 * Removes a file from the search targets.
 *
 * Since: 1.1
 */
void
foundry_file_search_options_remove_target (FoundryFileSearchOptions *self,
                                           GFile                    *file)
{
  guint n_items;

  g_return_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self));
  g_return_if_fail (G_IS_FILE (file));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->targets));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (GFile) item = g_list_model_get_item (G_LIST_MODEL (self->targets), i);

      if (g_file_equal (item, file))
        {
          g_list_store_remove (self->targets, i);
          break;
        }
    }
}

/**
 * foundry_file_search_options_list_targets:
 * @self: a [class@Foundry.FileSearchOptions]
 *
 * Gets the list of files that are targets for searching.
 *
 * Returns: (transfer full) (not nullable): a [iface@Gio.ListModel] of
 *   [iface@Gio.File] that are targets for searching.
 *
 * Since: 1.1
 */
GListModel *
foundry_file_search_options_list_targets (FoundryFileSearchOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->targets));
}

/**
 * foundry_file_search_options_dup_search_text:
 * @self: a [class@Foundry.FileSearchOptions]
 *
 * Gets a copy of the search text.
 *
 * Returns: (transfer full) (nullable): a newly allocated copy of the search text,
 *   or %NULL if not set
 *
 * Since: 1.1
 */
char *
foundry_file_search_options_dup_search_text (FoundryFileSearchOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self), NULL);

  return g_strdup (self->search_text);
}

/**
 * foundry_file_search_options_set_search_text:
 * @self: a [class@Foundry.FileSearchOptions]
 * @search_text: (nullable): the text to search for
 *
 * Sets the search text.
 *
 * Since: 1.1
 */
void
foundry_file_search_options_set_search_text (FoundryFileSearchOptions *self,
                                             const char               *search_text)
{
  g_return_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self));

  if (g_set_str (&self->search_text, search_text))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEARCH_TEXT]);
}

/**
 * foundry_file_search_options_get_recursive:
 * @self: a [class@Foundry.FileSearchOptions]
 *
 * Gets whether the search should be recursive.
 *
 * Returns: %TRUE if the search should be recursive
 *
 * Since: 1.1
 */
gboolean
foundry_file_search_options_get_recursive (FoundryFileSearchOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self), FALSE);

  return self->recursive;
}

/**
 * foundry_file_search_options_set_recursive:
 * @self: a [class@Foundry.FileSearchOptions]
 * @recursive: whether the search should be recursive
 *
 * Sets whether the search should be recursive.
 *
 * Since: 1.1
 */
void
foundry_file_search_options_set_recursive (FoundryFileSearchOptions *self,
                                           gboolean                  recursive)
{
  g_return_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self));

  if (self->recursive != recursive)
    {
      self->recursive = recursive;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RECURSIVE]);
    }
}

/**
 * foundry_file_search_options_get_case_sensitive:
 * @self: a [class@Foundry.FileSearchOptions]
 *
 * Gets whether the search should be case sensitive.
 *
 * Returns: %TRUE if the search should be case sensitive
 *
 * Since: 1.1
 */
gboolean
foundry_file_search_options_get_case_sensitive (FoundryFileSearchOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self), FALSE);

  return self->case_sensitive;
}

/**
 * foundry_file_search_options_set_case_sensitive:
 * @self: a [class@Foundry.FileSearchOptions]
 * @case_sensitive: whether the search should be case sensitive
 *
 * Sets whether the search should be case sensitive.
 *
 * Since: 1.1
 */
void
foundry_file_search_options_set_case_sensitive (FoundryFileSearchOptions *self,
                                                gboolean                  case_sensitive)
{
  g_return_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self));

  if (self->case_sensitive != case_sensitive)
    {
      self->case_sensitive = case_sensitive;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CASE_SENSITIVE]);
    }
}

/**
 * foundry_file_search_options_get_match_whole_words:
 * @self: a [class@Foundry.FileSearchOptions]
 *
 * Gets whether the search should match whole words only.
 *
 * Returns: %TRUE if the search should match whole words only
 *
 * Since: 1.1
 */
gboolean
foundry_file_search_options_get_match_whole_words (FoundryFileSearchOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self), FALSE);

  return self->match_whole_words;
}

/**
 * foundry_file_search_options_set_match_whole_words:
 * @self: a [class@Foundry.FileSearchOptions]
 * @match_whole_words: whether the search should match whole words only
 *
 * Sets whether the search should match whole words only.
 *
 * Since: 1.1
 */
void
foundry_file_search_options_set_match_whole_words (FoundryFileSearchOptions *self,
                                                   gboolean                  match_whole_words)
{
  g_return_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self));

  if (self->match_whole_words != match_whole_words)
    {
      self->match_whole_words = match_whole_words;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MATCH_WHOLE_WORDS]);
    }
}

/**
 * foundry_file_search_options_get_use_regex:
 * @self: a [class@Foundry.FileSearchOptions]
 *
 * Gets whether the search should use regular expressions.
 *
 * Returns: %TRUE if the search should use regular expressions
 *
 * Since: 1.1
 */
gboolean
foundry_file_search_options_get_use_regex (FoundryFileSearchOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self), FALSE);

  return self->use_regex;
}

/**
 * foundry_file_search_options_set_use_regex:
 * @self: a [class@Foundry.FileSearchOptions]
 * @use_regex: whether the search should use regular expressions
 *
 * Sets whether the search should use regular expressions.
 *
 * Since: 1.1
 */
void
foundry_file_search_options_set_use_regex (FoundryFileSearchOptions *self,
                                           gboolean                  use_regex)
{
  g_return_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self));

  if (self->use_regex != use_regex)
    {
      self->use_regex = use_regex;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_REGEX]);
    }
}

/**
 * foundry_file_search_options_get_max_matches:
 * @self: a [class@Foundry.FileSearchOptions]
 *
 * Gets the maximum number of matches to return.
 *
 * Returns: the maximum number of matches
 *
 * Since: 1.1
 */
guint
foundry_file_search_options_get_max_matches (FoundryFileSearchOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self), 0);

  return self->max_matches;
}

/**
 * foundry_file_search_options_set_max_matches:
 * @self: a [class@Foundry.FileSearchOptions]
 * @max_matches: the maximum number of matches to return
 *
 * Sets the maximum number of matches to return.
 *
 * Since: 1.1
 */
void
foundry_file_search_options_set_max_matches (FoundryFileSearchOptions *self,
                                             guint                     max_matches)
{
  g_return_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self));

  if (self->max_matches != max_matches)
    {
      self->max_matches = max_matches;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_MATCHES]);
    }
}

/**
 * foundry_file_search_options_get_context_lines:
 * @self: a [class@Foundry.FileSearchOptions]
 *
 * Gets the number of context lines to include with each match.
 * A value of 1 means 1 line before and 1 line after the match.
 *
 * Returns: the number of context lines
 *
 * Since: 1.1
 */
guint
foundry_file_search_options_get_context_lines (FoundryFileSearchOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self), 0);

  return self->context_lines;
}

/**
 * foundry_file_search_options_set_context_lines:
 * @self: a [class@Foundry.FileSearchOptions]
 * @context_lines: the number of context lines to include with each match
 *
 * Sets the number of context lines to include with each match.
 * A value of 1 means 1 line before and 1 line after the match.
 *
 * Since: 1.1
 */
void
foundry_file_search_options_set_context_lines (FoundryFileSearchOptions *self,
                                               guint                     context_lines)
{
  g_return_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self));

  if (self->context_lines != context_lines)
    {
      self->context_lines = context_lines;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTEXT_LINES]);
    }
}

/**
 * foundry_file_search_options_copy:
 * @self: a [class@Foundry.FileSearchOptions]
 *
 * Creates a new [class@Foundry.FileSearchOptions] with a copy of all the values
 * from the original.
 *
 * Returns: (transfer full) (not nullable): a new [class@Foundry.FileSearchOptions]
 */
FoundryFileSearchOptions *
foundry_file_search_options_copy (FoundryFileSearchOptions *self)
{
  FoundryFileSearchOptions *copy;
  guint n_items;

  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (self), NULL);

  copy = foundry_file_search_options_new ();

  if (self->search_text != NULL)
    foundry_file_search_options_set_search_text (copy, self->search_text);

  foundry_file_search_options_set_recursive (copy, self->recursive);
  foundry_file_search_options_set_case_sensitive (copy, self->case_sensitive);
  foundry_file_search_options_set_match_whole_words (copy, self->match_whole_words);
  foundry_file_search_options_set_use_regex (copy, self->use_regex);
  foundry_file_search_options_set_max_matches (copy, self->max_matches);
  foundry_file_search_options_set_context_lines (copy, self->context_lines);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->targets));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (GFile) file = g_list_model_get_item (G_LIST_MODEL (self->targets), i);

      foundry_file_search_options_add_target (copy, file);
    }

  return copy;
}
