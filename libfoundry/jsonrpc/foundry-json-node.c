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
create_node_for_arg (const char *valueptr)
{
  if (strncmp (valueptr, _FOUNDRY_JSON_NODE_PUT_STRING_MAGIC, 8) == 0)
    return from_put_string ((FoundryJsonNodePutString *)(gpointer)valueptr);
  else if (strncmp (valueptr, _FOUNDRY_JSON_NODE_PUT_STRV_MAGIC, 8) == 0)
    return from_put_strv ((FoundryJsonNodePutStrv *)(gpointer)valueptr);
  else if (strncmp (valueptr, _FOUNDRY_JSON_NODE_PUT_DOUBLE_MAGIC, 8) == 0)
    return from_put_double ((FoundryJsonNodePutDouble *)(gpointer)valueptr);
  else if (strncmp (valueptr, _FOUNDRY_JSON_NODE_PUT_BOOLEAN_MAGIC, 8) == 0)
    return from_put_boolean ((FoundryJsonNodePutBoolean *)(gpointer)valueptr);
  else if (strncmp (valueptr, _FOUNDRY_JSON_NODE_PUT_INT_MAGIC, 8) == 0)
    return from_put_int ((FoundryJsonNodePutInt *)(gpointer)valueptr);
  else
    return from_string (valueptr);
}

static gboolean
foundry_json_object_populate_recurse (JsonObject *object,
                                      const char *key,
                                      va_list    *args)
{
  const char *valueptr = va_arg ((*args), const char *);
  JsonNode *member;

  g_assert (key != NULL);
  g_assert (valueptr != NULL);

  if (valueptr[0] == '{' && valueptr[1] == 0)
    {
      const char *subkey;
      JsonObject *subobject = json_object_new ();

      while ((subkey = va_arg ((*args), const char *))[0] != '}')
        {
          if (foundry_json_object_populate_recurse (subobject, subkey, args))
            break;
        }

      member = json_node_new (JSON_NODE_OBJECT);
      json_node_set_object (member, subobject);

      g_assert (subkey != NULL);
      g_assert (subkey[0] == '}');

      json_object_set_member (object, key, g_steal_pointer (&member));

      return TRUE;
    }

  member = create_node_for_arg (valueptr);
  json_object_set_member (object, key, g_steal_pointer (&member));

  return FALSE;
}

static JsonNode *
foundry_json_object_new_va (const char *first_field,
                            va_list    *args)
{
  const char *key;
  JsonObject *object;
  JsonNode *node;

  g_return_val_if_fail (first_field != NULL, NULL);

  node = json_node_new (JSON_NODE_OBJECT);
  object = json_object_new ();

  key = first_field;

  do
    foundry_json_object_populate_recurse (object, key, args);
  while ((key = va_arg ((*args), const char *)));

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
  ret = foundry_json_object_new_va (first_field, &args);
  va_end (args);

  return ret;
}
