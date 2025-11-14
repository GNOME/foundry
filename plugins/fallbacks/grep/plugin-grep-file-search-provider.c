/* plugin-grep-file-search-provider.c
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

#include "plugin-grep-file-search-match.h"
#include "plugin-grep-file-search-provider.h"

struct _PluginGrepFileSearchProvider
{
  FoundryFileSearchProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginGrepFileSearchProvider, plugin_grep_file_search_provider, FOUNDRY_TYPE_FILE_SEARCH_PROVIDER)

typedef struct
{
  DexPromise *promise;
  GError **error;
  char *ret;
  gsize *len;
} ReadLine;

static void
_g_data_input_stream_read_line_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  ReadLine *state = user_data;
  state->ret = g_data_input_stream_read_line_finish (G_DATA_INPUT_STREAM (object), result, state->len, state->error);
  dex_promise_resolve_boolean (state->promise, TRUE);
}

static char *
_g_data_input_stream_read_line (GDataInputStream  *stream,
                                gsize             *len,
                                GError           **error)
{
  ReadLine state = { dex_promise_new (), error, NULL, len };
  g_data_input_stream_read_line_async (stream,
                                       G_PRIORITY_DEFAULT,
                                       NULL,
                                       _g_data_input_stream_read_line_cb,
                                       &state);
  dex_await (DEX_FUTURE (state.promise), NULL);
  return state.ret;
}

typedef struct
{
  GListStore *store;
  GListStore *batch;
} AddBatchInMain;

static void
add_batch_in_main_free (AddBatchInMain *state)
{
  g_clear_object (&state->store);
  g_clear_object (&state->batch);
  g_free (state);
}

static gboolean
add_batch_in_main_cb (gpointer data)
{
  AddBatchInMain *state = data;
  g_list_store_append (state->store, state->batch);
  return G_SOURCE_REMOVE;
}

static void
add_batch_in_main (GListStore *store,
                   GListStore *batch)
{
  AddBatchInMain *state;

  state = g_new0 (AddBatchInMain, 1);
  state->store = g_object_ref (store);
  state->batch = batch;

  /* Priority needs to be higher than whatever our dispatch
   * notification scheme is otherwise if the main thread awaits
   * completion of the fiber, it could get notified before these
   * have really been added to the list model.
   *
   * This could be improved with a thread-safe liststore
   * replacement that we could "await" to synchronize.
   */
  g_idle_add_full (G_PRIORITY_HIGH,
                   add_batch_in_main_cb,
                   state,
                   (GDestroyNotify) add_batch_in_main_free);
}

typedef struct
{
  char *filename;
  GString *before;
  GString *after;
  GString *match;
  char *line_content;
  guint line;
  guint line_offset;
  guint length;
  gboolean seen_match;
  guint counter;
} MatchBuilder;

static MatchBuilder *
match_builder_new (void)
{
  MatchBuilder *state;

  state = g_new0 (MatchBuilder, 1);
  state->before = g_string_new (NULL);
  state->after = g_string_new (NULL);
  state->match = g_string_new (NULL);

  return state;
}

static void
match_builder_free (MatchBuilder *state)
{
  g_clear_pointer (&state->filename, g_free);
  g_string_free (state->before, TRUE);
  g_string_free (state->after, TRUE);
  g_string_free (state->match, TRUE);
  g_clear_pointer (&state->line_content, g_free);
  g_free (state);
}

static void
match_builder_flush_match (MatchBuilder *state,
                            GListStore   *store,
                            guint         line_offset,
                            guint         length)
{
  g_autoptr(FoundryFileSearchMatch) match = NULL;
  g_autoptr(GFile) file = NULL;

  g_assert (state != NULL);
  g_assert (G_IS_LIST_STORE (store));

  if (state->filename == NULL)
    return;

  file = g_file_new_for_path (state->filename);
  match = plugin_grep_file_search_match_new (file,
                                             state->line - 1,
                                             line_offset,
                                             length,
                                             g_strndup (state->before->str, state->before->len),
                                             g_strndup (state->match->str, state->match->len),
                                             g_strndup (state->after->str, state->after->len));
  g_list_store_append (store, match);

  state->counter++;
}

static void
match_builder_reset (MatchBuilder *state)
{
  g_assert (state != NULL);

  g_string_truncate (state->before, 0);
  g_string_truncate (state->after, 0);
  g_string_truncate (state->match, 0);
  g_clear_pointer (&state->filename, g_free);
  g_clear_pointer (&state->line_content, g_free);

  state->line = 0;
  state->line_offset = 0;
  state->length = 0;
  state->seen_match = FALSE;
}

static void
match_builder_set_filename (MatchBuilder *state,
                            const char   *begin,
                            const char   *endptr)
{
  g_clear_pointer (&state->filename, g_free);
  state->filename = g_strndup (begin, endptr - begin);
}

static void
match_builder_add_context (MatchBuilder *builder,
                           const char   *text,
                           const char   *endptr)
{
  GString *str = builder->seen_match ? builder->before : builder->after;

  if (str->len)
    g_string_append_c (str, '\n');
  g_string_append_len (str, text, endptr - text);
}

static void
match_builder_set_match (MatchBuilder *builder,
                         const char   *text,
                         const char   *endptr)
{
  g_string_truncate (builder->match, 0);
  g_string_append_len (builder->match, text, endptr - text);
}

static void
match_builder_find_all_matches (MatchBuilder *builder,
                                 GListStore   *store,
                                 GRegex       *regex,
                                 const char   *line_content,
                                 gsize         line_content_len,
                                 const char   *search_text,
                                 gsize         search_text_len,
                                 gboolean      case_sensitive,
                                 const char   *search_down)
{
  g_assert (builder != NULL);
  g_assert (G_IS_LIST_STORE (store));
  g_assert (line_content != NULL);

  if (regex != NULL)
    {
      g_autoptr(GMatchInfo) match_info = NULL;

      if (g_regex_match_all_full (regex, line_content, line_content_len, 0, 0, &match_info, NULL))
        {
          while (g_match_info_matches (match_info))
            {
              int match_start = 0;
              int match_end = 0;

              if (g_match_info_fetch_pos (match_info, 0, &match_start, &match_end))
                {
                  if (match_start >= 0 && match_end > match_start && match_end <= line_content_len)
                    {
                      guint line_offset = g_utf8_strlen (line_content, match_start);
                      guint length = g_utf8_strlen (line_content + match_start, match_end - match_start);

                      match_builder_flush_match (builder, store, line_offset, length);
                    }
                }

              if (!g_match_info_next (match_info, NULL))
                break;
            }
        }
    }
  else
    {
      const char *haystack = line_content;
      gsize haystack_len = line_content_len;
      const char *needle;
      gsize needle_len = search_text_len;
      g_autofree char *haystack_down = NULL;
      gsize cumulative_offset = 0;

      if (!case_sensitive)
        {
          haystack_down = g_utf8_strdown (line_content, -1);
          haystack = haystack_down;
          haystack_len = strlen (haystack);
          needle = search_down;
        }
      else
        {
          needle = search_text;
        }

      while (haystack_len >= needle_len)
        {
          const char *match;
          gsize match_pos;

          match = memmem (haystack, haystack_len, needle, needle_len);

          if (match == NULL)
            break;

          match_pos = match - haystack;

          if (case_sensitive)
            {
              gsize absolute_match_pos = cumulative_offset + match_pos;
              guint line_offset = g_utf8_strlen (line_content, absolute_match_pos);
              guint length = g_utf8_strlen (line_content + absolute_match_pos, needle_len);

              match_builder_flush_match (builder, store, line_offset, length);
            }
          else
            {
              /* For case-insensitive, find character position */
              /* We need to map from haystack_down position back to line_content */
              /* Since both are UTF-8, character positions should align */
              gsize absolute_match_pos_down = cumulative_offset + match_pos;
              guint char_pos = g_utf8_strlen (haystack_down, absolute_match_pos_down);
              guint length = g_utf8_strlen (search_text, -1);

              match_builder_flush_match (builder, store, char_pos, length);
            }

          /* Move past this match */
          cumulative_offset += match_pos + needle_len;
          haystack += match_pos + needle_len;
          haystack_len -= match_pos + needle_len;
        }
    }
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MatchBuilder, match_builder_free)

static gboolean
read_to_null (const char **iter,
              const char  *endptr)
{
  *iter = memchr (*iter, '\0', endptr - *iter);
  return *iter && *iter[0] == 0;
}

static gboolean
skip_byte (const char **iter,
           const char  *endptr)
{
  *iter = (*iter) + 1;
  return *iter < endptr;
}

static gboolean
read_uint (const char **iter,
           guint       *value)
{
  guint64 v;

  if (!g_ascii_isdigit (**iter))
    return FALSE;

  v = g_ascii_strtoull (*iter, (char **)iter, 10);
  if (v > G_MAXUINT)
    return FALSE;

  *value = v;
  return TRUE;
}

static DexFuture *
plugin_grep_file_search_provider_search_fiber (FoundryFileSearchOptions *options,
                                               FoundryOperation         *operation,
                                               GListStore               *flatten_store)
{
  const guint BATCH_LIMIT = 100;

  g_autoptr(GListModel) targets = NULL;
  g_autoptr(GListStore) batch = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GDataInputStream) data_stream = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) argv = NULL;
  g_autoptr(GRegex) regex = NULL;
  g_autoptr(MatchBuilder) builder = NULL;
  g_auto(GStrv) include = NULL;
  g_auto(GStrv) exclude = NULL;
  g_autofree char *search_text = NULL;
  g_autofree char *escaped_text = NULL;
  g_autofree char *context_arg = NULL;
  g_autofree char *search_down = NULL;
  g_autofree char *max_matches_flag = NULL;
  GRegexCompileFlags regex_flags = G_REGEX_OPTIMIZE;
  GInputStream *stdout_stream = NULL;
  gboolean use_regex;
  gboolean case_sensitive;
  gsize search_text_len;
  gsize line_len;
  guint max_matches;
  guint context_lines;
  guint n_targets;
  char *lineptr;

  g_assert (FOUNDRY_IS_FILE_SEARCH_OPTIONS (options));
  g_assert (FOUNDRY_IS_OPERATION (operation));
  g_assert (G_IS_LIST_STORE (flatten_store));

  targets = foundry_file_search_options_list_targets (options);
  batch = g_list_store_new (FOUNDRY_TYPE_FILE_SEARCH_MATCH);

  /* Get search options */
  search_text = foundry_file_search_options_dup_search_text (options);
  max_matches = foundry_file_search_options_get_max_matches (options);
  context_lines = foundry_file_search_options_get_context_lines (options);
  include = foundry_file_search_options_dup_required_patterns (options);
  exclude = foundry_file_search_options_dup_excluded_patterns (options);

  if (search_text == NULL || search_text[0] == '\0')
    return dex_future_new_true ();

  search_down = g_utf8_strdown (search_text, -1);
  search_text_len = strlen (search_text);

  /* Setup regex and search options once */
  use_regex = foundry_file_search_options_get_use_regex (options);
  case_sensitive = foundry_file_search_options_get_case_sensitive (options);

  if (use_regex)
    {
      if (!case_sensitive)
        regex_flags |= G_REGEX_CASELESS;

      if (!(regex = g_regex_new (search_text, regex_flags, 0, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  /* Build grep command arguments */
  argv = g_ptr_array_new ();
  g_ptr_array_add (argv, (gpointer)"grep");
  g_ptr_array_add (argv, (gpointer)"-I");      /* Ignore binary files */
  g_ptr_array_add (argv, (gpointer)"-H");      /* Always print filename */
  g_ptr_array_add (argv, (gpointer)"-n");      /* Print line numbers */
  g_ptr_array_add (argv, (gpointer)"--null");  /* Use null separator */

  /* Add context lines if requested */
  context_arg = g_strdup_printf ("-C%u", context_lines);
  g_ptr_array_add (argv, context_arg);

  /* Add case sensitivity */
  if (!foundry_file_search_options_get_case_sensitive (options))
    g_ptr_array_add (argv, (gpointer)"-i");

  /* Add whole word matching */
  if (foundry_file_search_options_get_match_whole_words (options))
    g_ptr_array_add (argv, (gpointer)"-w");

  /* Add max matches flag */
  if (max_matches > 0)
    {
      max_matches_flag = g_strdup_printf ("--max-count=%u", max_matches);
      g_ptr_array_add (argv, (gpointer)max_matches_flag);
    }

  /* Add recursive flag */
  if (foundry_file_search_options_get_recursive (options))
    g_ptr_array_add (argv, (gpointer)"-r");

  /* Setup exclude filtering */
  if (exclude != NULL)
    {
      for (guint i = 0; exclude[i]; i++)
        {
          g_ptr_array_add (argv, (gpointer)"--exclude");
          g_ptr_array_add (argv, exclude[i]);
        }
    }

  /* Setup include (required) filtering */
  if (include != NULL)
    {
      for (guint i = 0; include[i]; i++)
        {
          g_ptr_array_add (argv, (gpointer)"--include");
          g_ptr_array_add (argv, include[i]);
        }
    }

  /* Setup PCRE support in grep do match our regex elsewhere */
  if (foundry_file_search_options_get_use_regex (options))
    g_ptr_array_add (argv, (gpointer)"-P");
  else
    g_ptr_array_add (argv, (gpointer)"-F");

  g_ptr_array_add (argv, (gpointer)"-e");
  g_ptr_array_add (argv, search_text);

#if 0
  /* XXX: This works with git grep only.
   *
   * Avoid pathological lines up front before reading them into
   * the UI process memory space.
   *
   * Note that we do this *after* our query match because it causes
   * grep to have to look at every line up to it. So to do this in
   * reverse order is incredibly slow.
   */
  g_ptr_array_add (argv, (gpointer)"--and");
  g_ptr_array_add (argv, (gpointer)"-e");
  g_ptr_array_add (argv, (gpointer)"^.{0,1024}$");
#endif

  /* Add target files/directories */
  n_targets = g_list_model_get_n_items (targets);
  for (guint i = 0; i < n_targets; i++)
    {
      g_autoptr(GFile) target = g_list_model_get_item (targets, i);
      g_autofree char *path = g_file_get_path (target);
      if (path != NULL)
        g_ptr_array_add (argv, g_steal_pointer (&path));
    }

  g_ptr_array_add (argv, NULL);

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE);

  if (!(subprocess = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) argv->pdata, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  stdout_stream = g_subprocess_get_stdout_pipe (subprocess);
  data_stream = g_data_input_stream_new (g_object_ref (G_INPUT_STREAM (stdout_stream)));
  g_data_input_stream_set_newline_type (data_stream, G_DATA_STREAM_NEWLINE_TYPE_LF);

  builder = match_builder_new ();

  while ((lineptr = _g_data_input_stream_read_line (data_stream, &line_len, &error)))
    {
      g_autofree char *line = lineptr;
      const char *endptr;
      const char *iter;
      gboolean is_context;
      guint lineno;

      iter = line;
      endptr = line + line_len;

      if (line_len == 2 && memcmp (line, "--", 2) == 0)
        {
          match_builder_reset (builder);

          if (g_list_model_get_n_items (G_LIST_MODEL (batch)) >= BATCH_LIMIT)
            {
              add_batch_in_main (flatten_store, g_steal_pointer (&batch));
              batch = g_list_store_new (FOUNDRY_TYPE_FILE_SEARCH_MATCH);
            }

          if (max_matches > 0 && builder->counter >= max_matches)
            break;

          continue;
        }

      if (!read_to_null (&iter, endptr))
        continue;

      match_builder_set_filename (builder, line, iter);

      if (!skip_byte (&iter, endptr))
        continue;

      if (!read_uint (&iter, &lineno) || lineno == 0)
        continue;

      builder->line = lineno;

      if (!(*iter == '-' || *iter == ':'))
        continue;

      is_context = *iter == '-';

      if (!skip_byte (&iter, endptr))
        continue;

      if (*iter == ':')
        builder->seen_match = TRUE;

      if (is_context)
        match_builder_add_context (builder, iter, endptr);
      else
        {
          g_autofree char *freeme = NULL;
          const char *line_content;
          gsize line_content_len;
          gsize size_to_end = line_len - (iter - line);

          match_builder_set_match (builder, iter, endptr);

          if (g_utf8_validate_len (iter, size_to_end, NULL))
            {
              line_content = iter;
              line_content_len = size_to_end;
            }
          else
            {
              freeme = g_utf8_make_valid (iter, size_to_end);
              line_content = freeme;
              line_content_len = strlen (line_content);
            }

          /* Store line content for finding all matches */
          g_clear_pointer (&builder->line_content, g_free);
          builder->line_content = g_strndup (line_content, line_content_len);

          /* Find all matches on this line */
          match_builder_find_all_matches (builder,
                                          batch,
                                          regex,
                                          line_content,
                                          line_content_len,
                                          search_text,
                                          search_text_len,
                                          case_sensitive,
                                          search_down);
        }
    }

  match_builder_reset (builder);

  if (g_list_model_get_n_items (G_LIST_MODEL (batch)) > 0)
    add_batch_in_main (flatten_store, g_steal_pointer (&batch));

  if (error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  dex_await (dex_timeout_new_msec (10), NULL);

  return dex_future_new_true ();
}

static DexFuture *
plugin_grep_file_search_provider_search (FoundryFileSearchProvider *provider,
                                         FoundryFileSearchOptions  *options,
                                         FoundryOperation          *operation)
{
  g_autoptr(FoundryFileSearchOptions) copy = NULL;
  g_autoptr(GListModel) flatten = NULL;
  g_autoptr(GListStore) store = NULL;
  DexFuture *future;

  g_assert (PLUGIN_IS_GREP_FILE_SEARCH_PROVIDER (provider));
  g_assert (FOUNDRY_IS_FILE_SEARCH_OPTIONS (options));

  copy = foundry_file_search_options_copy (options);

  /* We run the search on a thread-pool fiber but give the caller back
   * a GListModel quickly which will contain those results. They can await
   * for the full completion using foundry_list_model_await().
   */
  store = g_list_store_new (G_TYPE_LIST_MODEL);
  flatten = foundry_flatten_list_model_new (g_object_ref (G_LIST_MODEL (store)));
  future = foundry_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                                    G_CALLBACK (plugin_grep_file_search_provider_search_fiber),
                                    3,
                                    FOUNDRY_TYPE_FILE_SEARCH_OPTIONS, copy,
                                    FOUNDRY_TYPE_OPERATION, operation,
                                    G_TYPE_LIST_STORE, store);
  foundry_list_model_set_future (flatten, g_steal_pointer (&future));

  return dex_future_new_take_object (g_steal_pointer (&flatten));
}

static void
plugin_grep_file_search_provider_class_init (PluginGrepFileSearchProviderClass *klass)
{
  FoundryFileSearchProviderClass *file_search_provider_class = FOUNDRY_FILE_SEARCH_PROVIDER_CLASS (klass);

  file_search_provider_class->search = plugin_grep_file_search_provider_search;
}

static void
plugin_grep_file_search_provider_init (PluginGrepFileSearchProvider *self)
{
}
