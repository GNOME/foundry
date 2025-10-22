/* foundry-cli-builtin-secret-get-api-key.c
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
#include "foundry-context.h"
#include "foundry-secret-service.h"
#include "foundry-service.h"
#include "foundry-util-private.h"

static void
foundry_cli_builtin_secret_get_api_key_help (FoundryCommandLine *command_line)
{
  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));

  foundry_command_line_print (command_line, "Usage:\n");
  foundry_command_line_print (command_line, "  foundry secret get-api-key HOSTNAME SERVICE\n");
  foundry_command_line_print (command_line, "\n");
  foundry_command_line_print (command_line, "Options:\n");
  foundry_command_line_print (command_line, "  --help                Show help options\n");
  foundry_command_line_print (command_line, "\n");
  foundry_command_line_print (command_line, "Description:\n");
  foundry_command_line_print (command_line, "  Retrieve an API key for a service on a specific hostname.\n");
  foundry_command_line_print (command_line, "  The API key will be retrieved from the system's\n");
  foundry_command_line_print (command_line, "  secret storage and printed to stdout.\n");
  foundry_command_line_print (command_line, "\n");
  foundry_command_line_print (command_line, "Examples:\n");
  foundry_command_line_print (command_line, "  foundry secret get-api-key gitlab.com gitlab\n");
  foundry_command_line_print (command_line, "\n");
}

static int
foundry_cli_builtin_secret_get_api_key_run (FoundryCommandLine *command_line,
                                           const char * const *argv,
                                           FoundryCliOptions  *options,
                                           DexCancellable     *cancellable)
{
  g_autoptr(FoundrySecretService) secret_service = NULL;
  g_autoptr(FoundryContext) foundry = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *api_key = NULL;
  const char *hostname;
  const char *service;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (argv[0] != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  if (foundry_cli_options_help (options))
    {
      foundry_cli_builtin_secret_get_api_key_help (command_line);
      return EXIT_SUCCESS;
    }

  if (argv[1] == NULL || argv[2] == NULL)
    {
      foundry_command_line_printerr (command_line, "usage: foundry secret get-api-key HOSTNAME SERVICE\n");
      return EXIT_FAILURE;
    }

  hostname = argv[1];
  service = argv[2];

  if (foundry_str_empty0 (hostname))
    {
      foundry_command_line_printerr (command_line, "hostname cannot be empty\n");
      return EXIT_FAILURE;
    }

  if (foundry_str_empty0 (service))
    {
      foundry_command_line_printerr (command_line, "service cannot be empty\n");
      return EXIT_FAILURE;
    }

  if (!(foundry = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    goto handle_error;

  if (!(secret_service = foundry_context_dup_secret_service (foundry)))
    {
      foundry_command_line_printerr (command_line, "Failed to get secret service\n");
      return EXIT_FAILURE;
    }

  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (secret_service)), &error))
    goto handle_error;

  if (!(api_key = dex_await_string (foundry_secret_service_lookup_api_key (secret_service, hostname, service), &error)))
    goto handle_error;

  if (api_key == NULL)
    {
      foundry_command_line_printerr (command_line, "No API key found for %s on %s\n", service, hostname);
      return EXIT_FAILURE;
    }

  foundry_command_line_print (command_line, "%s\n", api_key);

  return EXIT_SUCCESS;

handle_error:

  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_secret_get_api_key (FoundryCliCommandTree *tree)
{
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "secret", "get-api-key"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         { "help", 0, 0, G_OPTION_ARG_NONE },
                                         {0}
                                       },
                                       .run = foundry_cli_builtin_secret_get_api_key_run,
                                       .prepare = NULL,
                                       .complete = NULL,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("HOSTNAME SERVICE - Retrieve API key for service"),
                                     });
}
