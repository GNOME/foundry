/* plugin-gitlab-ci-component.c
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
 * You should have received a copy of the GNU Lesser General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "plugin-gitlab-ci-component-private.h"
#include "plugin-gitlab-ci-error-private.h"
#include "plugin-gitlab-ci-yaml-private.h"

#define MAX_DEPTH 64
#define MAX_EXPANDED (8 * 1024 * 1024)

static JsonNode *
member (JsonNode   *node,
        const char *name)
{
  g_assert (name != NULL);

  if (node == NULL || !JSON_NODE_HOLDS_OBJECT (node))
    return NULL;

  return json_object_get_member (json_node_get_object (node), name);
}

static const char *
string_value (JsonNode *node)
{
  if (node == NULL ||
      !JSON_NODE_HOLDS_VALUE (node) ||
      json_node_get_value_type (node) != G_TYPE_STRING)
    return NULL;

  return json_node_get_string (node);
}

static gboolean
input_matches_type (JsonNode   *value,
                    const char *type)
{
  char *endptr = NULL;
  const char *string;

  g_assert (value != NULL);
  g_assert (type != NULL);

  if (JSON_NODE_HOLDS_NULL (value))
    return TRUE;

  if (g_str_equal (type, "array"))
    return JSON_NODE_HOLDS_ARRAY (value);

  if (g_str_equal (type, "boolean"))
    return JSON_NODE_HOLDS_VALUE (value) &&
           json_node_get_value_type (value) == G_TYPE_BOOLEAN;

  if (g_str_equal (type, "number") && (string = string_value (value)))
    {
      g_ascii_strtod (string, &endptr);
      return endptr != string && *endptr == '\0';
    }

  return g_str_equal (type, "string") && string_value (value) != NULL;
}

static gboolean
input_is_allowed (JsonNode *value,
                  JsonNode *options)
{
  JsonArray *array;
  guint length;

  g_assert (value != NULL);
  g_assert (options != NULL);

  if (!JSON_NODE_HOLDS_ARRAY (options))
    return FALSE;

  array = json_node_get_array (options);
  length = json_array_get_length (array);

  for (guint i = 0; i < length; i++)
    {
      JsonNode *option = json_array_get_element (array, i);
      const char *option_string = string_value (option);
      const char *value_string = string_value (value);

      if (option_string != NULL &&
          value_string != NULL &&
          g_str_equal (option_string, value_string))
        return TRUE;
    }

  return FALSE;
}

static GHashTable *
collect_inputs (JsonNode  *declarations,
                JsonNode  *supplied,
                GError   **error)
{
  g_autoptr(GHashTable) values = NULL;
  JsonObjectIter iter;
  JsonObject *declaration_object;
  const char *name;
  JsonNode *definition;

  g_assert (declarations != NULL);

  if (!JSON_NODE_HOLDS_OBJECT (declarations) ||
      (supplied != NULL && !JSON_NODE_HOLDS_OBJECT (supplied)))
    {
      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                           "component inputs must be mappings");
      return NULL;
    }

  declaration_object = json_node_get_object (declarations);

  if (supplied != NULL)
    {
      JsonObjectIter supplied_iter;
      const char *supplied_name;
      JsonNode *supplied_value;

      json_object_iter_init (&supplied_iter, json_node_get_object (supplied));
      while (json_object_iter_next (&supplied_iter, &supplied_name, &supplied_value))
        {
          if (!json_object_has_member (declaration_object, supplied_name))
            {
              g_set_error (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_INVALID_ARGUMENT,
                           "unknown component input '%s'",
                           supplied_name);
              return NULL;
            }
        }
    }

  values = g_hash_table_new_full (g_str_hash,
                                  g_str_equal,
                                  g_free,
                                  (GDestroyNotify)json_node_unref);

  json_object_iter_init (&iter, declaration_object);
  while (json_object_iter_next (&iter, &name, &definition))
    {
      JsonNode *value = supplied ? member (supplied, name) : NULL;
      JsonNode *options = member (definition, "options");
      const char *type = string_value (member (definition, "type"));

      if (value == NULL)
        value = member (definition, "default");

      if (value == NULL)
        {
          g_set_error (error,
                       PLUGIN_GITLAB_CI_ERROR,
                       PLUGIN_GITLAB_CI_ERROR_INVALID_ARGUMENT,
                       "required component input '%s' was not supplied",
                       name);
          return NULL;
        }

      if (type == NULL)
        type = "string";

      if (!input_matches_type (value, type))
        {
          g_set_error (error,
                       PLUGIN_GITLAB_CI_ERROR,
                       PLUGIN_GITLAB_CI_ERROR_INVALID_ARGUMENT,
                       "component input '%s' requires type %s",
                       name,
                       type);
          return NULL;
        }

      if (options &&
          !JSON_NODE_HOLDS_NULL (value) &&
          !input_is_allowed (value, options))
        {
          g_set_error (error,
                       PLUGIN_GITLAB_CI_ERROR,
                       PLUGIN_GITLAB_CI_ERROR_INVALID_ARGUMENT,
                       "component input '%s' is not one of its options",
                       name);
          return NULL;
        }

      g_hash_table_insert (values, g_strdup (name), json_node_copy (value));
    }

  return g_steal_pointer (&values);
}

static gboolean
find_input (const char  *string,
            gsize        offset,
            gsize       *match_start,
            gsize       *match_end,
            char       **name)
{
  const char *start;
  const char *cursor;
  const char *name_start;

  g_assert (string != NULL);
  g_assert (match_start != NULL);
  g_assert (match_end != NULL);
  g_assert (name != NULL);

  if (!(start = strstr (string + offset, "$[[")))
    return FALSE;

  cursor = start + 3;
  while (g_ascii_isspace (*cursor))
    cursor++;

  if (!g_str_has_prefix (cursor, "inputs."))
    return FALSE;

  cursor += strlen ("inputs.");
  name_start = cursor;

  while (g_ascii_isalnum (*cursor) || *cursor == '_' || *cursor == '-')
    cursor++;

  if (cursor == name_start)
    return FALSE;

  *name = g_strndup (name_start, cursor - name_start);

  while (g_ascii_isspace (*cursor))
    cursor++;

  if (!g_str_has_prefix (cursor, "]]"))
    {
      g_clear_pointer (name, g_free);
      return FALSE;
    }

  *match_start = start - string;
  *match_end = cursor + 2 - string;

  return TRUE;
}

static char *
input_to_string (JsonNode  *input,
                 GError   **error)
{
  const char *string;

  g_assert (input != NULL);

  if (JSON_NODE_HOLDS_NULL (input))
    return g_strdup ("");

  if ((string = string_value (input)))
    return g_strdup (string);

  if (JSON_NODE_HOLDS_VALUE (input) &&
      json_node_get_value_type (input) == G_TYPE_BOOLEAN)
    return g_strdup (json_node_get_boolean (input) ? "true" : "false");

  g_set_error_literal (error,
                       PLUGIN_GITLAB_CI_ERROR,
                       PLUGIN_GITLAB_CI_ERROR_INVALID_ARGUMENT,
                       "array component input cannot be embedded in a string");
  return NULL;
}

static JsonNode *
interpolate (JsonNode    *node,
             GHashTable  *inputs,
             guint        depth,
             GError     **error)
{
  g_autoptr(JsonArray) result_array = NULL;
  g_autoptr(JsonObject) result_object = NULL;
  const char *string;

  g_assert (node != NULL);
  g_assert (inputs != NULL);

  if (depth > MAX_DEPTH)
    {
      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_LIMIT_EXCEEDED,
                           "component interpolation nesting limit exceeded");
      return NULL;
    }

  if ((string = string_value (node)))
    {
      g_autoptr(GString) expanded = g_string_new (NULL);
      g_autofree char *name = NULL;
      gsize match_start;
      gsize match_end;
      gsize offset = 0;
      gboolean found = FALSE;

      while (find_input (string, offset, &match_start, &match_end, &name))
        {
          JsonNode *input = g_hash_table_lookup (inputs, name);
          g_autofree char *embedded = NULL;

          found = TRUE;
          if (input == NULL)
            {
              g_set_error (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_INVALID_ARGUMENT,
                           "unknown component input '%s'",
                           name);
              return NULL;
            }

          if (match_start == 0 && match_end == strlen (string))
            return json_node_copy (input);

          if (!(embedded = input_to_string (input, error)))
            return NULL;

          g_string_append_len (expanded, string + offset, match_start - offset);
          g_string_append (expanded, embedded);

          if (expanded->len > MAX_EXPANDED)
            {
              g_set_error_literal (error,
                                   PLUGIN_GITLAB_CI_ERROR,
                                   PLUGIN_GITLAB_CI_ERROR_LIMIT_EXCEEDED,
                                   "component interpolation is too large");
              return NULL;
            }

          offset = match_end;
          g_clear_pointer (&name, g_free);
        }

      if (found)
        {
          g_string_append (expanded, string + offset);
          return json_node_init_string (json_node_alloc (), expanded->str);
        }

      return json_node_copy (node);
    }

  if (JSON_NODE_HOLDS_ARRAY (node))
    {
      JsonArray *array = json_node_get_array (node);
      guint length = json_array_get_length (array);

      result_array = json_array_new ();
      for (guint i = 0; i < length; i++)
        {
          JsonNode *child = interpolate (json_array_get_element (array, i), inputs, depth + 1, error);

          if (child == NULL)
            return NULL;

          json_array_add_element (result_array, child);
        }

      return json_node_init_array (json_node_alloc (), result_array);
    }

  if (JSON_NODE_HOLDS_OBJECT (node))
    {
      JsonObjectIter iter;
      const char *key;
      JsonNode *value;

      result_object = json_object_new ();
      json_object_iter_init (&iter, json_node_get_object (node));
      while (json_object_iter_next (&iter, &key, &value))
        {
          g_autoptr(JsonNode) key_node = json_node_init_string (json_node_alloc (), key);
          g_autoptr(JsonNode) expanded_key = NULL;
          JsonNode *expanded_value;
          const char *new_key;

          expanded_key = interpolate (key_node, inputs, depth + 1, error);
          if (expanded_key == NULL ||
              !(new_key = string_value (expanded_key)))
            {
              if (error == NULL || *error == NULL)
                g_set_error_literal (error,
                                     PLUGIN_GITLAB_CI_ERROR,
                                     PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                                     "component input produces a non-string key");
              return NULL;
            }

          if (json_object_has_member (result_object, new_key))
            {
              g_set_error (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                           "component interpolation creates duplicate key '%s'",
                           new_key);
              return NULL;
            }

          expanded_value = interpolate (value, inputs, depth + 1, error);
          if (expanded_value == NULL)
            return NULL;

          json_object_set_member (result_object, new_key, expanded_value);
        }

      return json_node_init_object (json_node_alloc (), result_object);
    }

  return json_node_copy (node);
}

JsonNode *
plugin_gitlab_ci_component_expand (GPtrArray  *documents,
                                   JsonNode   *supplied,
                                   GError    **error)
{
  g_autoptr(GHashTable) inputs = NULL;
  g_autoptr(JsonNode) body = NULL;
  JsonNode *header;
  JsonNode *declarations;

  g_return_val_if_fail (documents != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (documents->len < 2)
    {
      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                           "component must contain a spec header and body");
      return NULL;
    }

  header = g_ptr_array_index (documents, 0);

  if (!(declarations = member (member (header, "spec"), "inputs")))
    {
      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                           "component spec has no inputs");
      return NULL;
    }

  if (!(inputs = collect_inputs (declarations, supplied, error)))
    return NULL;

  for (guint i = 1; i < documents->len; i++)
    {
      g_autoptr(JsonNode) expanded = interpolate (g_ptr_array_index (documents, i), inputs, 0, error);

      if (expanded == NULL)
        return NULL;

      if (body == NULL)
        {
          body = g_steal_pointer (&expanded);
        }
      else
        {
          JsonNode *merged = plugin_gitlab_ci_json_merge (body, expanded);

          json_node_unref (g_steal_pointer (&body));
          body = merged;
        }
    }

  return g_steal_pointer (&body);
}
