/* foundry-cli-builtin-pipeline-unlink.c
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

#include "foundry-build-pipeline.h"
#include "foundry-cli-builtin-private.h"
#include "foundry-cli-command-private.h"
#include "foundry-context.h"
#include "foundry-settings.h"
#include "foundry-util-private.h"
#include "settings/gsettings-mapping.h"

static FoundryBuildPipelinePhase
parse_phase_string (const char *phase_str,
                    GError    **error)
{
  GFlagsClass *fclass;
  GFlagsValue *fvalue;
  g_auto(GStrv) parts = NULL;
  guint flags = 0;

  if (phase_str == NULL || phase_str[0] == 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Phase string cannot be empty");
      return FOUNDRY_BUILD_PIPELINE_PHASE_NONE;
    }

  fclass = g_type_class_ref (FOUNDRY_TYPE_BUILD_PIPELINE_PHASE);
  parts = g_strsplit (phase_str, "|", -1);

  for (guint i = 0; parts[i] != NULL; i++)
    {
      const char *nick = parts[i];

      /* Trim whitespace */
      while (*nick == ' ' || *nick == '\t')
        nick++;

      if (*nick == 0)
        continue;

      fvalue = g_flags_get_value_by_nick (fclass, nick);

      if (fvalue == NULL)
        {
          g_type_class_unref (fclass);
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Unknown phase flag: %s", nick);
          return FOUNDRY_BUILD_PIPELINE_PHASE_NONE;
        }

      flags |= fvalue->value;
    }

  g_type_class_unref (fclass);

  return flags;
}

static int
foundry_cli_builtin_pipeline_unlink_run (FoundryCommandLine *command_line,
                                         const char * const *argv,
                                         FoundryCliOptions  *options,
                                         DexCancellable     *cancellable)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryContext) other_context = NULL;
  g_autoptr(FoundrySettings) settings = NULL;
  g_autoptr(GFile) project_directory_file = NULL;
  g_autoptr(GFile) state_directory_file = NULL;
  g_autoptr(GFile) other_project_directory_file = NULL;
  g_autoptr(GVariant) current_variant = NULL;
  g_autoptr(GVariant) phase_variant = NULL;
  g_autoptr(GVariant) new_array = NULL;
  g_autoptr(GVariantBuilder) builder = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *state_directory_path = NULL;
  g_autofree char *project_directory_uri = NULL;
  FoundryBuildPipelinePhase phase;
  g_auto(GValue) phase_value = G_VALUE_INIT;
  guint removed_count = 0;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  if (argv[1] == NULL || argv[2] == NULL)
    {
      foundry_command_line_printerr (command_line, "usage: %s PHASE PROJECT_DIRECTORY\n", argv[0]);
      return EXIT_FAILURE;
    }

  /* Load our own context */
  if (!(context = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    goto handle_error;

  /* Get settings for "app.devsuite.foundry.build" */
  settings = foundry_context_load_settings (context, "app.devsuite.foundry.build", NULL);

  /* Parse phase string (phase of our pipeline) */
  phase = parse_phase_string (argv[1], &error);
  if (error != NULL)
    goto handle_error;

  /* Discover state directory for PROJECT_DIRECTORY */
  state_directory_path = dex_await_string (foundry_context_discover (argv[2], cancellable), &error);
  if (error != NULL)
    goto handle_error;

  /* Load other_context for the provided PROJECT_DIRECTORY */
  state_directory_file = g_file_new_for_path (state_directory_path);
  project_directory_file = g_file_get_parent (state_directory_file);

  other_context = dex_await_object (foundry_context_new (state_directory_path,
                                                         g_file_peek_path (project_directory_file),
                                                         FOUNDRY_CONTEXT_FLAGS_NONE,
                                                         cancellable),
                                    &error);
  if (error != NULL)
    goto handle_error;

  /* Get project-directory URI */
  other_project_directory_file = foundry_context_dup_project_directory (other_context);
  project_directory_uri = g_file_get_uri (other_project_directory_file);

  /* Convert phase flags to GVariant array of strings */
  g_value_init (&phase_value, FOUNDRY_TYPE_BUILD_PIPELINE_PHASE);
  g_value_set_flags (&phase_value, phase);
  phase_variant = g_variant_take_ref (g_settings_set_mapping (&phase_value, G_VARIANT_TYPE ("as"), NULL));

  /* Get current linked-workspaces array */
  current_variant = g_variant_take_ref (foundry_settings_get_value (settings, "linked-workspaces"));

  /* Build new array without matching entries */
  builder = g_variant_builder_new (G_VARIANT_TYPE ("aa{sv}"));

  for (guint i = 0; i < g_variant_n_children (current_variant); i++)
    {
      g_autoptr(GVariant) child = g_variant_get_child_value (current_variant, i);
      g_autoptr(GVariantDict) dict = NULL;
      g_autoptr(GVariant) entry_phase_variant = NULL;
      g_autofree char *entry_project_directory = NULL;

      dict = g_variant_dict_new (child);

      /* Get project-directory from entry */
      if (!g_variant_dict_lookup (dict, "project-directory", "s", &entry_project_directory))
        {
          /* Missing project-directory, keep entry */
          g_variant_builder_add_value (builder, child);
          continue;
        }

      /* Compare project-directory URIs */
      if (g_strcmp0 (project_directory_uri, entry_project_directory) != 0)
        {
          /* Project directory doesn't match, keep entry */
          g_variant_builder_add_value (builder, child);
          continue;
        }

      /* Get phase from entry */
      entry_phase_variant = g_variant_dict_lookup_value (dict, "phase", G_VARIANT_TYPE ("as"));

      if (entry_phase_variant == NULL)
        {
          /* Missing phase, keep entry */
          g_variant_builder_add_value (builder, child);
          continue;
        }

      /* Compare phases using GVariant equality */
      if (g_variant_equal (phase_variant, entry_phase_variant))
        {
          /* Both phase and project-directory match, remove this entry */
          removed_count++;
        }
      else
        {
          /* Phase doesn't match, keep entry */
          g_variant_builder_add_value (builder, child);
        }
    }

  new_array = g_variant_take_ref (g_variant_builder_end (builder));

  /* Set the new array back */
  foundry_settings_set_value (settings, "linked-workspaces", new_array);

  if (removed_count == 0)
    {
      foundry_command_line_printerr (command_line, "No matching workspace found to unlink\n");
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;

handle_error:

  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_pipeline_unlink (FoundryCliCommandTree *tree)
{
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "pipeline", "unlink"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         { "help", 0, 0, G_OPTION_ARG_NONE },
                                         {0}
                                       },
                                       .run = foundry_cli_builtin_pipeline_unlink_run,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("PHASE PROJECT_DIRECTORY - Unlink a workspace from the build pipeline"),
                                     });
}
