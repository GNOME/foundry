/* test-yaml.c
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-yaml-private.h"

#define REFERENCE_MEMBER "$gitlab-reference"

static GPtrArray *
parse_yaml (const char               *source,
            const char               *yaml,
            FoundryYamlParseFlags     flags,
            FoundryYamlParseTagFunc   tag_func,
            gpointer                  tag_data,
            GError                  **error)
{
  g_autoptr(GBytes) bytes = NULL;

  g_assert (yaml != NULL);

  bytes = g_bytes_new_static (yaml, strlen (yaml));

  return _foundry_yaml_parse (source, bytes, flags, tag_func, tag_data, error);
}

static JsonObject *
get_root_object (GPtrArray *documents)
{
  JsonNode *root;

  g_assert_nonnull (documents);
  g_assert_cmpuint (documents->len, >, 0);

  root = g_ptr_array_index (documents, 0);
  g_assert_true (JSON_NODE_HOLDS_OBJECT (root));

  return json_node_get_object (root);
}

static void
test_flatpak_compatibility (void)
{
  static const char yaml[] =
    "lower_true: true\n"
    "title_true: True\n"
    "upper_true: TRUE\n"
    "mixed_true: tRuE\n"
    "lower_false: false\n"
    "title_false: False\n"
    "upper_false: FALSE\n"
    "mixed_false: fAlSe\n"
    "lower_null: null\n"
    "title_null: Null\n"
    "upper_null: NULL\n"
    "mixed_null: nUlL\n"
    "tilde: ~\n"
    "empty:\n"
    "integer: -42\n"
    "quoted_integer: '42'\n"
    "duplicate: first\n"
    "duplicate: second\n"
    "defaults: &defaults\n"
    "  inherited: yes\n"
    "merged:\n"
    "  <<: *defaults\n"
    "  own: value\n"
    "---\n"
    ": invalid trailing document\n";
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) documents = NULL;
  JsonObject *merged;
  JsonObject *object;
  JsonNode *node;

  documents = parse_yaml (NULL,
                          yaml,
                          FOUNDRY_YAML_PARSE_FLAGS_COERCE_INTEGERS,
                          NULL,
                          NULL,
                          &error);
  g_assert_no_error (error);
  g_assert_cmpuint (documents->len, ==, 1);

  object = get_root_object (documents);
  g_assert_true (json_object_get_boolean_member (object, "lower_true"));
  g_assert_true (json_object_get_boolean_member (object, "title_true"));
  g_assert_true (json_object_get_boolean_member (object, "upper_true"));
  g_assert_cmpstr (json_object_get_string_member (object, "mixed_true"), ==, "tRuE");
  g_assert_false (json_object_get_boolean_member (object, "lower_false"));
  g_assert_false (json_object_get_boolean_member (object, "title_false"));
  g_assert_false (json_object_get_boolean_member (object, "upper_false"));
  g_assert_cmpstr (json_object_get_string_member (object, "mixed_false"), ==, "fAlSe");
  g_assert_true (JSON_NODE_HOLDS_NULL (json_object_get_member (object, "lower_null")));
  g_assert_true (JSON_NODE_HOLDS_NULL (json_object_get_member (object, "title_null")));
  g_assert_true (JSON_NODE_HOLDS_NULL (json_object_get_member (object, "upper_null")));
  g_assert_cmpstr (json_object_get_string_member (object, "mixed_null"), ==, "nUlL");
  g_assert_cmpstr (json_object_get_string_member (object, "tilde"), ==, "~");
  g_assert_cmpstr (json_object_get_string_member (object, "empty"), ==, "");
  g_assert_cmpint (json_object_get_int_member (object, "integer"), ==, -42);
  g_assert_cmpstr (json_object_get_string_member (object, "quoted_integer"), ==, "42");
  g_assert_cmpstr (json_object_get_string_member (object, "duplicate"), ==, "second");

  merged = json_object_get_object_member (object, "merged");
  node = json_object_get_member (merged, "<<");
  g_assert_true (JSON_NODE_HOLDS_OBJECT (node));
  g_assert_cmpstr (json_object_get_string_member (json_node_get_object (node), "inherited"),
                   ==,
                   "yes");
}

static void
test_flatpak_decimal_warning (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) documents = NULL;
  JsonObject *object;

  g_test_expect_message ("Foundry",
                         G_LOG_LEVEL_WARNING,
                         "*'1.25' will be parsed as a number by many YAML parsers");
  documents = parse_yaml (NULL,
                          "value: 1.25\n",
                          FOUNDRY_YAML_PARSE_FLAGS_COERCE_INTEGERS,
                          NULL,
                          NULL,
                          &error);
  g_test_assert_expected_messages ();
  g_assert_no_error (error);

  object = get_root_object (documents);
  g_assert_cmpstr (json_object_get_string_member (object, "value"), ==, "1.25");
}

static gboolean
transform_reference (const char  *tag,
                     JsonNode   **node,
                     gpointer     user_data,
                     GError     **error)
{
  g_autoptr(JsonObject) wrapper = NULL;
  guint *transforms = user_data;

  g_assert_nonnull (tag);
  g_assert_nonnull (node);
  g_assert_nonnull (*node);
  g_assert_nonnull (transforms);

  if (!g_str_equal (tag, "!reference"))
    return TRUE;

  (*transforms)++;
  wrapper = json_object_new ();
  json_object_set_member (wrapper, REFERENCE_MEMBER, g_steal_pointer (node));
  *node = json_node_init_object (json_node_alloc (), wrapper);

  return TRUE;
}

static void
test_gitlab_features (void)
{
  static const char yaml[] =
    ".base: &base\n"
    "  first: one\n"
    "  keep: base\n"
    "job:\n"
    "  <<: *base\n"
    "  keep: job\n"
    "  flag: tRuE\n"
    "  nil: ~\n"
    "  empty:\n"
    "  number: 42\n"
    "  ref: !reference [.base, first]\n"
    "---\n"
    "spec:\n"
    "  inputs: {}\n";
  const FoundryYamlParseFlags flags =
    (FOUNDRY_YAML_PARSE_FLAGS_EXTENDED_LITERALS |
     FOUNDRY_YAML_PARSE_FLAGS_MERGE_KEYS |
     FOUNDRY_YAML_PARSE_FLAGS_REJECT_DUPLICATE_KEYS |
     FOUNDRY_YAML_PARSE_FLAGS_LIMIT_EXPANSION |
     FOUNDRY_YAML_PARSE_FLAGS_ALL_DOCUMENTS);
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) documents = NULL;
  JsonObject *reference;
  JsonObject *object;
  JsonObject *job;
  JsonArray *path;
  guint transforms = 0;

  documents = parse_yaml ("pipeline.yml",
                          yaml,
                          flags,
                          transform_reference,
                          &transforms,
                          &error);
  g_assert_no_error (error);
  g_assert_cmpuint (documents->len, ==, 2);
  g_assert_cmpuint (transforms, ==, 1);

  object = get_root_object (documents);
  job = json_object_get_object_member (object, "job");
  g_assert_false (json_object_has_member (job, "<<"));
  g_assert_cmpstr (json_object_get_string_member (job, "first"), ==, "one");
  g_assert_cmpstr (json_object_get_string_member (job, "keep"), ==, "job");
  g_assert_true (json_object_get_boolean_member (job, "flag"));
  g_assert_true (JSON_NODE_HOLDS_NULL (json_object_get_member (job, "nil")));
  g_assert_true (JSON_NODE_HOLDS_NULL (json_object_get_member (job, "empty")));
  g_assert_cmpstr (json_object_get_string_member (job, "number"), ==, "42");

  reference = json_object_get_object_member (job, "ref");
  path = json_object_get_array_member (reference, REFERENCE_MEMBER);
  g_assert_cmpuint (json_array_get_length (path), ==, 2);
  g_assert_cmpstr (json_array_get_string_element (path, 0), ==, ".base");
  g_assert_cmpstr (json_array_get_string_element (path, 1), ==, "first");
}

static void
test_duplicate_keys (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) documents = NULL;

  documents = parse_yaml ("pipeline.yml",
                          "job: first\njob: second\n",
                          FOUNDRY_YAML_PARSE_FLAGS_REJECT_DUPLICATE_KEYS,
                          NULL,
                          NULL,
                          &error);
  g_assert_null (documents);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_cmpstr (error->message,
                   ==,
                   "pipeline.yml:2:1: duplicate mapping key 'job'");
}

static void
test_cyclic_alias (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) documents = NULL;

  documents = parse_yaml ("pipeline.yml",
                          "cycle: &cycle [*cycle]\n",
                          FOUNDRY_YAML_PARSE_FLAGS_NONE,
                          NULL,
                          NULL,
                          &error);
  g_assert_null (documents);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_cmpstr (error->message, ==, "cyclic YAML alias");
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Foundry/Yaml/flatpak-compatibility", test_flatpak_compatibility);
  g_test_add_func ("/Foundry/Yaml/flatpak-decimal-warning", test_flatpak_decimal_warning);
  g_test_add_func ("/Foundry/Yaml/gitlab-features", test_gitlab_features);
  g_test_add_func ("/Foundry/Yaml/duplicate-keys", test_duplicate_keys);
  g_test_add_func ("/Foundry/Yaml/cyclic-alias", test_cyclic_alias);
  return g_test_run ();
}
