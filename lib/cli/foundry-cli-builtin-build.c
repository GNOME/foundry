/* foundry-cli-builtin-build.c
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

#include "foundry-build-manager.h"
#include "foundry-build-pipeline.h"
#include "foundry-build-progress.h"
#include "foundry-cli-builtin-private.h"
#include "foundry-cli-command-private.h"
#include "foundry-context.h"
#include "foundry-util-private.h"

static int
foundry_cli_builtin_build_run (FoundryCommandLine *command_line,
                               const char * const *argv,
                               FoundryCliOptions  *options,
                               DexCancellable     *cancellable)
{
  g_autoptr(FoundryBuildManager) build_manager = NULL;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryBuildProgress) progress = NULL;
  g_autoptr(FoundryContext) foundry = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *existing = NULL;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  if (!(foundry = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    goto handle_error;

  build_manager = foundry_context_dup_build_manager (foundry);

  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (build_manager)), &error))
    goto handle_error;

  if (!(pipeline = dex_await_object (foundry_build_manager_load_pipeline (build_manager), &error)))
    goto handle_error;

  progress = foundry_build_pipeline_build (pipeline, FOUNDRY_BUILD_PIPELINE_PHASE_BUILD);

  if (!dex_await (foundry_build_progress_await (progress), &error))
    goto handle_error;

  return EXIT_SUCCESS;

handle_error:
  foundry_command_line_printerr (command_line, "%s\n", error->message);

  return EXIT_FAILURE;
}

void
foundry_cli_builtin_build (FoundryCliCommandTree *tree)
{
  const char * const * aliases[] = {
    FOUNDRY_STRV_INIT ("foundry", "build"),
    FOUNDRY_STRV_INIT ("foundry", "pipeline", "build"),
  };

  for (guint i = 0; i < G_N_ELEMENTS (aliases); i++)
    foundry_cli_command_tree_register (tree,
                                       aliases[i],
                                       &(FoundryCliCommand) {
                                         .options = (GOptionEntry[]) {
                                           { "help", 0, 0, G_OPTION_ARG_NONE },
                                           {0}
                                         },
                                         .run = foundry_cli_builtin_build_run,
                                         .prepare = NULL,
                                         .complete = NULL,
                                         .gettext_package = GETTEXT_PACKAGE,
                                         .description = N_("Build the project"),
                                       });
}
