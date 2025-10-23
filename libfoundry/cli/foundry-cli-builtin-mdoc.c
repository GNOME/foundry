/* foundry-cli-builtin-mdoc.c
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

#include "foundry-build-manager.h"
#include "foundry-build-pipeline.h"
#include "foundry-config.h"
#include "foundry-cli-builtin-private.h"
#include "foundry-cli-command-tree.h"
#include "foundry-command-line.h"
#include "foundry-context.h"
#include "foundry-file.h"
#include "foundry-gir.h"
#include "foundry-gir-markdown.h"
#include "foundry-gir-node-private.h"
#include "foundry-model-manager.h"
#include "foundry-sdk.h"
#include "foundry-service.h"
#include "foundry-util-private.h"

static gboolean
match_possible (const char *base,
                const char *symbol)
{
  /* TODO: This could obviously be better */
  return g_unichar_tolower (g_utf8_get_char (base)) == g_unichar_tolower (g_utf8_get_char (symbol));
}

static FoundryGirTraverseResult
traverse_func (FoundryGirNode *node,
               gpointer        user_data)
{
  const char *attr;

  attr = foundry_gir_node_get_attribute (node, "c:identifier");
  if (attr && strcmp (attr, (const char *)user_data) == 0)
    return FOUNDRY_GIR_TRAVERSE_MATCH;

  attr = foundry_gir_node_get_attribute (node, "c:type");
  if (attr && strcmp (attr, (const char *)user_data) == 0)
    return FOUNDRY_GIR_TRAVERSE_MATCH;

  return FOUNDRY_GIR_TRAVERSE_CONTINUE;
}

static FoundryGirNode *
scan_for_symbol (FoundryGir *gir,
                 const char *symbol)
{
  FoundryGirNode *repository;

  g_assert (FOUNDRY_IS_GIR (gir));
  g_assert (symbol != NULL);

  if (!(repository = foundry_gir_get_repository (gir)))
    return NULL;

  return _foundry_gir_node_traverse (repository, traverse_func, (gpointer)symbol);
}

static gboolean
mdoc (FoundryCommandLine  *command_line,
      FoundryGir          *gir,
      FoundryGirNode      *node,
      GError             **error)
{
  g_autoptr(FoundryGirMarkdown) markdown = NULL;
  g_autofree char *str = NULL;

  markdown = g_object_new (FOUNDRY_TYPE_GIR_MARKDOWN,
                           "gir", gir,
                           "node", node,
                           NULL);

  if (!(str = foundry_gir_markdown_generate (markdown, error)))
    return FALSE;

  foundry_command_line_print (command_line, "%s\n", g_strstrip (str));

  return TRUE;
}

static int
foundry_cli_builtin_mdoc_run (FoundryCommandLine *command_line,
                              const char * const *argv,
                              FoundryCliOptions  *options,
                              DexCancellable     *cancellable)
{
  g_autoptr(FoundryBuildPipeline) build_pipeline = NULL;
  g_autoptr(FoundryBuildManager) build_manager = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GPtrArray) search_dirs = NULL;
  g_autoptr(GHashTable) seen = NULL;
  g_autoptr(GError) error = NULL;
  const char *symbol;
  gboolean found_match = FALSE;

  g_assert (FOUNDRY_IS_COMMAND_LINE (command_line));
  g_assert (argv != NULL);
  g_assert (options != NULL);
  g_assert (!cancellable || DEX_IS_CANCELLABLE (cancellable));

  if (!(symbol = argv[1]))
    {
      foundry_command_line_printerr (command_line, "usage: %s Symbol\n", argv[0]);
      return EXIT_FAILURE;
    }

  seen = g_hash_table_new_full ((GHashFunc)g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);

  if (!(context = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    goto handle_error;

  build_manager = foundry_context_dup_build_manager (context);
  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (build_manager)), &error))
    goto handle_error;

  search_dirs = g_ptr_array_new_with_free_func (g_object_unref);

  if ((build_pipeline = dex_await_object (foundry_build_manager_load_pipeline (build_manager), NULL)))
    {
      g_autofree char *builddir = foundry_build_pipeline_dup_builddir (build_pipeline);
      g_autoptr(FoundrySdk) sdk = foundry_build_pipeline_dup_sdk (build_pipeline);
      g_autoptr(FoundryConfig) config = foundry_build_pipeline_dup_config (build_pipeline);
      g_autoptr(GFile) inst_dir = NULL;
      g_autoptr(GFile) usr_dir = NULL;
      g_autofree char *prefix = NULL;
      g_autofree char *gir_dir = NULL;

      prefix = foundry_sdk_dup_config_option (sdk, FOUNDRY_SDK_CONFIG_OPTION_PREFIX);
      gir_dir = g_build_filename (prefix, "share/gir-1.0", NULL);

      g_ptr_array_add (search_dirs, g_file_new_for_path (builddir));

      if ((usr_dir = dex_await_object (foundry_sdk_translate_path (sdk, build_pipeline, "/usr/share/gir-1.0"), NULL)))
        g_ptr_array_add (search_dirs, g_object_ref (usr_dir));

      if ((inst_dir = dex_await_object (foundry_sdk_translate_path (sdk, build_pipeline, gir_dir), NULL)))
        g_ptr_array_add (search_dirs, g_object_ref (inst_dir));
    }

  g_ptr_array_add (search_dirs, g_file_new_for_path ("/usr/share/gir-1.0"));

  for (guint i = 0; i < search_dirs->len; i++)
    {
      GFile *file = g_ptr_array_index (search_dirs, i);
      g_autoptr(GPtrArray) files = NULL;
      g_autoptr(GPtrArray) futures = NULL;

      if (g_hash_table_contains (seen, file))
        continue;
      else
        g_hash_table_replace (seen, g_object_ref (file), NULL);

      g_debug ("Searching %s...", g_file_peek_path (file));

      if (!(files = dex_await_boxed (foundry_file_find_with_depth (file, "*.gir", 0), NULL)))
        continue;

      if (files->len == 0)
        continue;

      futures = g_ptr_array_new_with_free_func (dex_unref);

      for (guint j = 0; j < files->len; j++)
        {
          GFile *gir_file = g_ptr_array_index (files, j);
          g_autofree char *base = g_file_get_basename (gir_file);

          if (!match_possible (base, symbol))
            continue;

          g_ptr_array_add (futures, foundry_gir_new (gir_file));
        }

      if (futures->len > 0)
        dex_await (foundry_future_all (futures), NULL);

      for (guint j = 0; j < futures->len; j++)
        {
          DexFuture *future = g_ptr_array_index (futures, j);
          const GValue *value;

          if ((value = dex_future_get_value (future, NULL)) &&
              G_VALUE_HOLDS (value, FOUNDRY_TYPE_GIR))
            {
              FoundryGir *gir = g_value_get_object (value);
              FoundryGirNode *node = NULL;

              if ((node = scan_for_symbol (gir, symbol)))
                {
                  found_match = TRUE;
                  mdoc (command_line, gir, node, &error);
                  break;
                }
            }
        }

      if (found_match)
        break;
    }

  if (found_match == FALSE)
    {
      foundry_command_line_printerr (command_line, "Nothing relevant found\n");
      return EXIT_FAILURE;
    }

  if (error != NULL)
    goto handle_error;

  return EXIT_SUCCESS;

handle_error:

  foundry_command_line_printerr (command_line, "%s\n", error->message);
  return EXIT_FAILURE;
}

void
foundry_cli_builtin_mdoc (FoundryCliCommandTree *tree)
{
  foundry_cli_command_tree_register (tree,
                                     FOUNDRY_STRV_INIT ("foundry", "mdoc"),
                                     &(FoundryCliCommand) {
                                       .options = (GOptionEntry[]) {
                                         { "help", 0, 0, G_OPTION_ARG_NONE },
                                         {0}
                                       },
                                       .run = foundry_cli_builtin_mdoc_run,
                                       .prepare = NULL,
                                       .complete = NULL,
                                       .gettext_package = GETTEXT_PACKAGE,
                                       .description = N_("KEYWORD - find gir doc in markdown"),
                                     });
}
