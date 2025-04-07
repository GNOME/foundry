/* foundry-cli-builtin-config-list.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <glib/gi18n.h>

#include "foundry-cli-builtin-private.h"
#include "foundry-cli-command-tree.h"
#include "foundry-command-line.h"
#include "foundry-context.h"
#include "foundry-documentation.h"
#include "foundry-documentation-manager.h"
#include "foundry-documentation-query.h"
#include "foundry-service.h"
#include "foundry-util-private.h"

static int
foundry_cli_builtin_doc_query_run (FoundryCommandLine *command_line,
                                   const char * const *argv,
                                   FoundryCliOptions  *options,
                                   DexCancellable     *cancellable)
{
  FoundryObjectSerializerFormat format;
  g_autoptr(FoundryDocumentationManager) documentation_manager = NULL;
  g_autoptr(FoundryDocumentationQuery) query = NULL;
  g_autoptr(FoundryFutureListModel) results = NULL;
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(FoundryContext) foundry = NULL;
  g_autoptr(GString) str = NULL;
  g_autoptr(GError) error = NULL;
  const char *format_arg;

  static const FoundryObjectSerializerEntry fields[] = {
    { "title", N_("Title") },
    { "uri", N_("Location") },
    { 0 }
  };

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (options != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  if (argv[1] == NULL)
    {
      foundry_command_line_printerr (command_line, "usage: %s SEARCH_TEXT\n", argv[0]);
      return EXIT_FAILURE;
    }

  str = g_string_new (argv[1]);
  for (guint i = 2; argv[i]; i++)
    g_string_append_printf (str, " %s", argv[i]);

  if (!(foundry = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    goto handle_error;

  documentation_manager = foundry_context_dup_documentation_manager (foundry);
  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (documentation_manager)), &error))
    goto handle_error;

  query = foundry_documentation_query_new ();
  foundry_documentation_query_set_keyword (query, str->str);

  if (!(results = dex_await_object (foundry_documentation_manager_query (documentation_manager, query), &error)) ||
      !dex_await (foundry_future_list_model_await (results), &error))
    goto handle_error;

  format_arg = foundry_cli_options_get_string (options, "format");
  format = foundry_object_serializer_format_parse (format_arg);
  foundry_command_line_print_list (command_line, G_LIST_MODEL (results), fields, format, FOUNDRY_TYPE_DOCUMENTATION);

  return EXIT_SUCCESS;

handle_error:

  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_doc_query (FoundryCliCommandTree *tree)
{
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "doc", "query"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         { "help", 0, 0, G_OPTION_ARG_NONE },
                                         { "format", 'f', 0, G_OPTION_ARG_STRING, NULL, N_("Output format (text, json)"), N_("FORMAT") },
                                         {0}
                                       },
                                       .run = foundry_cli_builtin_doc_query_run,
                                       .prepare = NULL,
                                       .complete = NULL,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("Query documentation"),
                                     });
}
