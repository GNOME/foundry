/* test-json.c
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

#include <foundry.h>

static char *
serialize (JsonNode *node)
{
  g_autoptr(JsonGenerator) generator = json_generator_new ();
  json_generator_set_root (generator, node);
  return json_generator_to_data (generator, NULL);
}

static void
compare_json (const char *filename,
              JsonNode   *node)
{
  g_autofree char *path = NULL;
  g_autofree char *contents = NULL;
  g_autofree char *serialized = NULL;
  g_autoptr(GError) error = NULL;
  gsize len;

  g_assert_nonnull (g_getenv ("G_TEST_SRCDIR"));
  g_assert_nonnull (filename);
  g_assert_nonnull (node);

  path = g_build_filename (g_getenv ("G_TEST_SRCDIR"), "test-json", filename, NULL);

  g_file_get_contents (path, &contents, &len, &error);
  g_assert_no_error (error);
  g_assert_nonnull (contents);

  serialized = serialize (node);
  g_assert_nonnull (serialized);

  g_strstrip (contents);
  g_strstrip (serialized);

  g_assert_cmpstr (contents, ==, serialized);

  json_node_unref (node);
}

static void
test_json_object_new (void)
{
  compare_json ("test1.json", FOUNDRY_JSON_OBJECT_NEW ("a", "b"));
  compare_json ("test1.json", FOUNDRY_JSON_OBJECT_NEW ("a", FOUNDRY_JSON_NODE_PUT_STRING ("b")));
  compare_json ("test2.json", FOUNDRY_JSON_OBJECT_NEW ("a", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE)));
  compare_json ("test3.json", FOUNDRY_JSON_OBJECT_NEW ("a", FOUNDRY_JSON_NODE_PUT_BOOLEAN (FALSE)));
  compare_json ("test4.json", FOUNDRY_JSON_OBJECT_NEW ("a", FOUNDRY_JSON_NODE_PUT_DOUBLE (123.45)));
  compare_json ("test5.json", FOUNDRY_JSON_OBJECT_NEW ("a", FOUNDRY_JSON_NODE_PUT_STRV (FOUNDRY_STRV_INIT ("a", "b", "c"))));
  compare_json ("test6.json", FOUNDRY_JSON_OBJECT_NEW ("a", FOUNDRY_JSON_NODE_PUT_INT (G_MAXINT)));
}

int
main (int argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Foundry/Json/Object/new", test_json_object_new);
  return g_test_run ();
}
