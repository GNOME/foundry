/* foundry-dap-request.c
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

#include "foundry-dap-request-private.h"
#include "foundry-dap-unknown-response.h"

G_DEFINE_TYPE (FoundryDapRequest, foundry_dap_request, FOUNDRY_TYPE_DAP_PROTOCOL_MESSAGE)

static gboolean
foundry_dap_request_serialize (FoundryDapProtocolMessage  *message,
                               JsonObject                 *object,
                               GError                    **error)
{
  FoundryDapRequest *self = FOUNDRY_DAP_REQUEST (message);
  const char *command;

  if ((command = _foundry_dap_request_get_command (self)))
    json_object_set_string_member (object, "command", command);

  return FOUNDRY_DAP_PROTOCOL_MESSAGE_CLASS (foundry_dap_request_parent_class)->serialize (message, object, error);
}

static void
foundry_dap_request_class_init (FoundryDapRequestClass *klass)
{
  FoundryDapProtocolMessageClass *protocol_message_class = FOUNDRY_DAP_PROTOCOL_MESSAGE_CLASS (klass);

  protocol_message_class->serialize = foundry_dap_request_serialize;
}

static void
foundry_dap_request_init (FoundryDapRequest *self)
{
}

GType
_foundry_dap_request_get_response_type (FoundryDapRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_REQUEST (self), G_TYPE_INVALID);

  if (FOUNDRY_DAP_REQUEST_GET_CLASS (self)->get_response_type)
    return FOUNDRY_DAP_REQUEST_GET_CLASS (self)->get_response_type (self);

  return FOUNDRY_TYPE_DAP_UNKNOWN_RESPONSE;
}

const char *
_foundry_dap_request_get_command (FoundryDapRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_REQUEST (self), NULL);

  if (FOUNDRY_DAP_REQUEST_GET_CLASS (self)->get_command)
    return FOUNDRY_DAP_REQUEST_GET_CLASS (self)->get_command (self);

  return NULL;
}
