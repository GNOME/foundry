/* foundry-dap-protocol-message-private.h
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

#pragma once

#include <json-glib/json-glib.h>

#include "foundry-dap-protocol-message.h"

G_BEGIN_DECLS

struct _FoundryDapProtocolMessage
{
  GObject parent_instance;
  gint64 seq;
};

struct _FoundryDapProtocolMessageClass
{
  GObjectClass parent_class;

  gboolean (*deserialize) (FoundryDapProtocolMessage  *message,
                           JsonObject          *object,
                           GError             **error);
  gboolean (*serialize)   (FoundryDapProtocolMessage  *message,
                           JsonObject          *object,
                           GError             **error);
};

GBytes                    *_foundry_dap_protocol_message_to_bytes   (FoundryDapProtocolMessage  *self,
                                                                     GError                    **error);
FoundryDapProtocolMessage *_foundry_dap_protocol_message_new_parsed (GType                       expected_type,
                                                                     JsonNode                   *node,
                                                                     GError                    **error);
gint64                     _foundry_dap_protocol_message_get_seq    (FoundryDapProtocolMessage  *self);
gboolean                   _foundry_dap_protocol_message_serialize  (FoundryDapProtocolMessage  *self,
                                                                     JsonObject                 *object,
                                                                     GError                    **error);

G_END_DECLS
