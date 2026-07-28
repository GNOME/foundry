/* plugin-gitlab-ci-yaml.c
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
#include "plugin-gitlab-ci-error-private.h"
#include "plugin-gitlab-ci-yaml-private.h"

#define REFERENCE_MEMBER "$gitlab-reference"

static gboolean
transform_tag (const char  *tag,
               JsonNode   **node,
               gpointer     user_data,
               GError     **error)
{
  g_autoptr(JsonObject) wrapper = NULL;

  g_assert (tag != NULL);
  g_assert (node != NULL);
  g_assert (*node != NULL);

  if (!g_str_equal (tag, "!reference"))
    return TRUE;

  wrapper = json_object_new ();
  json_object_set_member (wrapper, REFERENCE_MEMBER, g_steal_pointer (node));
  *node = json_node_init_object (json_node_alloc (), wrapper);

  return TRUE;
}

GPtrArray *
plugin_gitlab_ci_yaml_parse (const char  *source,
                             GBytes      *bytes,
                             GError     **error)
{
  g_autoptr(GError) local_error = NULL;
  g_autoptr(GPtrArray) documents = NULL;

  g_return_val_if_fail (source != NULL, NULL);
  g_return_val_if_fail (bytes != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  documents = _foundry_yaml_parse (
    source,
    bytes,
    (FOUNDRY_YAML_PARSE_FLAGS_EXTENDED_LITERALS |
     FOUNDRY_YAML_PARSE_FLAGS_MERGE_KEYS |
     FOUNDRY_YAML_PARSE_FLAGS_REJECT_DUPLICATE_KEYS |
     FOUNDRY_YAML_PARSE_FLAGS_LIMIT_EXPANSION |
     FOUNDRY_YAML_PARSE_FLAGS_ALL_DOCUMENTS),
    transform_tag,
    NULL,
    &local_error);

  if (documents == NULL)
    {
      PluginGitlabCiError code = PLUGIN_GITLAB_CI_ERROR_INVALID_DATA;

      if (local_error->domain == PLUGIN_GITLAB_CI_ERROR)
        {
          g_propagate_error (error, g_steal_pointer (&local_error));
          return NULL;
        }

      if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NO_SPACE))
        code = PLUGIN_GITLAB_CI_ERROR_LIMIT_EXCEEDED;

      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           code,
                           local_error->message);
      return NULL;
    }

  return g_steal_pointer (&documents);
}

JsonNode *
plugin_gitlab_ci_json_merge (JsonNode *base,
                             JsonNode *overlay)
{
  g_autoptr(JsonObject) result = NULL;
  JsonObjectIter iter;
  JsonObject *base_object;
  JsonObject *overlay_object;
  const char *name;
  JsonNode *value;

  g_return_val_if_fail (base != NULL, NULL);
  g_return_val_if_fail (overlay != NULL, NULL);

  if (!JSON_NODE_HOLDS_OBJECT (base) || !JSON_NODE_HOLDS_OBJECT (overlay))
    return json_node_copy (overlay);

  result = json_object_new ();
  base_object = json_node_get_object (base);
  overlay_object = json_node_get_object (overlay);

  json_object_iter_init (&iter, base_object);
  while (json_object_iter_next (&iter, &name, &value))
    json_object_set_member (result, name, json_node_copy (value));

  json_object_iter_init (&iter, overlay_object);
  while (json_object_iter_next (&iter, &name, &value))
    {
      JsonNode *previous = json_object_get_member (result, name);

      if (previous != NULL &&
          JSON_NODE_HOLDS_OBJECT (previous) &&
          JSON_NODE_HOLDS_OBJECT (value))
        json_object_set_member (result, name, plugin_gitlab_ci_json_merge (previous, value));
      else
        json_object_set_member (result, name, json_node_copy (value));
    }

  return json_node_init_object (json_node_alloc (), result);
}

char *
plugin_gitlab_ci_json_format (JsonNode *node)
{
  g_autoptr(JsonGenerator) generator = NULL;

  g_return_val_if_fail (node != NULL, NULL);

  generator = json_generator_new ();
  json_generator_set_root (generator, node);
  json_generator_set_pretty (generator, TRUE);

  return json_generator_to_data (generator, NULL);
}
