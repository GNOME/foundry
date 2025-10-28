/* foundry-file-search-replacement.c
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

#include <libdex.h>

#include "foundry-context.h"
#include "foundry-file-search-match.h"
#include "foundry-file-search-options.h"
#include "foundry-file-search-replacement.h"
#include "foundry-operation.h"
#include "foundry-text-edit.h"
#include "foundry-text-document.h"
#include "foundry-text-manager.h"
#include "foundry-util-private.h"

struct _FoundryFileSearchReplacement
{
  GObject                   parent_instance;
  FoundryContext           *context;
  GListModel               *matches;
  FoundryFileSearchOptions *options;
  char                     *replacement_text;
};

G_DEFINE_FINAL_TYPE (FoundryFileSearchReplacement, foundry_file_search_replacement, G_TYPE_OBJECT)

typedef struct
{
  FoundryFileSearchReplacement *self;
  FoundryOperation             *operation;
  GHashTable                   *edits_by_file;
} ApplyReplacementState;

static void
apply_replacement_state_free (ApplyReplacementState *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->operation);
  g_clear_pointer (&state->edits_by_file, g_hash_table_unref);
  g_free (state);
}

static DexFuture *
foundry_file_search_replacement_apply_fiber (gpointer data)
{
  ApplyReplacementState *state = data;
  g_autoptr(FoundryTextManager) text_manager = NULL;
  g_autoptr(GListStore) all_edits = NULL;
  g_autoptr(GError) error = NULL;
  GHashTableIter iter;
  gpointer key, value;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_FILE_SEARCH_REPLACEMENT (state->self));

  text_manager = foundry_context_dup_text_manager (state->self->context);
  all_edits = g_list_store_new (G_TYPE_OBJECT);

  g_hash_table_iter_init (&iter, state->edits_by_file);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GPtrArray *file_edits = value;
      g_autoptr(DexFuture) future = NULL;

      for (guint i = 0; i < file_edits->len; i++)
        {
          FoundryTextEdit *edit = g_ptr_array_index (file_edits, i);

          g_list_store_append (all_edits, edit);
        }
    }

  if (g_list_model_get_n_items (G_LIST_MODEL (all_edits)) == 0)
    return dex_future_new_true ();

  if (!dex_await (foundry_text_manager_apply_edits (text_manager,
                                                    G_LIST_MODEL (all_edits),
                                                    state->operation),
                  &error))
    {
      g_warning ("Failed to apply edits: %s", error->message);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

/**
 * foundry_file_search_replacement_new:
 * @context: a [class@Foundry.Context]
 * @matches: a [iface@Gio.ListModel] of [class@Foundry.FileSearchMatch]
 * @options: a [class@Foundry.FileSearchOptions]
 * @replacement_text: the replacement text (may contain back references if regex is enabled)
 *
 * Creates a new [class@Foundry.FileSearchReplacement] for performing
 * single or bulk text replacements.
 *
 * The @replacement_text can contain back references (like `\1`, `\2`, etc.)
 * if @options has regex enabled via [method@Foundry.FileSearchOptions.set_use_regex].
 *
 * Returns: (transfer full): a new [class@Foundry.FileSearchReplacement]
 *
 * Since: 1.1
 */
FoundryFileSearchReplacement *
foundry_file_search_replacement_new (FoundryContext           *context,
                                     GListModel               *matches,
                                     FoundryFileSearchOptions *options,
                                     const char               *replacement_text)
{
  FoundryFileSearchReplacement *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (G_IS_LIST_MODEL (matches), NULL);
  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (options), NULL);
  g_return_val_if_fail (replacement_text != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_FILE_SEARCH_REPLACEMENT, NULL);

  self->context = g_object_ref (context);
  self->matches = g_object_ref (matches);
  self->options = g_object_ref (options);
  self->replacement_text = g_strdup (replacement_text);

  return self;
}

/**
 * foundry_file_search_replacement_apply:
 * @self: a [class@Foundry.FileSearchReplacement]
 *
 * Applies the text replacements to all matched locations.
 *
 * This method starts a fiber and performs replacements asynchronously.
 * Each replacement is performed using [class@Foundry.TextEdit] and applied
 * using [class@Foundry.TextManager].
 *
 * Returns: (transfer full): a [class@Dex.Future] that completes when
 *   all replacements are finished
 *
 * Since: 1.1
 */
DexFuture *
foundry_file_search_replacement_apply (FoundryFileSearchReplacement *self)
{
  g_autoptr(GHashTable) edits_by_file = NULL;
  g_autoptr(GRegex) regex = NULL;
  g_autoptr(GError) error = NULL;
  ApplyReplacementState *state;
  guint n_matches;

  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_REPLACEMENT (self), NULL);

  n_matches = g_list_model_get_n_items (self->matches);
  edits_by_file = g_hash_table_new_full ((GHashFunc) g_file_hash,
                                         (GEqualFunc) g_file_equal,
                                         g_object_unref,
                                         (GDestroyNotify) g_ptr_array_unref);

  if (foundry_file_search_options_get_use_regex (self->options))
    {
      g_autofree char *pattern = foundry_file_search_options_dup_search_text (self->options);
      GRegexCompileFlags flags = G_REGEX_OPTIMIZE;

      if (foundry_file_search_options_get_case_sensitive (self->options))
        flags |=  G_REGEX_CASELESS;

      if (!(regex = g_regex_new (pattern, flags, 0, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  for (guint i = 0; i < n_matches; i++)
    {
      g_autoptr(FoundryFileSearchMatch) match = NULL;
      g_autoptr(FoundryTextEdit) edit = NULL;
      g_autoptr(GFile) file = NULL;
      g_autofree char *regex_replacement_text = NULL;
      GPtrArray *file_edits;
      const char *replacement_text;
      guint line, line_offset, length;

      match = g_list_model_get_item (self->matches, i);
      if (match == NULL)
        continue;

      file = foundry_file_search_match_dup_file (match);
      line = foundry_file_search_match_get_line (match);
      line_offset = foundry_file_search_match_get_line_offset (match);
      length = foundry_file_search_match_get_length (match);
      replacement_text = self->replacement_text;

      if (regex != NULL)
        {
          g_autofree char *original_text = foundry_file_search_match_dup_text (match);

          if (!(regex_replacement_text = g_regex_replace (regex, original_text, -1, 0, replacement_text, 0, &error)))
            return dex_future_new_for_error (g_steal_pointer (&error));

          replacement_text = regex_replacement_text;
        }

      edit = foundry_text_edit_new (file,
                                    line,
                                    line_offset,
                                    line,
                                    line_offset + length,
                                    replacement_text);

      if (!(file_edits = g_hash_table_lookup (edits_by_file, file)))
        {
          file_edits = g_ptr_array_new_with_free_func (g_object_unref);
          g_hash_table_replace (edits_by_file, g_object_ref (file), file_edits);
        }

      g_ptr_array_add (file_edits, g_object_ref (edit));
    }

  state = g_new0 (ApplyReplacementState, 1);
  state->self = g_object_ref (self);
  state->operation = foundry_operation_new ();
  state->edits_by_file = g_steal_pointer (&edits_by_file);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_file_search_replacement_apply_fiber,
                              state,
                              (GDestroyNotify) apply_replacement_state_free);
}

static void
foundry_file_search_replacement_finalize (GObject *object)
{
  FoundryFileSearchReplacement *self = (FoundryFileSearchReplacement *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->matches);
  g_clear_object (&self->options);
  g_clear_pointer (&self->replacement_text, g_free);

  G_OBJECT_CLASS (foundry_file_search_replacement_parent_class)->finalize (object);
}

static void
foundry_file_search_replacement_class_init (FoundryFileSearchReplacementClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_file_search_replacement_finalize;
}

static void
foundry_file_search_replacement_init (FoundryFileSearchReplacement *self)
{
}
