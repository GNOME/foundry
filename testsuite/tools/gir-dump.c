/* gir-dump.c
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

#include "foundry.h"

#include "../test-util.h"

static void
print_repository_summary (FoundryGirNode *repository)
{
  const gchar *version = foundry_gir_node_get_attribute (repository, "version");
  g_autofree FoundryGirNode **packages = NULL;
  g_autofree FoundryGirNode **includes = NULL;
  guint n_packages = 0;
  guint n_includes = 0;

  g_print ("Repository version: %s\n", version != NULL ? version : "unknown");

  packages = foundry_gir_node_list_children_typed (repository, FOUNDRY_GIR_NODE_PACKAGE, &n_packages);
  if (packages != NULL && n_packages > 0)
    {
      g_print ("Packages:\n");
      for (guint i = 0; i < n_packages; i++)
        {
          FoundryGirNode *pkg = packages[i];
          g_print ("  %s\n", foundry_gir_node_get_attribute (pkg, "name"));
        }
    }

  includes = foundry_gir_node_list_children_typed (repository, FOUNDRY_GIR_NODE_INCLUDE, &n_includes);
  if (includes != NULL && n_includes > 0)
    {
      g_print ("\n");
      g_print ("Includes:\n");
      for (guint i = 0; i < n_includes; i++)
        {
          FoundryGirNode *include = includes[i];
          const gchar *name = foundry_gir_node_get_attribute (include, "name");
          const gchar *inc_version = foundry_gir_node_get_attribute (include, "version");

          if (inc_version != NULL)
            g_print ("  %s-%s\n", name, inc_version);
          else
            g_print ("  %s\n", name);
        }
    }
}

static void
print_namespace_summary (FoundryGirNode *namespace)
{
  const gchar *name = foundry_gir_node_get_attribute (namespace, "name");
  const gchar *version = foundry_gir_node_get_attribute (namespace, "version");
  guint n_classes = 0;
  guint n_interfaces = 0;
  guint n_records = 0;
  guint n_unions = 0;
  guint n_bitfields = 0;
  guint n_enums = 0;
  guint n_callbacks = 0;
  guint n_functions = 0;
  guint n_constants = 0;
  guint n_methods = 0;

  g_autofree FoundryGirNode **classes = foundry_gir_node_list_children_typed (namespace, FOUNDRY_GIR_NODE_CLASS, &n_classes);
  g_autofree FoundryGirNode **interfaces = foundry_gir_node_list_children_typed (namespace, FOUNDRY_GIR_NODE_INTERFACE, &n_interfaces);
  g_autofree FoundryGirNode **records = foundry_gir_node_list_children_typed (namespace, FOUNDRY_GIR_NODE_RECORD, &n_records);
  g_autofree FoundryGirNode **unions = foundry_gir_node_list_children_typed (namespace, FOUNDRY_GIR_NODE_UNION, &n_unions);
  g_autofree FoundryGirNode **bitfields = foundry_gir_node_list_children_typed (namespace, FOUNDRY_GIR_NODE_BITFIELD, &n_bitfields);
  g_autofree FoundryGirNode **enums = foundry_gir_node_list_children_typed (namespace, FOUNDRY_GIR_NODE_ENUM, &n_enums);
  g_autofree FoundryGirNode **callbacks = foundry_gir_node_list_children_typed (namespace, FOUNDRY_GIR_NODE_CALLBACK, &n_callbacks);
  g_autofree FoundryGirNode **functions = foundry_gir_node_list_children_typed (namespace, FOUNDRY_GIR_NODE_FUNCTION, &n_functions);
  g_autofree FoundryGirNode **constants = foundry_gir_node_list_children_typed (namespace, FOUNDRY_GIR_NODE_CONSTANT, &n_constants);

  for (guint i = 0; i < n_classes; i++)
    {
      FoundryGirNode *class_node = classes[i];
      guint n_class_methods = 0;
      g_autofree FoundryGirNode **methods = foundry_gir_node_list_children_typed (class_node, FOUNDRY_GIR_NODE_METHOD, &n_class_methods);

      n_methods += n_class_methods;
    }

  g_print ("\nNamespace %s (%s)\n", name != NULL ? name : "<unnamed>", version != NULL ? version : "unknown");
  g_print ("  Classes:    %u\n", n_classes);
  g_print ("  Interfaces: %u\n", n_interfaces);
  g_print ("  Records:    %u\n", n_records);
  g_print ("  Unions:     %u\n", n_unions);
  g_print ("  Bitfields:  %u\n", n_bitfields);
  g_print ("  Enums:      %u\n", n_enums);
  g_print ("  Callbacks:  %u\n", n_callbacks);
  g_print ("  Functions:  %u\n", n_functions);
  g_print ("  Constants:  %u\n", n_constants);
  g_print ("  Methods:    %u\n", n_methods);
}

static int argc;
static char **argv;

static void
main_fiber (void)
{
  g_autoptr(GPtrArray) futures = NULL;

  if (argc < 2)
    {
      g_printerr ("usage: %s GIR_FILE...\n", argv[0]);
      exit (EXIT_FAILURE);
    }

  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (int i = 1; i < argc; i++)
    g_ptr_array_add (futures, foundry_gir_new_for_path (argv[i]));

  dex_await (dex_future_allv ((DexFuture **)futures->pdata, futures->len), NULL);

  for (guint j = 0; j < futures->len; j++)
    {
      DexFuture *future = g_ptr_array_index (futures, j);
      g_autoptr(FoundryGir) gir = NULL;
      g_autoptr(GError) error = NULL;
      FoundryGirNode *repository = NULL;
      g_autofree FoundryGirNode **namespaces = NULL;
      GFile *file;
      guint n_namespaces = 0;

      if (!(gir = dex_await_object (dex_ref (future), &error)))
        {
          g_printerr ("failed to load GIR: %s: %s\n", argv[1+j], error->message);
          exit (EXIT_FAILURE);
        }

      repository = foundry_gir_get_repository (gir);

      if (repository == NULL)
        {
          g_printerr ("GIR did not provide a repository root\n");
          exit (EXIT_FAILURE);
        }

      file = foundry_gir_get_file (gir);

      g_print ("File: %s\n", g_file_peek_path (file));
      print_repository_summary (repository);

      namespaces = foundry_gir_list_namespaces (gir, &n_namespaces);

      for (guint i = 0; i < n_namespaces; i++)
        {
          FoundryGirNode *namespace = namespaces[i];

          print_namespace_summary (namespace);
        }

      g_print ("\n");
    }
}

int
main (int    real_argc,
      char **real_argv)
{
  argc = real_argc;
  argv = real_argv;

  dex_init ();

  test_from_fiber (main_fiber);

  return EXIT_SUCCESS;
}
