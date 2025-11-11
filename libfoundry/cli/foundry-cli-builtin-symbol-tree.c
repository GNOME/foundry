/* foundry-cli-builtin-symbol-tree.c
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
print_symbol_tree_recursive (FoundryCommandLine *command_line,
                             FoundrySymbol      *symbol,
                             const char         *prefix,
                             gboolean            is_last)
{
  g_autofree char *name = NULL;
  g_autoptr(GListModel) children = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *new_prefix = NULL;
  guint n_children;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (FOUNDRY_IS_SYMBOL (symbol));

  name = foundry_symbol_dup_name (symbol);

  if (name == NULL)
    name = g_strdup ("(unnamed)");

  foundry_command_line_print (command_line, "%s%s %s\n",
                              prefix,
                              is_last ? "└──" : "├──",
                              name);

  children = dex_await_object (foundry_symbol_list_children (symbol), &error);

  if (error != NULL)
    return;

  if (children == NULL)
    return;

  if (!dex_await (foundry_list_model_await (children), &error))
    return;

  n_children = g_list_model_get_n_items (children);

  if (n_children == 0)
    return;

  new_prefix = g_strdup_printf ("%s%s   ", prefix, is_last ? " " : "│");

  for (guint i = 0; i < n_children; i++)
    {
      g_autoptr(FoundrySymbol) child = g_list_model_get_item (children, i);
      gboolean child_is_last = (i == n_children - 1);

      print_symbol_tree_recursive (command_line, child, new_prefix, child_is_last);
    }
}

static char **
foundry_cli_builtin_symbol_tree_complete (FoundryCommandLine *command_line,
                                          const char         *command,
                                          const GOptionEntry *entry,
                                          FoundryCliOptions  *options,
                                          const char * const *argv,
                                          const char         *current)
{
  return g_strdupv ((char **)FOUNDRY_STRV_INIT ("__FOUNDRY_FILE"));
}

static int
foundry_cli_builtin_symbol_tree_run (FoundryCommandLine *command_line,
                                      const char * const *argv,
                                      FoundryCliOptions  *options,
                                      DexCancellable     *cancellable)
{
  g_autoptr(FoundryTextManager) text_manager = NULL;
  g_autoptr(FoundryTextDocument) document = NULL;
  g_autoptr(FoundryOperation) operation = NULL;
  g_autoptr(FoundryContext) foundry = NULL;
  g_autoptr(GListModel) symbols = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  guint n_symbols;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (options != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  if (!argv[1])
    {
      foundry_command_line_printerr (command_line, "usage: %s FILENAME\n", argv[0]);
      return EXIT_FAILURE;
    }

  if (!(foundry = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    goto handle_error;

  file = g_file_new_for_commandline_arg_and_cwd (argv[1], foundry_command_line_get_directory (command_line));
  text_manager = foundry_context_dup_text_manager (foundry);
  operation = foundry_operation_new ();

  if (!(document = dex_await_object (foundry_text_manager_load (text_manager, file, operation, NULL), &error)))
    goto handle_error;

  if (!(symbols = dex_await_object (foundry_text_document_list_symbols (document), &error)))
    goto handle_error;

  if (!dex_await (foundry_list_model_await (symbols), &error))
    goto handle_error;

  n_symbols = g_list_model_get_n_items (symbols);

  if (n_symbols == 0)
    {
      foundry_command_line_print (command_line, "%s\n", _("No symbols found"));
      return EXIT_SUCCESS;
    }

  for (guint i = 0; i < n_symbols; i++)
    {
      g_autoptr(FoundrySymbol) symbol = g_list_model_get_item (symbols, i);
      gboolean is_last = (i == n_symbols - 1);

      print_symbol_tree_recursive (command_line, symbol, "", is_last);
    }

  return EXIT_SUCCESS;

handle_error:

  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_symbol_tree (FoundryCliCommandTree *tree)
{
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "symbol-tree"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         {0}
                                       },
                                       .run = foundry_cli_builtin_symbol_tree_run,
                                       .prepare = NULL,
                                       .complete = foundry_cli_builtin_symbol_tree_complete,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("List symbols in a file as a tree"),
                                     });
}
