/* foundry-cli-builtin-find-symbol-at.c
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
#include "foundry-model-manager.h"
#include "foundry-operation.h"
#include "foundry-symbol.h"
#include "foundry-text-document.h"
#include "foundry-text-manager.h"

static void
print_parent_tree (FoundryCommandLine *command_line,
                   GListModel         *symbols)
{
  g_autoptr(GError) error = NULL;
  guint n_symbols;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (G_IS_LIST_MODEL (symbols));

  if (!dex_await (foundry_list_model_await (symbols), &error))
    return;

  n_symbols = g_list_model_get_n_items (symbols);

  for (guint i = 0; i < n_symbols; i++)
    {
      g_autoptr(FoundrySymbol) symbol = g_list_model_get_item (symbols, i);
      g_autofree char *name = NULL;
      g_autoptr(GString) prefix = NULL;
      gboolean is_last = (i == n_symbols - 2);

      name = foundry_symbol_dup_name (symbol);

      if (name == NULL)
        name = g_strdup ("(unnamed)");

      prefix = g_string_new (NULL);
      for (guint j = 0; j < i; j++)
        g_string_append (prefix, "    ");

      foundry_command_line_print (command_line, "%s%s %s\n",
                                  prefix->str,
                                  is_last ? "└──" : "├──",
                                  name);
    }
}

static int
foundry_cli_builtin_find_symbol_at_run (FoundryCommandLine *command_line,
                                        const char * const *argv,
                                        FoundryCliOptions  *options,
                                        DexCancellable     *cancellable)
{
  g_autoptr(FoundryTextManager) text_manager = NULL;
  g_autoptr(FoundryTextDocument) document = NULL;
  g_autoptr(FoundryOperation) operation = NULL;
  g_autoptr(FoundryContext) foundry = NULL;
  g_autoptr(FoundrySymbol) symbol = NULL;
  g_autoptr(GListModel) symbol_path = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree char *name = NULL;
  gboolean print_tree = FALSE;
  guint line;
  guint line_offset;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (options != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  if (!argv[1] || !argv[2] || !argv[3])
    {
      foundry_command_line_printerr (command_line, "usage: %s FILE LINE LINE_OFFSET\n", argv[0]);
      return EXIT_FAILURE;
    }

  if (!(foundry = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    goto handle_error;

  file = g_file_new_for_commandline_arg_and_cwd (argv[1], foundry_command_line_get_directory (command_line));
  text_manager = foundry_context_dup_text_manager (foundry);
  operation = foundry_operation_new ();

  if (!(document = dex_await_object (foundry_text_manager_load (text_manager, file, operation, NULL), &error)))
    goto handle_error;

  line = (guint)g_ascii_strtoull (argv[2], NULL, 10);
  line_offset = (guint)g_ascii_strtoull (argv[3], NULL, 10);

  if (line == 0 || line_offset == 0)
    {
      foundry_command_line_printerr (command_line, "LINE and LINE_OFFSET must be >= 1\n");
      return EXIT_FAILURE;
    }

  line--;
  line_offset--;

  if (!(symbol = dex_await_object (foundry_text_document_find_symbol_at (document, line, line_offset), &error)))
    goto handle_error;

  foundry_cli_options_get_boolean (options, "tree", &print_tree);

  if (!(name = foundry_symbol_dup_name (symbol)))
    name = g_strdup ("(unnamed)");

  if (print_tree)
    {
      if (!(symbol_path = dex_await_object (foundry_symbol_list_to_root (symbol), &error)))
        goto handle_error;

      print_parent_tree (command_line, symbol_path);
    }
  else
    {
      foundry_command_line_print (command_line, "%s\n", name);
    }

  return EXIT_SUCCESS;

handle_error:

  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_find_symbol_at (FoundryCliCommandTree *tree)
{
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "find-symbol-at"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         { "tree", 't', 0, G_OPTION_ARG_NONE, NULL, N_("Print parent symbols as a tree"), NULL },
                                         {0}
                                       },
                                       .run = foundry_cli_builtin_find_symbol_at_run,
                                       .prepare = NULL,
                                       .complete = NULL,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("Find symbol at a specific position in a file"),
                                     });
}
