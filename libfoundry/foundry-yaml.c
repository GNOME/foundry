/* foundry-yaml.c
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

#include <yaml.h>

#include "foundry-yaml-private.h"

#define MAX_NODES 100000
#define MAX_DEPTH 64

typedef struct
{
  yaml_document_t          *document;
  GHashTable               *active;
  const char               *source;
  FoundryYamlParseFlags     flags;
  FoundryYamlParseTagFunc   tag_func;
  gpointer                  tag_data;
  guint                     nodes;
} ParseState;

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (yaml_parser_t, yaml_parser_delete)
G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (yaml_document_t, yaml_document_delete)

static JsonNode *parse_node (ParseState  *state,
                             int          id,
                             guint        depth,
                             GError     **error);

static gboolean
is_plain_literal (yaml_node_t *node,
                  const char  *lowercase,
                  const char  *titlecase,
                  const char  *uppercase,
                  gboolean     extended)
{
  const char *value;

  g_assert (node != NULL);
  g_assert (lowercase != NULL);
  g_assert (titlecase != NULL);
  g_assert (uppercase != NULL);

  if (node->data.scalar.style != YAML_PLAIN_SCALAR_STYLE)
    return FALSE;

  value = (char *)node->data.scalar.value;

  if (extended)
    return g_ascii_strcasecmp (value, lowercase) == 0;

  return g_str_equal (value, lowercase) ||
         g_str_equal (value, titlecase) ||
         g_str_equal (value, uppercase);
}

static JsonNode *
parse_scalar (ParseState *state,
              yaml_node_t *node)
{
  const char *value;
  JsonNode *result;
  gboolean extended;

  g_assert (state != NULL);
  g_assert (node != NULL);

  extended = (state->flags & FOUNDRY_YAML_PARSE_FLAGS_EXTENDED_LITERALS) != 0;
  value = (char *)node->data.scalar.value;
  result = json_node_alloc ();

  if ((extended &&
       node->tag != NULL &&
       g_str_equal ((char *)node->tag, YAML_NULL_TAG)) ||
      is_plain_literal (node, "null", "Null", "NULL", extended) ||
      (extended &&
       node->data.scalar.style == YAML_PLAIN_SCALAR_STYLE &&
       (value[0] == '\0' || g_str_equal (value, "~"))))
    {
      json_node_init_null (result);
    }
  else if (is_plain_literal (node, "true", "True", "TRUE", extended))
    {
      json_node_init_boolean (result, TRUE);
    }
  else if (is_plain_literal (node, "false", "False", "FALSE", extended))
    {
      json_node_init_boolean (result, FALSE);
    }
  else if ((state->flags & FOUNDRY_YAML_PARSE_FLAGS_COERCE_INTEGERS) != 0 &&
           node->data.scalar.style == YAML_PLAIN_SCALAR_STYLE &&
           value[0] != '\0')
    {
      char *endptr;
      gint64 number;

      number = g_ascii_strtoll (value, &endptr, 10);

      if (endptr[0] == '\0')
        {
          json_node_init_int (result, number);
        }
      else
        {
          if (endptr[0] == '.' && (endptr != value || endptr[1] != '\0'))
            {
              g_ascii_strtoll (endptr + 1, &endptr, 10);

              if (endptr[0] == '\0')
                g_warning ("%zu:%zu: '%s' will be parsed as a number by many YAML parsers",
                           node->start_mark.line + 1,
                           node->start_mark.column + 1,
                           value);
            }

          json_node_init_string (result, value);
        }
    }
  else
    {
      json_node_init_string (result, value);
    }

  return result;
}

static gboolean
merge_missing (JsonObject  *destination,
               JsonNode    *node,
               GError     **error)
{
  JsonObjectIter iter;
  JsonObject *source;
  const char *name;
  JsonNode *value;

  g_assert (destination != NULL);
  g_assert (node != NULL);

  if (!JSON_NODE_HOLDS_OBJECT (node))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "YAML merge value must be a mapping or sequence of mappings");
      return FALSE;
    }

  source = json_node_get_object (node);
  json_object_iter_init (&iter, source);

  while (json_object_iter_next (&iter, &name, &value))
    {
      if (!json_object_has_member (destination, name))
        json_object_set_member (destination, name, json_node_copy (value));
    }

  return TRUE;
}

static gboolean
apply_merge (JsonObject  *destination,
             JsonNode    *node,
             GError     **error)
{
  JsonArray *array;
  guint n_items;

  g_assert (destination != NULL);
  g_assert (node != NULL);

  if (JSON_NODE_HOLDS_OBJECT (node))
    return merge_missing (destination, node, error);

  if (!JSON_NODE_HOLDS_ARRAY (node))
    return merge_missing (destination, node, error);

  array = json_node_get_array (node);
  n_items = json_array_get_length (array);

  for (guint i = 0; i < n_items; i++)
    {
      if (!merge_missing (destination, json_array_get_element (array, i), error))
        return FALSE;
    }

  return TRUE;
}

static JsonNode *
parse_mapping (ParseState  *state,
               yaml_node_t *node,
               guint        depth,
               GError     **error)
{
  g_autoptr(GHashTable) explicit = NULL;
  g_autoptr(JsonObject) object = NULL;
  yaml_node_pair_t *pair;

  g_assert (state != NULL);
  g_assert (node != NULL);

  if ((state->flags & FOUNDRY_YAML_PARSE_FLAGS_REJECT_DUPLICATE_KEYS) != 0)
    explicit = g_hash_table_new (g_str_hash, g_str_equal);

  object = json_object_new ();

  if ((state->flags & FOUNDRY_YAML_PARSE_FLAGS_MERGE_KEYS) != 0)
    {
      for (pair = node->data.mapping.pairs.start;
           pair < node->data.mapping.pairs.top;
           pair++)
        {
          yaml_node_t *key = yaml_document_get_node (state->document, pair->key);

          if (key != NULL &&
              key->type == YAML_SCALAR_NODE &&
              g_str_equal ((char *)key->data.scalar.value, "<<"))
            {
              g_autoptr(JsonNode) value = NULL;

              value = parse_node (state, pair->value, depth + 1, error);

              if (value == NULL || !apply_merge (object, value, error))
                return NULL;
            }
        }
    }

  for (pair = node->data.mapping.pairs.start;
       pair < node->data.mapping.pairs.top;
       pair++)
    {
      yaml_node_t *key = yaml_document_get_node (state->document, pair->key);
      g_autoptr(JsonNode) value = NULL;
      const char *name;

      if (key == NULL || key->type != YAML_SCALAR_NODE)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "YAML mapping keys must be scalars");
          return NULL;
        }

      name = (char *)key->data.scalar.value;

      if ((state->flags & FOUNDRY_YAML_PARSE_FLAGS_MERGE_KEYS) != 0 &&
          g_str_equal (name, "<<"))
        continue;

      if (explicit != NULL && g_hash_table_contains (explicit, name))
        {
          if (state->source != NULL)
            g_set_error (error,
                         G_IO_ERROR,
                         G_IO_ERROR_FAILED,
                         "%s:%zu:%zu: duplicate mapping key '%s'",
                         state->source,
                         key->start_mark.line + 1,
                         key->start_mark.column + 1,
                         name);
          else
            g_set_error (error,
                         G_IO_ERROR,
                         G_IO_ERROR_FAILED,
                         "%zu:%zu: duplicate mapping key '%s'",
                         key->start_mark.line + 1,
                         key->start_mark.column + 1,
                         name);

          return NULL;
        }

      if (explicit != NULL)
        g_hash_table_add (explicit, (gpointer)name);

      if (!(value = parse_node (state, pair->value, depth + 1, error)))
        return NULL;

      json_object_set_member (object, name, g_steal_pointer (&value));
    }

  return json_node_init_object (json_node_alloc (), object);
}

static JsonNode *
parse_node (ParseState  *state,
            int          id,
            guint        depth,
            GError     **error)
{
  g_autoptr(JsonArray) array = NULL;
  g_autoptr(JsonNode) result = NULL;
  yaml_node_t *node;
  yaml_node_item_t *item;

  g_assert (state != NULL);
  g_assert (id > 0);

  if ((state->flags & FOUNDRY_YAML_PARSE_FLAGS_LIMIT_EXPANSION) != 0 &&
      (depth > MAX_DEPTH || ++state->nodes > MAX_NODES))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NO_SPACE,
                           "YAML expansion limit exceeded");
      return NULL;
    }

  if (g_hash_table_contains (state->active, GINT_TO_POINTER (id)))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "cyclic YAML alias");
      return NULL;
    }

  node = yaml_document_get_node (state->document, id);

  if (node == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "invalid YAML node");
      return NULL;
    }

  g_hash_table_add (state->active, GINT_TO_POINTER (id));

  if (node->type == YAML_NO_NODE)
    {
      result = json_node_init_null (json_node_alloc ());
    }
  else if (node->type == YAML_SCALAR_NODE)
    {
      result = parse_scalar (state, node);
    }
  else if (node->type == YAML_SEQUENCE_NODE)
    {
      array = json_array_new ();

      for (item = node->data.sequence.items.start;
           item < node->data.sequence.items.top;
           item++)
        {
          JsonNode *child = parse_node (state, *item, depth + 1, error);

          if (child == NULL)
            goto failure;

          json_array_add_element (array, child);
        }

      result = json_node_init_array (json_node_alloc (), array);
    }
  else if (node->type == YAML_MAPPING_NODE)
    {
      result = parse_mapping (state, node, depth, error);
    }
  else
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "invalid YAML node type");
      goto failure;
    }

  if (result != NULL &&
      state->tag_func != NULL &&
      node->tag != NULL &&
      node->tag[0] == '!')
    {
      if (!state->tag_func ((char *)node->tag, &result, state->tag_data, error))
        {
          if (error != NULL && *error == NULL)
            g_set_error_literal (error,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "YAML tag callback failed");
          goto failure;
        }

      if (result == NULL)
        {
          if (error != NULL && *error == NULL)
            g_set_error_literal (error,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "YAML tag callback returned no node");
          goto failure;
        }
    }

  g_hash_table_remove (state->active, GINT_TO_POINTER (id));
  return g_steal_pointer (&result);

failure:
  g_hash_table_remove (state->active, GINT_TO_POINTER (id));
  return NULL;
}

GPtrArray *
_foundry_yaml_parse (const char               *source,
                     GBytes                   *bytes,
                     FoundryYamlParseFlags     flags,
                     FoundryYamlParseTagFunc   tag_func,
                     gpointer                  tag_data,
                     GError                  **error)
{
  g_autoptr(GPtrArray) documents = NULL;
  g_auto(yaml_parser_t) parser = {0};
  const yaml_char_t *data;
  gsize size;

  g_return_val_if_fail (bytes != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (!yaml_parser_initialize (&parser))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "Failed to initialize YAML parser");
      return NULL;
    }

  documents = g_ptr_array_new_with_free_func ((GDestroyNotify)json_node_unref);
  data = g_bytes_get_data (bytes, &size);
  yaml_parser_set_input_string (&parser, data, size);

  for (;;)
    {
      g_autoptr(GHashTable) active = NULL;
      g_auto(yaml_document_t) document = {{0}};
      yaml_node_t *root;
      ParseState state = {0};
      JsonNode *converted;
      int root_id;

      if (!yaml_parser_load (&parser, &document))
        {
          if (source != NULL)
            g_set_error (error,
                         G_IO_ERROR,
                         G_IO_ERROR_FAILED,
                         "%s:%zu:%zu: %s",
                         source,
                         parser.problem_mark.line + 1,
                         parser.problem_mark.column + 1,
                         parser.problem);
          else
            g_set_error (error,
                         G_IO_ERROR,
                         G_IO_ERROR_FAILED,
                         "%zu:%zu: %s",
                         parser.problem_mark.line + 1,
                         parser.problem_mark.column + 1,
                         parser.problem);

          return NULL;
        }

      if (!(root = yaml_document_get_root_node (&document)))
        {
          if ((flags & FOUNDRY_YAML_PARSE_FLAGS_ALL_DOCUMENTS) != 0)
            break;

          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Document has no root node.");
          return NULL;
        }

      active = g_hash_table_new (g_direct_hash, g_direct_equal);
      state.document = &document;
      state.active = active;
      state.source = source;
      state.flags = flags;
      state.tag_func = tag_func;
      state.tag_data = tag_data;
      root_id = (int)(root - document.nodes.start) + 1;
      converted = parse_node (&state, root_id, 0, error);

      if (converted == NULL)
        return NULL;

      g_ptr_array_add (documents, converted);

      if ((flags & FOUNDRY_YAML_PARSE_FLAGS_ALL_DOCUMENTS) == 0)
        break;
    }

  return g_steal_pointer (&documents);
}
