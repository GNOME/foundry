/* foundry-cli-builtin-forge-switch.c
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
#include "foundry-forge.h"
#include "foundry-forge-manager.h"
#include "foundry-service.h"
#include "foundry-util-private.h"

static int
foundry_cli_builtin_forge_switch_run (FoundryCommandLine *command_line,
                                      const char * const *argv,
                                      FoundryCliOptions  *options,
                                      DexCancellable     *cancellable)
{
  g_autoptr(FoundryForgeManager) forge_manager = NULL;
  g_autoptr(FoundryContext) foundry = NULL;
  g_autoptr(FoundryForge) forge = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  if (argv[1] == NULL)
    {
      foundry_command_line_printerr (command_line, "usage: %s FORGE_ID\n", argv[0]);
      return EXIT_FAILURE;
    }

  if (!(foundry = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    goto handle_error;

  forge_manager = foundry_context_dup_forge_manager (foundry);
  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (forge_manager)), &error))
    goto handle_error;

  if (!(forge = foundry_forge_manager_find_by_id (forge_manager, argv[1])))
    {
      foundry_command_line_printerr (command_line, "No such forge `%s`\n", argv[1]);
      return EXIT_FAILURE;
    }

  foundry_forge_manager_set_forge (forge_manager, forge);

  foundry_command_line_print (command_line, "Switched forge to `%s`\n", argv[1]);

  return EXIT_SUCCESS;

handle_error:

  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_forge_switch (FoundryCliCommandTree *tree)
{
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "forge", "switch"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         { "help", 0, 0, G_OPTION_ARG_NONE },
                                         {0}
                                       },
                                       .run = foundry_cli_builtin_forge_switch_run,
                                       .prepare = NULL,
                                       .complete = NULL,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("FORGE - Switch forge to FORGE"),
                                     });
}
