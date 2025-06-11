/* foundry-json-node.c
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

#include "foundry-json.h"
#include "foundry-json-node.h"

static JsonNode *
from_string (const char *valueptr)
{
  JsonNode *node = json_node_new (JSON_NODE_VALUE);
  json_node_set_string (node, valueptr);
  return node;
}

static JsonNode *
from_put_string (FoundryJsonNodePutString *valueptr)
{
  JsonNode *node = json_node_new (JSON_NODE_VALUE);
  json_node_set_string (node, valueptr->val);
  return node;
}

static JsonNode *
from_put_strv (FoundryJsonNodePutStrv *valueptr)
{
  return foundry_json_node_new_strv (valueptr->val);
}

static JsonNode *
from_put_double (FoundryJsonNodePutDouble *valueptr)
{
  JsonNode *node = json_node_new (JSON_NODE_VALUE);
  json_node_set_double (node, valueptr->val);
  return node;
}

static JsonNode *
from_put_int (FoundryJsonNodePutInt *valueptr)
{
  JsonNode *node = json_node_new (JSON_NODE_VALUE);
  json_node_set_int (node, valueptr->val);
  return node;
}

static JsonNode *
from_put_boolean (FoundryJsonNodePutBoolean *valueptr)
{
  JsonNode *node = json_node_new (JSON_NODE_VALUE);
  json_node_set_boolean (node, !!valueptr->val);
  return node;
}

static JsonNode *
from_put_node (FoundryJsonNodePutNode *valueptr)
{
  if (valueptr->val)
    return json_node_ref (valueptr->val);

  return json_node_new (JSON_NODE_NULL);
}

static JsonNode *
create_for_value (va_list *args)
{
  const char *valueptr = va_arg ((*args), const char *);

  if (valueptr[0] == ']' && valueptr[1] == 0)
    return NULL;

  if (valueptr[0] == '{' && valueptr[1] == 0)
    {
      g_autoptr(JsonObject) object = json_object_new ();
      const char *key;
      JsonNode *node;

      node = json_node_new (JSON_NODE_OBJECT);
      json_node_set_object (node, object);

      while ((key = va_arg ((*args), const char *))[0] != '}')
        {
          JsonNode *value = create_for_value (args);

          json_object_set_member (object, key, g_steal_pointer (&value));
        }

      return node;
    }
  else if (valueptr[0] == '[' && valueptr[1] == 0)
    {
      g_autoptr(JsonArray) array = json_array_new ();
      JsonNode *node;
      JsonNode *element;

      node = json_node_new (JSON_NODE_ARRAY);
      json_node_set_array (node, array);

      while ((element = create_for_value (args)))
        json_array_add_element (array, g_steal_pointer (&element));

      return node;
    }
  else if (strncmp (valueptr, _FOUNDRY_JSON_NODE_PUT_STRING_MAGIC, 8) == 0)
    return from_put_string ((FoundryJsonNodePutString *)(gpointer)valueptr);
  else if (strncmp (valueptr, _FOUNDRY_JSON_NODE_PUT_STRV_MAGIC, 8) == 0)
    return from_put_strv ((FoundryJsonNodePutStrv *)(gpointer)valueptr);
  else if (strncmp (valueptr, _FOUNDRY_JSON_NODE_PUT_DOUBLE_MAGIC, 8) == 0)
    return from_put_double ((FoundryJsonNodePutDouble *)(gpointer)valueptr);
  else if (strncmp (valueptr, _FOUNDRY_JSON_NODE_PUT_BOOLEAN_MAGIC, 8) == 0)
    return from_put_boolean ((FoundryJsonNodePutBoolean *)(gpointer)valueptr);
  else if (strncmp (valueptr, _FOUNDRY_JSON_NODE_PUT_INT_MAGIC, 8) == 0)
    return from_put_int ((FoundryJsonNodePutInt *)(gpointer)valueptr);
  else if (strncmp (valueptr, _FOUNDRY_JSON_NODE_PUT_NODE_MAGIC, 8) == 0)
    return from_put_node ((FoundryJsonNodePutNode *)(gpointer)valueptr);
  else
    return from_string (valueptr);
}

/**
 * _foundry_json_node_new:
 *
 * Returns: (transfer full):
 */
JsonNode *
_foundry_json_node_new (gpointer unused,
                        ...)
{
  JsonNode *ret;
  va_list args;

  g_return_val_if_fail (unused == NULL, NULL);

  va_start (args, unused);
  ret = create_for_value (&args);
  va_end (args);

  return ret;
}
