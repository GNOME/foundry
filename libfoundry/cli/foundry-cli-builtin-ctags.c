/* foundry-cli-builtin-ctags.c
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

#ifndef HAVE_PLUGIN_CTAGS
# error "Not compiled with ctags support"
#endif

#include <unistd.h>

#include <glib/gi18n-lib.h>

#include "foundry-cli-builtin-private.h"
#include "foundry-cli-command-tree.h"
#include "foundry-command-line.h"
#include "foundry-context.h"
#include "foundry-service.h"
#include "foundry-util-private.h"

#include "plugins/ctags/plugin-ctags-file.h"
#include "plugins/ctags/plugin-ctags-service.h"

static int
foundry_cli_builtin_ctags_run (FoundryCommandLine *command_line,
                               const char * const *argv,
                               FoundryCliOptions  *options,
                               DexCancellable     *cancellable)
{
  g_autoptr(PluginCtagsService) service = NULL;
  g_autoptr(PluginCtagsFile) ctags_file = NULL;
  g_autoptr(FoundryContext) foundry = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  gsize size;
  const guint8 *data;
  int stdout_fd;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (options != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  if (!argv[1])
    {
      foundry_command_line_printerr (command_line, "usage: %s FILE\n", argv[0]);
      return EXIT_FAILURE;
    }

  if (!(foundry = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    goto handle_error;

  service = foundry_context_dup_service_typed (foundry, PLUGIN_TYPE_CTAGS_SERVICE);

  if (service == NULL)
    {
      foundry_command_line_printerr (command_line, "%s\n", _("ctags service not available"));
      return EXIT_FAILURE;
    }

  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (service)), &error))
    goto handle_error;

  file = g_file_new_for_commandline_arg_and_cwd (argv[1], foundry_command_line_get_directory (command_line));

  if (!(ctags_file = dex_await_object (plugin_ctags_service_index (service, file, NULL), &error)))
    goto handle_error;

  bytes = plugin_ctags_file_dup_bytes (ctags_file);

  if (bytes == NULL)
    {
      foundry_command_line_printerr (command_line, "%s\n", _("no ctags data available"));
      return EXIT_FAILURE;
    }

  size = g_bytes_get_size (bytes);
  data = g_bytes_get_data (bytes, NULL);
  stdout_fd = foundry_command_line_get_stdout (command_line);

  if (write (stdout_fd, data, size) != (ssize_t)size)
    {
      foundry_command_line_printerr (command_line, "%s\n", _("failed to write ctags data"));
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;

handle_error:

  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_ctags (FoundryCliCommandTree *tree)
{
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "ctags"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         {0}
                                       },
                                       .run = foundry_cli_builtin_ctags_run,
                                       .prepare = NULL,
                                       .complete = NULL,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("Index a file and output ctags data"),
                                     });
}
