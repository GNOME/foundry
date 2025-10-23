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
print_documentation (FoundryCommandLine *command_line,
                     FoundryGirNode     *node)
{
  guint n_docs;
  FoundryGirNode **docs = foundry_gir_node_list_children_typed (node, FOUNDRY_GIR_NODE_DOC, &n_docs);

  if (docs != NULL && n_docs > 0)
    {
      foundry_command_line_print (command_line, "\n");
      foundry_command_line_print (command_line, "Documentation:\n");

      for (guint i = 0; i < n_docs; i++)
        {
          const char *content = foundry_gir_node_get_content (docs[i]);
          if (content != NULL)
            {
              foundry_command_line_print (command_line, "%s\n", content);
            }
        }
    }
}

static void
print_function_signature (FoundryCommandLine *command_line,
                          FoundryGirNode     *node)
{
  const char *name = foundry_gir_node_get_attribute (node, "c:identifier");
  const char *return_type = NULL;
  guint n_return_values;
  /* First find the parameters container */
  guint n_parameters_containers;
  FoundryGirNode **parameters_containers = foundry_gir_node_list_children_typed (node, FOUNDRY_GIR_NODE_PARAMETERS, &n_parameters_containers);
  FoundryGirNode **params = NULL;
  FoundryGirNode **instance_params = NULL;
  guint n_params = 0;
  guint n_instance_params = 0;

  if (parameters_containers != NULL && n_parameters_containers > 0)
    {
      /* Get parameters from the first parameters container */
      params = foundry_gir_node_list_children_typed (parameters_containers[0], FOUNDRY_GIR_NODE_PARAMETER, &n_params);
      instance_params = foundry_gir_node_list_children_typed (parameters_containers[0], FOUNDRY_GIR_NODE_INSTANCE_PARAMETER, &n_instance_params);
    }
  FoundryGirNode **return_values = NULL;

  if (name == NULL)
    name = foundry_gir_node_get_name (node);

  /* Get return type */
  return_values = foundry_gir_node_list_children_typed (node, FOUNDRY_GIR_NODE_RETURN_VALUE, &n_return_values);
  if (return_values != NULL && n_return_values > 0)
    {
      guint n_types;
      FoundryGirNode **types = foundry_gir_node_list_children_typed (return_values[0], FOUNDRY_GIR_NODE_TYPE, &n_types);
      if (types != NULL && n_types > 0)
        {
          return_type = foundry_gir_node_get_attribute (types[0], "c:type");
        }
    }

  if (return_type == NULL)
    return_type = "void";

  foundry_command_line_print (command_line, "%s %s (", return_type, name);


  /* Process instance parameters first */
  gboolean first_param = TRUE;
  if (instance_params != NULL && n_instance_params > 0)
    {
      for (guint i = 0; i < n_instance_params; i++)
        {
          const char *param_name = foundry_gir_node_get_name (instance_params[i]);
          const char *param_type = NULL;
          guint n_param_types;
          FoundryGirNode **param_types = foundry_gir_node_list_children_typed (instance_params[i], FOUNDRY_GIR_NODE_TYPE, &n_param_types);

          if (param_types != NULL && n_param_types > 0)
            {
              param_type = foundry_gir_node_get_attribute (param_types[0], "c:type");
            }

          if (!first_param)
            foundry_command_line_print (command_line, ", ");
          first_param = FALSE;

          if (param_type != NULL)
            foundry_command_line_print (command_line, "%s", param_type);
          else
            foundry_command_line_print (command_line, "void");

          if (param_name != NULL)
            foundry_command_line_print (command_line, " %s", param_name);
        }
    }

  /* Process regular parameters */
  if (params != NULL && n_params > 0)
    {
      for (guint i = 0; i < n_params; i++)
        {
          const char *param_name = foundry_gir_node_get_name (params[i]);
          const char *param_type = NULL;
          guint n_param_types;
          FoundryGirNode **param_types = foundry_gir_node_list_children_typed (params[i], FOUNDRY_GIR_NODE_TYPE, &n_param_types);

          if (param_types != NULL && n_param_types > 0)
            {
              param_type = foundry_gir_node_get_attribute (param_types[0], "c:type");
            }

          if (!first_param)
            foundry_command_line_print (command_line, ", ");
          first_param = FALSE;

          if (param_type != NULL)
            foundry_command_line_print (command_line, "%s", param_type);
          else
            foundry_command_line_print (command_line, "void");

          if (param_name != NULL)
            foundry_command_line_print (command_line, " %s", param_name);
        }
    }

  foundry_command_line_print (command_line, ");\n");
}

static void
print_class_definition (FoundryCommandLine *command_line,
                        FoundryGirNode     *node)
{
  const char *name = foundry_gir_node_get_attribute (node, "c:type");
  const char *parent = foundry_gir_node_get_attribute (node, "parent");
  guint n_methods;
  guint n_implements;
  FoundryGirNode **methods = NULL;
  FoundryGirNode **implements = NULL;

  /* Get the namespace name for prefixing unqualified names */
  const char *namespace_name = NULL;

  /* Find the namespace node by traversing up the tree */
  FoundryGirNode *current = node;
  while (current != NULL)
    {
      if (foundry_gir_node_get_node_type (current) == FOUNDRY_GIR_NODE_NAMESPACE)
        {
          namespace_name = foundry_gir_node_get_attribute (current, "name");
          break;
        }
      current = foundry_gir_node_get_parent (current);
    }

  if (name == NULL)
    name = foundry_gir_node_get_name (node);

  foundry_command_line_print (command_line, "class %s", name);

  if (parent != NULL)
    {
      /* If parent name doesn't contain ".", prefix it with namespace */
      if (namespace_name != NULL && strchr (parent, '.') == NULL)
        foundry_command_line_print (command_line, " inherits %s.%s", namespace_name, parent);
      else
        foundry_command_line_print (command_line, " inherits %s", parent);
    }

  /* Print implemented interfaces */
  implements = foundry_gir_node_list_children_typed (node, FOUNDRY_GIR_NODE_IMPLEMENTS, &n_implements);
  if (implements != NULL && n_implements > 0)
    {
      for (guint i = 0; i < n_implements; i++)
        {
          const char *interface_name = foundry_gir_node_get_attribute (implements[i], "name");
          if (interface_name != NULL)
            {
              /* If interface name doesn't contain ".", prefix it with namespace */
              if (namespace_name != NULL && strchr (interface_name, '.') == NULL)
                {
                  if (i == 0 && parent == NULL)
                    foundry_command_line_print (command_line, " implements %s.%s", namespace_name, interface_name);
                  else if (i == 0)
                    foundry_command_line_print (command_line, ", implements %s.%s", namespace_name, interface_name);
                  else
                    foundry_command_line_print (command_line, ", %s.%s", namespace_name, interface_name);
                }
              else
                {
                  if (i == 0 && parent == NULL)
                    foundry_command_line_print (command_line, " implements %s", interface_name);
                  else if (i == 0)
                    foundry_command_line_print (command_line, ", implements %s", interface_name);
                  else
                    foundry_command_line_print (command_line, ", %s", interface_name);
                }
            }
        }
    }

  foundry_command_line_print (command_line, ";\n");

  /* Print methods outside the class definition */
  methods = foundry_gir_node_list_children_typed (node, FOUNDRY_GIR_NODE_METHOD, &n_methods);


  if (methods != NULL && n_methods > 0)
    {
      foundry_command_line_print (command_line, "\n");
      for (guint i = 0; i < n_methods; i++)
        {
          print_function_signature (command_line, methods[i]);
        }
    }
}

static void
print_enum_definition (FoundryCommandLine *command_line,
                       FoundryGirNode     *node)
{
  const char *name = foundry_gir_node_get_attribute (node, "c:type");
  const char *c_name = foundry_gir_node_get_attribute (node, "c:identifier");
  guint n_members;
  FoundryGirNode **members = NULL;

  if (name == NULL)
    name = foundry_gir_node_get_name (node);

  if (c_name != NULL)
    foundry_command_line_print (command_line, "typedef enum %s %s;\n", c_name, name);
  else
    foundry_command_line_print (command_line, "enum %s\n", name);

  foundry_command_line_print (command_line, "{\n");

  /* Print enum members */
  members = foundry_gir_node_list_children_typed (node, FOUNDRY_GIR_NODE_ENUM_MEMBER, &n_members);

  if (members != NULL && n_members > 0)
    {
      for (guint i = 0; i < n_members; i++)
        {
          const char *member_name = foundry_gir_node_get_attribute (members[i], "c:identifier");
          const char *member_value = foundry_gir_node_get_attribute (members[i], "value");

          if (member_name == NULL)
            member_name = foundry_gir_node_get_name (members[i]);

          foundry_command_line_print (command_line, "  %s", member_name);

          if (member_value != NULL)
            foundry_command_line_print (command_line, " = %s", member_value);

          foundry_command_line_print (command_line, ",\n");
        }
    }

  foundry_command_line_print (command_line, "};\n");
}

static void
print_constant_definition (FoundryCommandLine *command_line,
                           FoundryGirNode     *node)
{
  const char *name = foundry_gir_node_get_attribute (node, "c:identifier");
  const char *type = foundry_gir_node_get_attribute (node, "c:type");
  const char *value = foundry_gir_node_get_attribute (node, "value");

  if (name == NULL)
    name = foundry_gir_node_get_name (node);

  if (type != NULL)
    foundry_command_line_print (command_line, "#define %s", name);
  else
    foundry_command_line_print (command_line, "#define %s", name);

  if (value != NULL)
    foundry_command_line_print (command_line, " %s", value);

  foundry_command_line_print (command_line, "\n");
}

static void
print_record_definition (FoundryCommandLine *command_line,
                         FoundryGirNode     *node)
{
  const char *name = foundry_gir_node_get_attribute (node, "c:type");
  guint n_fields;
  FoundryGirNode **fields = NULL;

  if (name == NULL)
    name = foundry_gir_node_get_name (node);

  foundry_command_line_print (command_line, "struct %s\n", name);
  foundry_command_line_print (command_line, "{\n");

  /* Print fields */
  fields = foundry_gir_node_list_children_typed (node, FOUNDRY_GIR_NODE_FIELD, &n_fields);

  if (fields != NULL && n_fields > 0)
    {
      for (guint i = 0; i < n_fields; i++)
        {
          const char *field_name = foundry_gir_node_get_name (fields[i]);
          const char *field_type = NULL;
          guint n_field_types;
          FoundryGirNode **field_types = foundry_gir_node_list_children_typed (fields[i], FOUNDRY_GIR_NODE_TYPE, &n_field_types);

          if (field_types != NULL && n_field_types > 0)
            {
              field_type = foundry_gir_node_get_attribute (field_types[0], "c:type");
            }

          if (field_type != NULL)
            foundry_command_line_print (command_line, "  %s", field_type);
          else
            foundry_command_line_print (command_line, "  void");

          if (field_name != NULL)
            foundry_command_line_print (command_line, " %s", field_name);

          foundry_command_line_print (command_line, ";\n");
        }
    }

  foundry_command_line_print (command_line, "};\n");
}

static void
mdoc (FoundryCommandLine *command_line,
      FoundryGirNode     *node)
{
  FoundryGirNodeType node_type = foundry_gir_node_get_node_type (node);
  const char *name = foundry_gir_node_get_name (node);
  const char *full_name = NULL;

  /* Get the full type name based on node type */
  if (node_type == FOUNDRY_GIR_NODE_FUNCTION ||
      node_type == FOUNDRY_GIR_NODE_FUNCTION_MACRO ||
      node_type == FOUNDRY_GIR_NODE_METHOD ||
      node_type == FOUNDRY_GIR_NODE_VIRTUAL_METHOD ||
      node_type == FOUNDRY_GIR_NODE_CONSTRUCTOR ||
      node_type == FOUNDRY_GIR_NODE_NAMESPACE_FUNCTION)
    {
      full_name = foundry_gir_node_get_attribute (node, "c:identifier");
    }
  else if (node_type == FOUNDRY_GIR_NODE_CLASS ||
           node_type == FOUNDRY_GIR_NODE_INTERFACE ||
           node_type == FOUNDRY_GIR_NODE_RECORD ||
           node_type == FOUNDRY_GIR_NODE_UNION ||
           node_type == FOUNDRY_GIR_NODE_ENUM ||
           node_type == FOUNDRY_GIR_NODE_ALIAS ||
           node_type == FOUNDRY_GIR_NODE_CALLBACK ||
           node_type == FOUNDRY_GIR_NODE_GLIB_BOXED ||
           node_type == FOUNDRY_GIR_NODE_GLIB_ERROR_DOMAIN ||
           node_type == FOUNDRY_GIR_NODE_GLIB_SIGNAL)
    {
      full_name = foundry_gir_node_get_attribute (node, "c:type");
    }
  else if (node_type == FOUNDRY_GIR_NODE_CONSTANT ||
           node_type == FOUNDRY_GIR_NODE_PROPERTY ||
           node_type == FOUNDRY_GIR_NODE_FIELD ||
           node_type == FOUNDRY_GIR_NODE_ENUM_MEMBER)
    {
      full_name = foundry_gir_node_get_attribute (node, "c:identifier");
    }

  if (full_name == NULL)
    full_name = name;

  foundry_command_line_print (command_line, "Symbol: %s\n", full_name != NULL ? full_name : "<unnamed>");

  switch (node_type)
    {
    case FOUNDRY_GIR_NODE_FUNCTION:
    case FOUNDRY_GIR_NODE_FUNCTION_MACRO:
    case FOUNDRY_GIR_NODE_METHOD:
    case FOUNDRY_GIR_NODE_VIRTUAL_METHOD:
    case FOUNDRY_GIR_NODE_CONSTRUCTOR:
    case FOUNDRY_GIR_NODE_NAMESPACE_FUNCTION:
      foundry_command_line_print (command_line, "Type: Function\n");
      print_function_signature (command_line, node);
      break;

    case FOUNDRY_GIR_NODE_CLASS:
    case FOUNDRY_GIR_NODE_INTERFACE:
      foundry_command_line_print (command_line, "Type: Class\n");
      print_class_definition (command_line, node);
      break;

    case FOUNDRY_GIR_NODE_ENUM:
      foundry_command_line_print (command_line, "Type: Enum\n");
      print_enum_definition (command_line, node);
      break;

    case FOUNDRY_GIR_NODE_CONSTANT:
      foundry_command_line_print (command_line, "Type: Constant\n");
      print_constant_definition (command_line, node);
      break;

    case FOUNDRY_GIR_NODE_RECORD:
    case FOUNDRY_GIR_NODE_UNION:
      foundry_command_line_print (command_line, "Type: %s\n",
                                 node_type == FOUNDRY_GIR_NODE_RECORD ? "Record" : "Union");
      print_record_definition (command_line, node);
      break;

    case FOUNDRY_GIR_NODE_ENUM_MEMBER:
      foundry_command_line_print (command_line, "Type: Enum Member\n");
      print_constant_definition (command_line, node);
      break;

    case FOUNDRY_GIR_NODE_ALIAS:
    case FOUNDRY_GIR_NODE_CALLBACK:
    case FOUNDRY_GIR_NODE_GLIB_BOXED:
    case FOUNDRY_GIR_NODE_GLIB_ERROR_DOMAIN:
    case FOUNDRY_GIR_NODE_GLIB_SIGNAL:
    case FOUNDRY_GIR_NODE_BITFIELD:
    case FOUNDRY_GIR_NODE_PROPERTY:
    case FOUNDRY_GIR_NODE_FIELD:
      foundry_command_line_print (command_line, "Type: %s\n",
                                 foundry_gir_node_type_to_string (node_type));
      break;

    case FOUNDRY_GIR_NODE_UNKNOWN:
    case FOUNDRY_GIR_NODE_REPOSITORY:
    case FOUNDRY_GIR_NODE_INCLUDE:
    case FOUNDRY_GIR_NODE_C_INCLUDE:
    case FOUNDRY_GIR_NODE_PACKAGE:
    case FOUNDRY_GIR_NODE_NAMESPACE:
    case FOUNDRY_GIR_NODE_ARRAY:
    case FOUNDRY_GIR_NODE_CLASS_METHOD:
    case FOUNDRY_GIR_NODE_CLASS_VIRTUAL_METHOD:
    case FOUNDRY_GIR_NODE_CLASS_PROPERTY:
    case FOUNDRY_GIR_NODE_DOC:
    case FOUNDRY_GIR_NODE_DOC_PARA:
    case FOUNDRY_GIR_NODE_DOC_TEXT:
    case FOUNDRY_GIR_NODE_IMPLEMENTS:
    case FOUNDRY_GIR_NODE_INSTANCE_PARAMETER:
    case FOUNDRY_GIR_NODE_PARAMETER:
    case FOUNDRY_GIR_NODE_PARAMETERS:
    case FOUNDRY_GIR_NODE_PREREQUISITE:
    case FOUNDRY_GIR_NODE_RETURN_VALUE:
    case FOUNDRY_GIR_NODE_SOURCE_POSITION:
    case FOUNDRY_GIR_NODE_TYPE:
    case FOUNDRY_GIR_NODE_VARARGS:
      foundry_command_line_print (command_line, "Type: %s\n",
                                 foundry_gir_node_type_to_string (node_type));
      break;

    default:
      foundry_command_line_print (command_line, "Type: %s\n",
                                 foundry_gir_node_type_to_string (node_type));
      break;
    }

  /* Print documentation */
  print_documentation (command_line, node);
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
