/* foundry-cli-builtin-mcp.c
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
#include <glib-unix.h>

#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

#include "foundry-cli-builtin-private.h"
#include "foundry-cli-command-tree.h"
#include "foundry-command-line.h"
#include "foundry-context.h"
#include "foundry-mcp-server.h"
#include "foundry-util-private.h"

static int
foundry_cli_builtin_mcp_run (FoundryCommandLine *command_line,
                             const char * const *argv,
                             FoundryCliOptions  *options,
                             DexCancellable     *cancellable)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryMcpServer) server = NULL;
  g_autoptr(GIOStream) stream = NULL;
  g_autoptr(GInputStream) input = NULL;
  g_autoptr(GOutputStream) output = NULL;
  g_autoptr(GError) error = NULL;
  int stdin_fd;
  int stdout_fd;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (options != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  stdin_fd = foundry_command_line_get_stdin (command_line);
  stdout_fd = foundry_command_line_get_stdout (command_line);

  if (stdin_fd < 0 || stdout_fd < 0)
    {
      foundry_command_line_printerr (command_line, "%s\n", _("Failed to get stdin/stdout file descriptors"));
      return EXIT_FAILURE;
    }

  if (!g_unix_set_fd_nonblocking (stdin_fd, TRUE, &error) ||
      !g_unix_set_fd_nonblocking (stdout_fd, TRUE, &error))
    {
      foundry_command_line_printerr (command_line, "%s\n", error->message);
      return EXIT_FAILURE;
    }

  input = g_unix_input_stream_new (dup (stdin_fd), TRUE);
  output = g_unix_output_stream_new (dup (stdout_fd), TRUE);
  stream = g_simple_io_stream_new (input, output);

  if (!(context = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    goto handle_error;

  server = foundry_mcp_server_new (context, stream);

  if (server == NULL)
    {
      foundry_command_line_printerr (command_line, "%s\n", _("Failed to create MCP server"));
      return EXIT_FAILURE;
    }

  foundry_mcp_server_start (server);

  if (cancellable != NULL)
    {
      if (!dex_await (dex_ref (cancellable), &error))
        goto handle_error;
    }

  foundry_mcp_server_stop (server);

  return EXIT_SUCCESS;

handle_error:

  if (server != NULL)
    foundry_mcp_server_stop (server);

  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_mcp (FoundryCliCommandTree *tree)
{
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "mcp"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         { "help", 0, 0, G_OPTION_ARG_NONE },
                                         {0}
                                       },
                                       .run = foundry_cli_builtin_mcp_run,
                                       .prepare = NULL,
                                       .complete = NULL,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("Run MCP server"),
                                     });
}
