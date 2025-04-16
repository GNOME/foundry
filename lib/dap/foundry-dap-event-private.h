/* foundry-dap-event-private.h
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

#include "foundry-dap-event.h"
#include "foundry-dap-protocol-message-private.h"

G_BEGIN_DECLS

struct _FoundryDapEvent
{
  FoundryDapProtocolMessage parent_instance;
  JsonNode *body;
};

struct _FoundryDapEventClass
{
  FoundryDapProtocolMessageClass parent_class;
};

static inline const char *
foundry_dap_event_get_body_member_string (FoundryDapEvent *self,
                                          const char      *member)
{
  JsonObject *obj;

  if (self->body == NULL)
    return NULL;

  if (!(obj = json_node_get_object (self->body)))
    return NULL;

  return json_object_get_string_member_with_default (obj, member, NULL);
}

static inline int
foundry_dap_event_get_body_member_int (FoundryDapEvent *self,
                                       const char      *member)
{
  JsonObject *obj;

  if (self->body == NULL)
    return 0;

  if (!(obj = json_node_get_object (self->body)))
    return 0;

  return json_object_get_int_member_with_default (obj, member, 0);
}

static inline JsonNode *
foundry_dap_event_get_body_member (FoundryDapEvent *self,
                                   const char      *member)
{
  JsonObject *obj;

  if (self->body == NULL)
    return NULL;

  if (!(obj = json_node_get_object (self->body)))
    return NULL;

  if (!json_object_has_member (obj, member))
    return NULL;

  return json_object_get_member (obj, member);
}

G_END_DECLS
