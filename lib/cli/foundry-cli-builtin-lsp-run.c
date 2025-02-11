/* foundry-cli-builtin-lsp-run.c
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

#include <glib/gi18n.h>

#include "foundry-build-manager.h"
#include "foundry-build-pipeline.h"
#include "foundry-cli-builtin-private.h"
#include "foundry-cli-command-tree.h"
#include "foundry-command-line.h"
#include "foundry-context.h"
#include "foundry-lsp-client.h"
#include "foundry-lsp-manager.h"
#include "foundry-lsp-server.h"
#include "foundry-service.h"
#include "foundry-util-private.h"

static int
foundry_cli_builtin_lsp_run_run (FoundryCommandLine *command_line,
                                  const char * const *argv,
                                  FoundryCliOptions  *options,
                                  DexCancellable     *cancellable)
{
  g_autoptr(FoundryBuildManager) build_manager = NULL;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryLspManager) lsp_manager = NULL;
  g_autoptr(FoundryLspServer) best = NULL;
  g_autoptr(FoundryLspClient) client = NULL;
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(FoundryContext) foundry = NULL;
  g_autoptr(GError) error = NULL;
  const char *language;
  guint n_items;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (options != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  language = argv[1];

  if (language == NULL)
    {
      foundry_command_line_printerr (command_line, "usage: %s LANGUAGE\n", argv[0]);
      return EXIT_FAILURE;
    }

  if (!(foundry = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    goto handle_error;

  lsp_manager = foundry_context_dup_lsp_manager (foundry);
  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (lsp_manager)), &error))
    goto handle_error;

  build_manager = foundry_context_dup_build_manager (foundry);
  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (build_manager)), &error))
    goto handle_error;

  pipeline = dex_await_object (foundry_build_manager_load_pipeline (build_manager), NULL);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (lsp_manager));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryLspServer) server = g_list_model_get_item (G_LIST_MODEL (lsp_manager), i);
      g_auto(GStrv) languages = foundry_lsp_server_dup_languages (server);

      if (languages != NULL &&
          g_strv_contains ((const char * const *)languages, language))
        {
          g_set_object (&best, server);
          break;
        }
    }

  if (best == NULL)
    {
      foundry_command_line_printerr (command_line, "No LSP found for language \"%s\"\n", language);
      return EXIT_FAILURE;
    }

  client = dex_await_object (foundry_lsp_server_spawn (best,
                                                       pipeline,
                                                       foundry_command_line_get_stdin (command_line),
                                                       foundry_command_line_get_stdout (command_line)),
                             NULL);
  if (client == NULL)
    goto handle_error;

  if (!dex_await (foundry_lsp_client_await (client), &error))
    goto handle_error;

  return EXIT_SUCCESS;

handle_error:

  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_lsp_run (FoundryCliCommandTree *tree)
{
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "lsp", "run"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         { "help", 0, 0, G_OPTION_ARG_NONE },
                                         {0}
                                       },
                                       .run = foundry_cli_builtin_lsp_run_run,
                                       .prepare = NULL,
                                       .complete = NULL,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("LANGUAGE - Run a language server"),
                                     });
}
