/* foundry-dap-protocol-message.c
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

#include <gio/gio.h>

#include "foundry-dap-output-event.h"
#include "foundry-dap-protocol-message-private.h"
#include "foundry-dap-unknown-event.h"
#include "foundry-dap-unknown-request.h"
#include "foundry-dap-unknown-response.h"

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryDapProtocolMessage, foundry_dap_protocol_message, G_TYPE_OBJECT)

static GHashTable *event_map;

static GType
lookup_event_type (const char *event)
{
  GType gtype;

  if (event == NULL)
    return FOUNDRY_TYPE_DAP_UNKNOWN_EVENT;

  if G_UNLIKELY (event_map == NULL)
    {
      event_map = g_hash_table_new (g_str_hash, g_str_equal);
      g_hash_table_insert (event_map, (gpointer)"output", GSIZE_TO_POINTER (FOUNDRY_TYPE_DAP_OUTPUT_EVENT));
    }

  if (!(gtype = GPOINTER_TO_SIZE (g_hash_table_lookup (event_map, event))))
    return FOUNDRY_TYPE_DAP_UNKNOWN_EVENT;

  return gtype;
}

static GType
find_gtype_for_event (JsonObject *object)
{
  return lookup_event_type (json_object_get_string_member_with_default (object, "event", NULL));
}

static gboolean
foundry_dap_protocol_message_real_deserialize (FoundryDapProtocolMessage  *self,
                                               JsonObject                 *object,
                                               GError                    **error)
{
  if (!json_object_has_member (object, "type") ||
      !json_object_has_member (object, "seq"))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Invalid message received");
      return FALSE;
    }

  self->seq = json_object_get_int_member_with_default (object, "seq", 0);

  return TRUE;
}

static gboolean
foundry_dap_protocol_message_real_serialize (FoundryDapProtocolMessage  *self,
                                             JsonObject                 *object,
                                             GError                    **error)
{
  json_object_set_int_member (object, "seq", self->seq);
  return TRUE;
}

static void
foundry_dap_protocol_message_finalize (GObject *object)
{
  G_OBJECT_CLASS (foundry_dap_protocol_message_parent_class)->finalize (object);
}

static void
foundry_dap_protocol_message_class_init (FoundryDapProtocolMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_dap_protocol_message_finalize;

  klass->deserialize = foundry_dap_protocol_message_real_deserialize;
  klass->serialize = foundry_dap_protocol_message_real_serialize;
}

static void
foundry_dap_protocol_message_init (FoundryDapProtocolMessage *self)
{
}

gint64
_foundry_dap_protocol_message_get_seq (FoundryDapProtocolMessage *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_PROTOCOL_MESSAGE (self), 0);

  return 0;
}

GBytes *
_foundry_dap_protocol_message_to_bytes (FoundryDapProtocolMessage  *self,
                                        GError                    **error)
{
  g_autoptr(JsonGenerator) generator = NULL;
  g_autoptr(JsonObject) object = NULL;
  g_autoptr(JsonNode) root = NULL;
  g_autofree char *str = NULL;
  gsize len = 0;

  g_return_val_if_fail (FOUNDRY_IS_DAP_PROTOCOL_MESSAGE (self), NULL);

  object = json_object_new ();

  if (!_foundry_dap_protocol_message_serialize (self, object, error))
    return NULL;

  root = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (root, object);

  generator = json_generator_new ();
  json_generator_set_root (generator, root);
  str = json_generator_to_data (generator, &len);

  return g_bytes_new_take (g_steal_pointer (&str), len);
}

FoundryDapProtocolMessage *
_foundry_dap_protocol_message_new_parsed (GType      expected_gtype,
                                          JsonNode  *node,
                                          GError   **error)
{
  g_autoptr(FoundryDapProtocolMessage) message = NULL;
  g_autoptr(GTypeClass) klass = NULL;
  JsonObject *object;
  const char *type;
  JsonNode *member;

  g_return_val_if_fail (node != NULL, NULL);

  /* Make sure the root node is an object */
  if (!JSON_NODE_HOLDS_OBJECT (node) ||
      !(object = json_node_get_object (node)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "JSON node not an object");
      return NULL;
    }

  /* Discover the type for the message */
  if (!json_object_has_member (object, "type") ||
      !(member = json_object_get_member (object, "type")) ||
      !JSON_NODE_HOLDS_VALUE (member) ||
      !(type = json_node_get_string (member)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "message `type` is missing or invalid");
      return NULL;
    }

  /* Event types require us to peek at what subclass we
   * should be instantiating.
   */
  if (strcmp (type, "event") == 0)
    {
      expected_gtype = find_gtype_for_event (object);
    }
  else if (strcmp (type, "response") == 0)
    {
      if (expected_gtype == G_TYPE_INVALID)
        expected_gtype = FOUNDRY_TYPE_DAP_UNKNOWN_RESPONSE;
    }
  else if (strcmp (type, "request") == 0)
    {
      if (expected_gtype == G_TYPE_INVALID)
        expected_gtype = FOUNDRY_TYPE_DAP_UNKNOWN_REQUEST;
    }

  /* Make sure we can decode a proper type */
  if (expected_gtype == G_TYPE_INVALID ||
      !g_type_is_a (expected_gtype, FOUNDRY_TYPE_DAP_PROTOCOL_MESSAGE) ||
      G_TYPE_IS_ABSTRACT (expected_gtype))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Invalid message GType `%s`",
                   g_type_name (expected_gtype));
      return NULL;
    }

  /* Create the object and try to inflate it */
  message = g_object_new (expected_gtype, NULL);
  if (!FOUNDRY_DAP_PROTOCOL_MESSAGE_GET_CLASS (message)->deserialize (message, object, error))
    return NULL;

  return g_steal_pointer (&message);
}

gboolean
_foundry_dap_protocol_message_serialize (FoundryDapProtocolMessage  *self,
                                         JsonObject                 *object,
                                         GError                    **error)
{
  FoundryDapProtocolMessageClass *klass;

  g_return_val_if_fail (FOUNDRY_IS_DAP_PROTOCOL_MESSAGE (self), FALSE);
  g_return_val_if_fail (object != NULL, FALSE);

  klass = FOUNDRY_DAP_PROTOCOL_MESSAGE_GET_CLASS (self);

  if (klass->serialize)
    return klass->serialize (self, object, error);

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               "Not supported");

  return FALSE;
}
