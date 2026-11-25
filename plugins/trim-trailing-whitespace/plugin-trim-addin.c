/* plugin-trim-addin.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "plugin-trim-addin.h"

struct _PluginTrimAddin
{
  FoundryTextDocumentAddin parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginTrimAddin, plugin_trim_addin, FOUNDRY_TYPE_TEXT_DOCUMENT_ADDIN)

DEX_DEFINE_CLOSURE_TYPE (Trim, trim,
                         DEX_DEFINE_CLOSURE_OBJECT (FoundryTextDocument, document),
                         DEX_DEFINE_CLOSURE_OBJECT (GFile, file))

static DexFuture *
trim_fiber (gpointer data)
{
  Trim *state = data;
  g_autoptr(FoundryTextSettings) settings = NULL;
  g_autoptr(FoundryTextBuffer) buffer = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryVcsManager) manager = NULL;
  g_autoptr(FoundryVcs) vcs = NULL;
  g_autoptr(FoundryVcsFile) file = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (state != NULL);

  if (!(settings = dex_await_object (foundry_text_document_load_settings (state->document), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!foundry_text_settings_get_trim_trailing_whitespace (settings))
    return dex_future_new_true ();

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (state->document));
  manager = foundry_context_dup_vcs_manager (context);

  if (manager == NULL ||
      !(vcs = foundry_vcs_manager_dup_vcs (manager)) ||
      FOUNDRY_IS_NO_VCS (vcs))
    return dex_future_new_true ();

  if (!(file = dex_await_object (foundry_vcs_find_file (vcs, state->file), NULL)))
    return dex_future_new_true ();

  buffer = foundry_text_document_dup_buffer (state->document);

  for (;;)
    {
      g_autoptr(GBytes) contents = foundry_text_buffer_dup_contents (buffer);
      g_autoptr(FoundryVcsLineChanges) changes = NULL;
      g_autoptr(GPtrArray) edits = g_ptr_array_new_with_free_func (g_object_unref);
      gint64 change_count = foundry_text_buffer_get_change_count (buffer);
      const char *text;
      gsize length;
      guint line = 0;

      if (!(changes = dex_await_object (foundry_vcs_describe_line_changes (vcs, file, contents), NULL)))
        return dex_future_new_true ();

      if (change_count != foundry_text_buffer_get_change_count (buffer))
        continue;

      text = g_bytes_get_data (contents, &length);

      for (gsize start = 0; start < length; line++)
        {
          gsize end = start;
          gsize trimmed;
          gsize next;

          while (end < length && text[end] != '\n')
            end++;

          next = end < length ? end + 1 : end;
          if (end < length && end > start && text[end - 1] == '\r')
            end--;

          trimmed = end;
          while (trimmed > start && (text[trimmed - 1] == ' ' || text[trimmed - 1] == '\t'))
            trimmed--;

          if (trimmed < end &&
              (foundry_vcs_line_changes_query_line (changes, line) & (FOUNDRY_VCS_LINE_ADDED | FOUNDRY_VCS_LINE_CHANGED)))
            g_ptr_array_add (edits,
                             foundry_text_edit_new (state->file,
                                                    line,
                                                    g_utf8_strlen (text + start, trimmed - start),
                                                    line,
                                                    g_utf8_strlen (text + start, end - start),
                                                    ""));

          start = next;
        }

      for (guint i = edits->len; i > 0; i--)
        {
          if (!foundry_text_buffer_apply_edit (buffer, g_ptr_array_index (edits, i - 1)))
            return dex_future_new_reject (G_IO_ERROR,
                                          G_IO_ERROR_FAILED,
                                          "Failed to trim trailing whitespace");
        }

      return dex_future_new_true ();
    }
}

static DexFuture *
plugin_trim_addin_pre_save_as (FoundryTextDocumentAddin *addin,
                               GFile                    *file)
{
  Trim *state;

  g_assert (PLUGIN_IS_TRIM_ADDIN (addin));
  g_assert (G_IS_FILE (file));

  state = trim_new ();
  state->document = foundry_text_document_addin_dup_document (addin);
  state->file = g_object_ref (file);

  return dex_scheduler_spawn (NULL,
                              0,
                              trim_fiber,
                              state,
                              (GDestroyNotify) trim_free);
}

static void
plugin_trim_addin_class_init (PluginTrimAddinClass *klass)
{
  FoundryTextDocumentAddinClass *addin_class = FOUNDRY_TEXT_DOCUMENT_ADDIN_CLASS (klass);

  addin_class->pre_save_as = plugin_trim_addin_pre_save_as;
}

static void
plugin_trim_addin_init (PluginTrimAddin *self)
{
}
