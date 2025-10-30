/* foundry-cli-builtin-grep.c
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

#include <glib/gi18n-lib.h>

#include "foundry-cli-builtin-private.h"
#include "foundry-cli-command-tree.h"
#include "foundry-command-line.h"
#include "foundry-context.h"
#include "foundry-file-manager.h"
#include "foundry-file-search-options.h"
#include "foundry-file-search-match.h"
#include "foundry-file-search-replacement.h"
#include "foundry-model-manager.h"
#include "foundry-operation.h"
#include "foundry-service.h"
#include "foundry-util-private.h"

static int
foundry_cli_builtin_grep_run (FoundryCommandLine *command_line,
                              const char * const *argv,
                              FoundryCliOptions  *options,
                              DexCancellable     *cancellable)
{
  FoundryObjectSerializerFormat format;
  g_autoptr(FoundryFileManager) file_manager = NULL;
  g_autoptr(FoundryFileSearchOptions) search_options = NULL;
  g_autoptr(FoundryOperation) operation = NULL;
  g_autoptr(FoundryContext) foundry = NULL;
  g_autoptr(GListModel) results = NULL;
  g_autoptr(GError) error = NULL;
  const char *format_arg;
  const char *search_text;
  const char *replacement_text = NULL;
  gboolean recursive = FALSE;
  gboolean case_insensitive = FALSE;
  gboolean use_regex = FALSE;
  gboolean match_whole_words = FALSE;
  int max_matches = 0;
  int context_lines = 0;
  const char * const *required_patterns = NULL;
  const char * const *excluded_patterns = NULL;

  static const FoundryObjectSerializerEntry fields[] = {
    { "uri", N_("Uri") },
    { "line", N_("Line") },
    { "line-offset", N_("Offset") },
    { "length", N_("Length") },
    { "text", N_("Text") },
    { "before-context", N_("Before Context") },
    { "after-context", N_("After Context") },
    { 0 }
  };

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (options != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  if (!argv[1])
    {
      foundry_command_line_printerr (command_line, "usage: %s PATTERN [TARGETS...]\n", argv[0]);
      return EXIT_FAILURE;
    }

  search_text = argv[1];

  if (!(foundry = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    goto handle_error;

  file_manager = foundry_context_dup_file_manager (foundry);
  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (file_manager)), &error))
    goto handle_error;

  search_options = foundry_file_search_options_new ();
  foundry_file_search_options_set_search_text (search_options, search_text);

  foundry_cli_options_get_boolean (options, "recursive", &recursive);
  foundry_cli_options_get_boolean (options, "case-insensitive", &case_insensitive);
  foundry_cli_options_get_boolean (options, "regex", &use_regex);
  foundry_cli_options_get_boolean (options, "word", &match_whole_words);
  foundry_cli_options_get_int (options, "max-matches", &max_matches);
  foundry_cli_options_get_int (options, "context", &context_lines);
  required_patterns = foundry_cli_options_get_string_array (options, "require");
  excluded_patterns = foundry_cli_options_get_string_array (options, "exclude");
  replacement_text = foundry_cli_options_get_string (options, "replace");

  foundry_file_search_options_set_recursive (search_options, recursive);
  foundry_file_search_options_set_case_sensitive (search_options, !case_insensitive);
  foundry_file_search_options_set_use_regex (search_options, use_regex);
  foundry_file_search_options_set_match_whole_words (search_options, match_whole_words);
  foundry_file_search_options_set_max_matches (search_options, max_matches);
  foundry_file_search_options_set_context_lines (search_options, context_lines);
  foundry_file_search_options_set_required_patterns (search_options, required_patterns);
  foundry_file_search_options_set_excluded_patterns (search_options, excluded_patterns);

  /* Add targets - if none specified, use current directory */
  if (argv[2])
    {
      for (guint i = 2; argv[i]; i++)
        {
          g_autoptr(GFile) target = g_file_new_for_commandline_arg_and_cwd (argv[i], foundry_command_line_get_directory (command_line));
          foundry_file_search_options_add_target (search_options, target);
        }
    }
  else
    {
      g_autoptr(GFile) current_dir = g_file_new_for_path (foundry_command_line_get_directory (command_line));
      foundry_file_search_options_add_target (search_options, current_dir);
    }

  operation = foundry_operation_new ();

  if (!(results = dex_await_object (foundry_file_manager_search (file_manager, search_options, operation), &error)))
    goto handle_error;

  /* Wait for all results to be received */
  if (!dex_await (foundry_list_model_await (results), &error))
    goto handle_error;

  if (replacement_text == NULL)
    {
      format_arg = foundry_cli_options_get_string (options, "format");
      format = foundry_object_serializer_format_parse (format_arg);

      if (format == FOUNDRY_OBJECT_SERIALIZER_FORMAT_TEXT)
        {
          guint n_items = g_list_model_get_n_items (results);

          for (guint i = 0; i < n_items; i++)
            {
              g_autoptr(FoundryFileSearchMatch) match = g_list_model_get_item (results, i);
              g_autofree char *text = foundry_file_search_match_dup_text (match);
              g_autoptr(GString) str = g_string_new (text);
              g_autoptr(GFile) file = foundry_file_search_match_dup_file (match);
              g_autofree char *path = g_file_get_path (file);
              guint line = foundry_file_search_match_get_line (match);
              guint line_offset = foundry_file_search_match_get_line_offset (match);
              guint length = foundry_file_search_match_get_length (match);

              foundry_command_line_print (command_line, "%s:%u:%u-%u:",
                                          path, line + 1,
                                          line_offset, line_offset + length);

              if (foundry_command_line_isatty (command_line))
                {
                  if ((gsize)line_offset + (gsize)length <= g_utf8_strlen (text, -1))
                    {
                      const char *end = g_utf8_offset_to_pointer (text, line_offset + length);
                      const char *begin = g_utf8_offset_to_pointer (text, line_offset);
                      gsize end_offset = end - text;
                      gsize begin_offset = begin - text;

                      if (end != NULL && begin != NULL)
                        {
                          g_string_insert (str, end_offset, "\033[0m");
                          g_string_insert (str, begin_offset, "\033[31m");
                        }
                    }
                }

              foundry_command_line_print (command_line, "%s\n", str->str);
            }
        }
      else
        {
          foundry_command_line_print_list (command_line, results, fields, format, FOUNDRY_TYPE_FILE_SEARCH_MATCH);
        }
    }
  else
    {
      g_autoptr(FoundryFileSearchReplacement) replacement = foundry_file_search_replacement_new (foundry, results, search_options, replacement_text);

      if (!dex_await (foundry_file_search_replacement_apply (replacement), &error))
        goto handle_error;
    }

  return EXIT_SUCCESS;

handle_error:

  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_grep (FoundryCliCommandTree *tree)
{
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "grep"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         { "help", 0, 0, G_OPTION_ARG_NONE },
                                         { "format", 'f', 0, G_OPTION_ARG_STRING, NULL, N_("Output format (text, json)"), N_("FORMAT") },
                                         { "recursive", 'r', 0, G_OPTION_ARG_NONE, NULL, N_("Search recursively"), NULL },
                                         { "case-insensitive", 'i', 0, G_OPTION_ARG_NONE, NULL, N_("Case insensitive search"), NULL },
                                         { "regex", 'E', 0, G_OPTION_ARG_NONE, NULL, N_("Use regular expressions"), NULL },
                                         { "word", 'w', 0, G_OPTION_ARG_NONE, NULL, N_("Match whole words"), NULL },
                                         { "max-matches", 'm', 0, G_OPTION_ARG_INT, NULL, N_("Maximum number of matches"), N_("COUNT") },
                                         { "context", 'C', 0, G_OPTION_ARG_INT, NULL, N_("Number of context lines"), N_("LINES") },
                                         { "require", 0, 0, G_OPTION_ARG_STRING_ARRAY, NULL, N_("Required file patterns (shell globs)"), N_("PATTERN") },
                                         { "exclude", 0, 0, G_OPTION_ARG_STRING_ARRAY, NULL, N_("Excluded file patterns (shell globs)"), N_("PATTERN") },
                                         { "replace", 0, 0, G_OPTION_ARG_STRING, NULL, N_("Replace matches with the given text"), N_("TEXT") },
                                         {0}
                                       },
                                       .run = foundry_cli_builtin_grep_run,
                                       .prepare = NULL,
                                       .complete = NULL,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("Search for text patterns in files"),
                                     });
}
