/* foundry-cli-builtin-forge-issues-list.c
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
#include "foundry-forge-issue.h"
#include "foundry-forge-listing.h"
#include "foundry-forge-manager.h"
#include "foundry-forge-project.h"
#include "foundry-forge-user.h"
#include "foundry-forge.h"
#include "foundry-service.h"
#include "foundry-util-private.h"

static int
foundry_cli_builtin_forge_user_run (FoundryCommandLine *command_line,
                                    const char * const *argv,
                                    FoundryCliOptions  *options,
                                    DexCancellable     *cancellable)
{
  FoundryObjectSerializerFormat format;
  g_autoptr(FoundryForgeManager) forge_manager = NULL;
  g_autoptr(FoundryForgeUser) user = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryForge) forge = NULL;
  g_autoptr(GError) error = NULL;
  const char *format_arg;

  static const FoundryObjectSerializerEntry fields[] = {
    { "handle", N_("ID") },
    { "name", N_("Name") },
    { "online-url", N_("URL") },
    { "bio", N_("Bio") },
    { "location", N_("Location") },
    { "avatar-url", N_("Avatar URL") },
    { 0 }
  };

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  if (!(context = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    goto handle_error;

  forge_manager = foundry_context_dup_forge_manager (context);
  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (forge_manager)), &error))
    goto handle_error;

  if (!(forge = foundry_forge_manager_dup_forge (forge_manager)))
    {
      foundry_command_line_printerr (command_line, "No forge active\n");
      return EXIT_FAILURE;
    }

  if (!(user = dex_await_object (foundry_forge_find_user (forge), &error)))
    goto handle_error;

  format_arg = foundry_cli_options_get_string (options, "format");
  format = foundry_object_serializer_format_parse (format_arg);
  foundry_command_line_print_object (command_line, G_OBJECT (user), fields, format);

  return EXIT_SUCCESS;

handle_error:

  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_forge_user (FoundryCliCommandTree *tree)
{
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "forge", "user"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         { "help", 0, 0, G_OPTION_ARG_NONE },
                                         { "format", 'f', 0, G_OPTION_ARG_STRING, NULL, N_("Output format (text, json)"), N_("FORMAT") },
                                         {0}
                                       },
                                       .run = foundry_cli_builtin_forge_user_run,
                                       .prepare = NULL,
                                       .complete = NULL,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("Get info on current forge user"),
                                     });
}
