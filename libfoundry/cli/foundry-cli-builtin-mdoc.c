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

static FoundryGirNode *
scan_for_symbol (FoundryGir *gir,
                 const char *symbol)
{
  g_autofree FoundryGirNode **namespaces = NULL;
  FoundryGirNode *repository;
  FoundryGirNode *result = NULL;
  guint n_namespaces;

  g_assert (FOUNDRY_IS_GIR (gir));
  g_assert (symbol != NULL);

  if (!(repository = foundry_gir_get_repository (gir)))
    return NULL;

  if (!(namespaces = foundry_gir_list_namespaces (gir, &n_namespaces)) || n_namespaces == 0)
    return NULL;

  for (guint i = 0; i < n_namespaces && result == NULL; i++)
    {
      FoundryGirNode *namespace = namespaces[i];

      FoundryGirNodeType types[] = {
        FOUNDRY_GIR_NODE_FUNCTION,
        FOUNDRY_GIR_NODE_FUNCTION_MACRO,
        FOUNDRY_GIR_NODE_CLASS,
        FOUNDRY_GIR_NODE_INTERFACE,
        FOUNDRY_GIR_NODE_ENUM,
        FOUNDRY_GIR_NODE_CONSTANT,
        FOUNDRY_GIR_NODE_RECORD,
        FOUNDRY_GIR_NODE_UNION,
        FOUNDRY_GIR_NODE_ALIAS,
        FOUNDRY_GIR_NODE_CALLBACK,
        FOUNDRY_GIR_NODE_GLIB_BOXED,
        FOUNDRY_GIR_NODE_GLIB_ERROR_DOMAIN,
        FOUNDRY_GIR_NODE_GLIB_SIGNAL,
        FOUNDRY_GIR_NODE_METHOD,
        FOUNDRY_GIR_NODE_VIRTUAL_METHOD,
        FOUNDRY_GIR_NODE_PROPERTY,
        FOUNDRY_GIR_NODE_FIELD,
        FOUNDRY_GIR_NODE_CONSTRUCTOR,
        FOUNDRY_GIR_NODE_NAMESPACE_FUNCTION
      };

      for (guint j = 0; j < G_N_ELEMENTS (types) && result == NULL; j++)
        {
          guint n_nodes;
          g_autofree FoundryGirNode **nodes = foundry_gir_node_list_children_typed (namespace, types[j], &n_nodes);

          for (guint k = 0; k < n_nodes && result == NULL; k++)
            {
              const char *node_name = NULL;

              /* Get the appropriate attribute based on node type */
              if (types[j] == FOUNDRY_GIR_NODE_FUNCTION ||
                  types[j] == FOUNDRY_GIR_NODE_FUNCTION_MACRO ||
                  types[j] == FOUNDRY_GIR_NODE_METHOD ||
                  types[j] == FOUNDRY_GIR_NODE_VIRTUAL_METHOD ||
                  types[j] == FOUNDRY_GIR_NODE_CONSTRUCTOR ||
                  types[j] == FOUNDRY_GIR_NODE_NAMESPACE_FUNCTION)
                {
                  /* For functions, use c:identifier */
                  node_name = foundry_gir_node_get_attribute (nodes[k], "c:identifier");
                }
              else if (types[j] == FOUNDRY_GIR_NODE_CLASS ||
                       types[j] == FOUNDRY_GIR_NODE_INTERFACE ||
                       types[j] == FOUNDRY_GIR_NODE_RECORD ||
                       types[j] == FOUNDRY_GIR_NODE_UNION ||
                       types[j] == FOUNDRY_GIR_NODE_ENUM ||
                       types[j] == FOUNDRY_GIR_NODE_ALIAS ||
                       types[j] == FOUNDRY_GIR_NODE_CALLBACK ||
                       types[j] == FOUNDRY_GIR_NODE_GLIB_BOXED ||
                       types[j] == FOUNDRY_GIR_NODE_GLIB_ERROR_DOMAIN ||
                       types[j] == FOUNDRY_GIR_NODE_GLIB_SIGNAL)
                {
                  /* For types, use c:type */
                  node_name = foundry_gir_node_get_attribute (nodes[k], "c:type");
                }
              else if (types[j] == FOUNDRY_GIR_NODE_CONSTANT ||
                       types[j] == FOUNDRY_GIR_NODE_PROPERTY ||
                       types[j] == FOUNDRY_GIR_NODE_FIELD)
                {
                  /* For constants, properties, and fields, use c:identifier */
                  node_name = foundry_gir_node_get_attribute (nodes[k], "c:identifier");
                }
              else
                {
                  /* Fallback to the name attribute */
                  node_name = foundry_gir_node_get_name (nodes[k]);
                }

              if (node_name != NULL && g_strcmp0 (node_name, symbol) == 0)
                {
                  result = g_object_ref (nodes[k]);
                  break;
                }
            }
        }

      if (result == NULL)
        {
          guint n_enums;
          g_autofree FoundryGirNode **enums = foundry_gir_node_list_children_typed (namespace, FOUNDRY_GIR_NODE_ENUM, &n_enums);

          for (guint j = 0; j < n_enums && result == NULL; j++)
            {
              guint n_members;
              FoundryGirNode **members = foundry_gir_node_list_children_typed (enums[j], FOUNDRY_GIR_NODE_ENUM_MEMBER, &n_members);

              for (guint k = 0; k < n_members && result == NULL; k++)
                {
                  const char *member_name = foundry_gir_node_get_attribute (members[k], "c:identifier");
                  if (member_name != NULL && g_strcmp0 (member_name, symbol) == 0)
                    {
                      result = g_object_ref (members[k]);
                      break;
                    }
                }
            }
        }
    }

  return result;
}

static void
mdoc (FoundryCommandLine *command_line,
      FoundryGirNode     *node)
{
  g_print ("Got documentation node: %s\n",
           foundry_gir_node_get_name (node));
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
              g_autoptr(FoundryGirNode) node = NULL;

              if ((node = scan_for_symbol (gir, symbol)))
                {
                  found_match = TRUE;
                  mdoc (command_line, node);
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
