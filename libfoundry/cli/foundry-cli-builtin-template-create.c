/* foundry-cli-builtin-template-create.c
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

#include <glib/gi18n-lib.h>

#include "foundry-cli-builtin-private.h"
#include "foundry-context.h"
#include "foundry-init-private.h"
#include "foundry-input.h"
#include "foundry-model-manager.h"
#include "foundry-project-template.h"
#include "foundry-template-manager.h"
#include "foundry-service.h"
#include "foundry-util-private.h"

static int
foundry_cli_builtin_template_create_run (FoundryCommandLine *command_line,
                                         const char * const *argv,
                                         FoundryCliOptions  *options,
                                         DexCancellable     *cancellable)
{
  g_autoptr(FoundryTemplateManager) template_manager = NULL;
  g_autoptr(FoundryTemplate) template = NULL;
  g_autoptr(FoundryInput) input = NULL;
  g_autoptr(GError) error = NULL;
  const char *template_id;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  if (g_strv_length ((char **)argv) != 2)
    {
      foundry_command_line_printerr (command_line, "usage: %s TEMPLATE_ID\n", argv[0]);
      return EXIT_FAILURE;
    }

  /* Since we're not using a context, make sure our plugins
   * are loaded or we wont find any templates.
   */
  _foundry_init_plugins ();

  template_id = argv[1];
  template_manager = foundry_template_manager_new ();

  if (!(template = dex_await_object (foundry_template_manager_find_template (template_manager, template_id), &error)))
    goto handle_error;

  if ((input = foundry_template_dup_input (template)))
    {
      if (!dex_await (foundry_command_line_request_input (command_line, input), &error))
        goto handle_error;
    }

  if (!dex_await (foundry_template_expand (template), &error))
    goto handle_error;

  return EXIT_SUCCESS;

handle_error:

  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_template_create (FoundryCliCommandTree *tree)
{
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "template", "create"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         { "help", 0, 0, G_OPTION_ARG_NONE },
                                         {0}
                                       },
                                       .run = foundry_cli_builtin_template_create_run,
                                       .prepare = NULL,
                                       .complete = NULL,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("TEMPLATE_ID - Expand a template"),
                                     });
}
