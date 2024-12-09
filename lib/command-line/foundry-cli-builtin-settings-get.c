/* foundry-cli-builtin-settings-get.c
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
#include "foundry-context.h"
#include "foundry-settings.h"
#include "foundry-service.h"
#include "foundry-util-private.h"

static char **
foundry_cli_builtin_settings_get_complete (const char         *command,
                                           const GOptionEntry *entry,
                                           FoundryCliOptions  *options,
                                           const char * const *argv)
{
  return NULL;
}

static void
foundry_cli_builtin_settings_get_help (FoundryCommandLine *command_line)
{
  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));

  foundry_command_line_print (command_line, "Usage:\n");
  foundry_command_line_print (command_line, "  foundry settings get schema key\n");
  foundry_command_line_print (command_line, "\n");
  foundry_command_line_print (command_line, "Options:\n");
  foundry_command_line_print (command_line, "  --help                Show help options\n");
  foundry_command_line_print (command_line, "\n");
}

static int
foundry_cli_builtin_settings_get_run (FoundryCommandLine *command_line,
                                      const char * const *argv,
                                      FoundryCliOptions  *options,
                                      DexCancellable     *cancellable)
{
  g_autoptr(FoundrySettings) settings = NULL;
  g_autoptr(GSettingsSchema) _schema = NULL;
  g_autoptr(FoundryContext) foundry = NULL;
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *schema = NULL;
  g_autofree char *key = NULL;
  g_autofree char *value_string = NULL;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (argv[0] != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  if (foundry_cli_options_help (options))
    {
      foundry_cli_builtin_settings_get_help (command_line);
      return EXIT_SUCCESS;
    }

  if (argv[1] == NULL || argv[2] == NULL)
    {
      foundry_command_line_printerr (command_line, "usage: foundry settings get SCHEMA KEY\n");
      return EXIT_FAILURE;
    }

  schema = g_strdup_printf ("app.devsuite.foundry.%s", argv[1]);
  key = g_strdup (argv[2]);

  if (!(foundry = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    goto handle_error;

  if (!(_schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
                                                   schema, TRUE)))
    {
      foundry_command_line_printerr (command_line, "No such schema \"%s\"\n", schema);
      return EXIT_FAILURE;
    }

  if (!g_settings_schema_has_key (_schema, key))
    {
      foundry_command_line_printerr (command_line, "No such key \"%s\" in schema \"%s\"\n",
                                     key, schema);
      return EXIT_FAILURE;
    }

  settings = foundry_context_load_settings (foundry, schema, NULL);
  variant = foundry_settings_get_value (settings, key);
  value_string = g_variant_print (variant, FALSE);

  foundry_command_line_print (command_line, "%s\n", value_string);

  return EXIT_SUCCESS;

handle_error:

  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_settings_get (FoundryCliCommandTree *tree)
{
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "settings", "get"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         { "help", 0, 0, G_OPTION_ARG_NONE },
                                         {0}
                                       },
                                       .run = foundry_cli_builtin_settings_get_run,
                                       .prepare = NULL,
                                       .complete = foundry_cli_builtin_settings_get_complete,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("SCCHEMA KEY - Get setting"),
                                     });
}
