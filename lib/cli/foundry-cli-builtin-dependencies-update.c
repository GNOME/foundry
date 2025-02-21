/* foundry-cli-builtin-dependency-update.c
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

#include "foundry-cli-builtin-private.h"
#include "foundry-cli-command-tree.h"
#include "foundry-command-line.h"
#include "foundry-config.h"
#include "foundry-config-manager.h"
#include "foundry-context.h"
#include "foundry-dependency-manager.h"
#include "foundry-dependency.h"
#include "foundry-future-list-model.h"
#include "foundry-service.h"
#include "foundry-util-private.h"

static int
foundry_cli_builtin_dependencies_update_run (FoundryCommandLine *command_line,
                                             const char * const *argv,
                                             FoundryCliOptions  *options,
                                             DexCancellable     *cancellable)
{
  g_autoptr(FoundryDependencyManager) dependency_manager = NULL;
  g_autoptr(FoundryConfigManager) config_manager = NULL;
  g_autoptr(FoundryContext) foundry = NULL;
  g_autoptr(FoundryConfig) config = NULL;
  g_autoptr(GListModel) model = NULL;
  g_autoptr(GError) error = NULL;
  guint n_items;
  int pty_fd;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (options != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  if (!(foundry = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    goto handle_error;

  dependency_manager = foundry_context_dup_dependency_manager (foundry);
  config_manager = foundry_context_dup_config_manager (foundry);

  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (config_manager)), &error))
    goto handle_error;

  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (dependency_manager)), &error))
    goto handle_error;

  if (!(config = foundry_config_manager_dup_config (config_manager)))
    {
      foundry_command_line_printerr (command_line, "No active configuration\n");
      return EXIT_FAILURE;
    }

  if (!(model = dex_await_object (foundry_dependency_manager_list_dependencies (dependency_manager, config), &error)))
    goto handle_error;

  /* Wait for async population of model */
  if (FOUNDRY_IS_FUTURE_LIST_MODEL (model))
    dex_await (foundry_future_list_model_await (FOUNDRY_FUTURE_LIST_MODEL (model)), NULL);

  n_items = g_list_model_get_n_items (model);
  pty_fd = foundry_command_line_get_stdout (command_line);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryDependency) dependency = g_list_model_get_item (model, i);

      if (!dex_await (foundry_dependency_update (dependency, cancellable, pty_fd), &error))
        goto handle_error;
    }

  return EXIT_SUCCESS;

handle_error:

  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_dependencies_update (FoundryCliCommandTree *tree)
{
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "dependencies", "update"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         { "help", 0, 0, G_OPTION_ARG_NONE },
                                         {0}
                                       },
                                       .run = foundry_cli_builtin_dependencies_update_run,
                                       .prepare = NULL,
                                       .complete = NULL,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("Update dependencies"),
                                     });
}
