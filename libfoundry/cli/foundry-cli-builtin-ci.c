/* foundry-cli-builtin-ci.c
 *
 * Copyright 2026 Christian Hergert
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

#include "foundry-ci-job.h"
#include "foundry-ci-manager.h"
#include "foundry-ci-pipeline.h"
#include "foundry-ci-run-options.h"
#include "foundry-ci-run.h"
#include "foundry-cli-builtin-private.h"
#include "foundry-context.h"
#include "foundry-service.h"

static gboolean
load_ci (FoundryCommandLine  *command_line,
         FoundryCliOptions   *options,
         FoundryContext     **context,
         FoundryCiManager   **manager,
         GListModel         **pipelines,
         GError             **error)
{
  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (options != NULL);
  g_assert (context != NULL);
  g_assert (manager != NULL);
  g_assert (pipelines != NULL);

  if (!(*context = dex_await_object (foundry_cli_options_load_context (options, command_line), error)))
    return FALSE;

  *manager = foundry_context_dup_ci_manager (*context);
  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (*manager)), error))
    return FALSE;

  if (!(*pipelines = dex_await_object (foundry_ci_manager_list_pipelines (*manager), error)))
    return FALSE;

  if (g_list_model_get_n_items (*pipelines) == 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_FOUND,
                           "No local CI pipeline was found");
      return FALSE;
    }

  return TRUE;
}

static FoundryCiRunOptions *
create_run_options (FoundryCommandLine *command_line,
                    FoundryCliOptions  *options)
{
  FoundryCiRunOptions *run_options;
  const char *output_dir;
  gboolean value;
  int max_jobs;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (options != NULL);

  run_options = foundry_ci_run_options_new ();
  foundry_ci_run_options_set_fds (run_options,
                                  foundry_command_line_get_stdin (command_line),
                                  foundry_command_line_get_stdout (command_line),
                                  foundry_command_line_get_stderr (command_line));

  if (foundry_cli_options_get_int (options, "jobs", &max_jobs) && max_jobs > 0)
    foundry_ci_run_options_set_max_jobs (run_options, max_jobs);
  if (foundry_cli_options_get_boolean (options, "fail-fast", &value))
    foundry_ci_run_options_set_fail_fast (run_options, value);
  if (foundry_cli_options_get_boolean (options, "offline", &value))
    foundry_ci_run_options_set_offline (run_options, value);
  if (foundry_cli_options_get_boolean (options, "save-state", &value))
    foundry_ci_run_options_set_save_state (run_options, value);
  if (foundry_cli_options_get_boolean (options, "save-workspace", &value))
    foundry_ci_run_options_set_save_workspace (run_options, value);
  if ((output_dir = foundry_cli_options_get_filename (options, "output-dir")))
    foundry_ci_run_options_set_output_dir (run_options, output_dir);

  return run_options;
}

static int
await_ci_run (FoundryCommandLine *command_line,
              FoundryCiRun       *run,
              DexCancellable     *cancellable)
{
  g_autoptr(DexFuture) completion = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *output_dir = NULL;
  int exit_status;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (FOUNDRY_IS_CI_RUN (run));
  g_assert (DEX_IS_CANCELLABLE (cancellable));

  completion = foundry_ci_run_await (run);
  exit_status = dex_await_int (dex_future_first (dex_ref (completion),
                                                 dex_ref (DEX_FUTURE (cancellable)),
                                                 NULL),
                               &error);

  if (error != NULL)
    {
      foundry_ci_run_cancel (run);
      dex_await (dex_ref (completion), NULL);
      foundry_command_line_printerr (command_line, "%s\n", error->message);
      return EXIT_FAILURE;
    }

  if ((output_dir = foundry_ci_run_dup_output_dir (run)))
    foundry_command_line_printerr (command_line, "CI output: %s\n", output_dir);

  return exit_status;
}

static int
foundry_cli_builtin_ci_list_run (FoundryCommandLine *command_line,
                                 const char * const *argv,
                                 FoundryCliOptions  *options,
                                 DexCancellable     *cancellable)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryCiManager) manager = NULL;
  g_autoptr(GListModel) pipelines = NULL;
  g_autoptr(GListStore) jobs = NULL;
  g_autoptr(GError) error = NULL;
  FoundryObjectSerializerFormat format;
  const char *format_arg;
  guint n_pipelines;

  static const FoundryObjectSerializerEntry fields[] = {
    { "id", N_("ID") },
    { "stage", N_("Stage") },
    { "disposition", N_("Disposition") },
    { "reason", N_("Reason") },
    { "image", N_("Image") },
    { 0 }
  };

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (DEX_IS_CANCELLABLE (cancellable));

  if (!load_ci (command_line, options, &context, &manager, &pipelines, &error))
    goto failure;

  jobs = g_list_store_new (FOUNDRY_TYPE_CI_JOB);
  n_pipelines = g_list_model_get_n_items (pipelines);
  for (guint i = 0; i < n_pipelines; i++)
    {
      g_autoptr(FoundryCiPipeline) pipeline = g_list_model_get_item (pipelines, i);
      g_autoptr(GListModel) pipeline_jobs = foundry_ci_pipeline_list_jobs (pipeline);
      guint n_jobs = g_list_model_get_n_items (pipeline_jobs);

      for (guint j = 0; j < n_jobs; j++)
        {
          g_autoptr(FoundryCiJob) job = g_list_model_get_item (pipeline_jobs, j);

          g_list_store_append (jobs, job);
        }
    }

  format_arg = foundry_cli_options_get_string (options, "format");
  format = foundry_object_serializer_format_parse (format_arg);
  foundry_command_line_print_list (command_line,
                                   G_LIST_MODEL (jobs),
                                   fields,
                                   format,
                                   FOUNDRY_TYPE_CI_JOB);
  return EXIT_SUCCESS;

failure:
  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

static int
foundry_cli_builtin_ci_run_run (FoundryCommandLine *command_line,
                                const char * const *argv,
                                FoundryCliOptions  *options,
                                DexCancellable     *cancellable)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryCiManager) manager = NULL;
  g_autoptr(FoundryCiPipeline) pipeline = NULL;
  g_autoptr(FoundryCiRunOptions) run_options = NULL;
  g_autoptr(FoundryCiRun) run = NULL;
  g_autoptr(GListModel) pipelines = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (DEX_IS_CANCELLABLE (cancellable));

  if (!load_ci (command_line, options, &context, &manager, &pipelines, &error))
    goto failure;

  pipeline = g_list_model_get_item (pipelines, 0);
  run_options = create_run_options (command_line, options);
  run = dex_await_object (foundry_ci_manager_run (manager,
                                                  pipeline,
                                                  argv[1] != NULL ? &argv[1] : NULL,
                                                  run_options),
                          &error);

  if (run != NULL)
    return await_ci_run (command_line, run, cancellable);

failure:
  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

static FoundryCiJob *
find_job (GListModel *pipelines,
          const char *job_id)
{
  guint n_pipelines;

  g_assert (G_IS_LIST_MODEL (pipelines));
  g_assert (job_id != NULL);

  n_pipelines = g_list_model_get_n_items (pipelines);
  for (guint i = 0; i < n_pipelines; i++)
    {
      g_autoptr(FoundryCiPipeline) pipeline = g_list_model_get_item (pipelines, i);
      g_autoptr(GListModel) jobs = foundry_ci_pipeline_list_jobs (pipeline);
      guint n_jobs = g_list_model_get_n_items (jobs);

      for (guint j = 0; j < n_jobs; j++)
        {
          g_autoptr(FoundryCiJob) job = g_list_model_get_item (jobs, j);
          g_autofree char *id = foundry_ci_job_dup_id (job);

          if (g_strcmp0 (id, job_id) == 0)
            return g_steal_pointer (&job);
        }
    }

  return NULL;
}

static int
foundry_cli_builtin_ci_shell_run (FoundryCommandLine *command_line,
                                  const char * const *argv,
                                  FoundryCliOptions  *options,
                                  DexCancellable     *cancellable)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryCiManager) manager = NULL;
  g_autoptr(FoundryCiRunOptions) run_options = NULL;
  g_autoptr(FoundryCiRun) run = NULL;
  g_autoptr(FoundryCiJob) job = NULL;
  g_autoptr(GListModel) pipelines = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (DEX_IS_CANCELLABLE (cancellable));

  if (argv[1] == NULL || argv[2] != NULL)
    {
      foundry_command_line_printerr (command_line, "usage: %s JOB\n", argv[0]);
      return EXIT_FAILURE;
    }

  if (!load_ci (command_line, options, &context, &manager, &pipelines, &error))
    goto failure;

  if (!(job = find_job (pipelines, argv[1])))
    {
      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "No CI job named “%s”",
                   argv[1]);
      goto failure;
    }

  run_options = create_run_options (command_line, options);
  run = dex_await_object (foundry_ci_manager_run_shell (manager, job, run_options), &error);

  if (run != NULL)
    return await_ci_run (command_line, run, cancellable);

failure:
  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_ci (FoundryCliCommandTree *tree)
{
  static GOptionEntry run_entries[] = {
    { "help", 0, 0, G_OPTION_ARG_NONE },
    { "jobs", 'j', 0, G_OPTION_ARG_INT, NULL, N_("Bound concurrent jobs"), N_("N") },
    { "offline", 0, 0, G_OPTION_ARG_NONE, NULL, N_("Use only cached includes"), NULL },
    { "fail-fast", 0, 0, G_OPTION_ARG_NONE, NULL, N_("Stop after the first failure"), NULL },
    { "output-dir", 0, 0, G_OPTION_ARG_FILENAME, NULL, N_("Store outputs below DIR"), N_("DIR") },
    { "save-state", 0, 0, G_OPTION_ARG_NONE, NULL, N_("Preserve diagnostic state"), NULL },
    { "save-workspace", 0, 0, G_OPTION_ARG_NONE, NULL, N_("Preserve job workspaces"), NULL },
    { 0 }
  };

  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "ci", "list"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         { "help", 0, 0, G_OPTION_ARG_NONE },
                                         { "format", 'f', 0, G_OPTION_ARG_STRING, NULL, N_("Output format (text, json)"), N_("FORMAT") },
                                         { 0 }
                                       },
                                       .run = foundry_cli_builtin_ci_list_run,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("List local CI jobs"),
                                     });
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "ci", "run"),
                                     &(FoundryCliCommand) {
                                       .options = run_entries,
                                       .run = foundry_cli_builtin_ci_run_run,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("[JOB…] - Run local CI jobs"),
                                     });
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "ci", "shell"),
                                     &(FoundryCliCommand) {
                                       .options = run_entries,
                                       .run = foundry_cli_builtin_ci_shell_run,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("JOB - Open a shell for a CI job"),
                                     });
}
