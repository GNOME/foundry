/* foundry-dap-response.c
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

#include "foundry-dap-response-private.h"

G_DEFINE_ABSTRACT_TYPE (FoundryDapResponse, foundry_dap_response, FOUNDRY_TYPE_DAP_PROTOCOL_MESSAGE)

static gboolean
foundry_dap_response_deserialize (FoundryDapProtocolMessage  *message,
                                  JsonObject                 *object,
                                  GError                    **error)
{
  FoundryDapResponse *self = FOUNDRY_DAP_RESPONSE (message);

  if (json_object_has_member (object, "body"))
    self->body = json_node_ref (json_object_get_member (object, "body"));

  return FOUNDRY_DAP_PROTOCOL_MESSAGE_CLASS (foundry_dap_response_parent_class)->deserialize (message, object, error);
}

static void
foundry_dap_response_finalize (GObject *object)
{
  FoundryDapResponse *self = (FoundryDapResponse *)object;

  g_clear_pointer (&self->body, json_node_unref);

  G_OBJECT_CLASS (foundry_dap_response_parent_class)->finalize (object);
}

static void
foundry_dap_response_class_init (FoundryDapResponseClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryDapProtocolMessageClass *protocol_message_class = FOUNDRY_DAP_PROTOCOL_MESSAGE_CLASS (klass);

  object_class->finalize = foundry_dap_response_finalize;

  protocol_message_class->deserialize = foundry_dap_response_deserialize;
}

static void
foundry_dap_response_init (FoundryDapResponse *self)
{
}

gint64
foundry_dap_response_get_request_seq (FoundryDapResponse *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_RESPONSE (self), 0);

  return self->request_seq;
}

/**
 * foundry_dap_response_get_body:
 * @self: a [class@Foundry.DapResponse]
 *
 * Returns: (transfer none) (nullable):
 */
JsonNode *
foundry_dap_response_get_body (FoundryDapResponse *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_RESPONSE (self), NULL);

  return self->body;
}
