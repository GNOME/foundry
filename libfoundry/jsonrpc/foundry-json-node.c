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
from_put_double (FoundryJsonNodePutDouble *valueptr)
{
  JsonNode *node = json_node_new (JSON_NODE_VALUE);
  json_node_set_double (node, valueptr->val);
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
create_node_for_arg (const char *valueptr)
{
  if (strncmp (valueptr, _FOUNDRY_JSON_NODE_PUT_STRING_MAGIC, 8) == 0)
    return from_put_string ((FoundryJsonNodePutString *)(gpointer)valueptr);
  else if (strncmp (valueptr, _FOUNDRY_JSON_NODE_PUT_DOUBLE_MAGIC, 8) == 0)
    return from_put_double ((FoundryJsonNodePutDouble *)(gpointer)valueptr);
  else if (strncmp (valueptr, _FOUNDRY_JSON_NODE_PUT_BOOLEAN_MAGIC, 8) == 0)
    return from_put_boolean ((FoundryJsonNodePutBoolean *)(gpointer)valueptr);
  else
    return from_string (valueptr);
}

static void
foundry_json_object_populate_recurse (JsonObject *object,
                                      const char *key,
                                      va_list    *args)
{
  const char *valueptr = va_arg ((*args), const char *);
  JsonNode *member;

  g_return_if_fail (key != NULL);
  g_return_if_fail (valueptr != NULL);

  member = create_node_for_arg (valueptr);
  json_object_set_member (object, key, g_steal_pointer (&member));

  if ((key = va_arg ((*args), const char *)))
    foundry_json_object_populate_recurse (object, key, args);
}

static JsonNode *
foundry_json_object_new_va (const char *first_field,
                            va_list     args)
{
  JsonObject *object;
  JsonNode *node;

  g_return_val_if_fail (first_field != NULL, NULL);

  node = json_node_new (JSON_NODE_OBJECT);
  object = json_object_new ();

  foundry_json_object_populate_recurse (object, first_field, &args);

  json_node_set_object (node, object);
  json_object_unref (object);

  return node;
}

/**
 * foundry_json_node_new:
 *
 * Returns: (transfer full):
 */
JsonNode *
foundry_json_object_new (const char *first_field,
                         ...)
{
  JsonNode *ret;
  va_list args;

  va_start (args, first_field);
  ret = foundry_json_object_new_va (first_field, args);
  va_end (args);

  return ret;
}
