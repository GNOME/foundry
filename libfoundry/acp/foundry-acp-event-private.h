/* foundry-acp-event-private.h
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "foundry-acp-event.h"

G_BEGIN_DECLS

FoundryAcpEvent *_foundry_acp_event_new_full     (const char           *session_id,
                                                  const char           *turn_id,
                                                  const char           *event_id,
                                                  const char           *parent_event_id,
                                                  FoundryAcpEventKind   kind,
                                                  const char           *raw_kind,
                                                  const char           *title,
                                                  const char           *body,
                                                  FoundryAcpEventState  state,
                                                  JsonNode             *raw);
void             _foundry_acp_event_set_turn_id  (FoundryAcpEvent      *self,
                                                  const char           *turn_id);
void             _foundry_acp_event_set_raw_kind (FoundryAcpEvent      *self,
                                                  const char           *raw_kind);
void             _foundry_acp_event_set_title    (FoundryAcpEvent      *self,
                                                  const char           *title);
void             _foundry_acp_event_set_body     (FoundryAcpEvent      *self,
                                                  const char           *body);
void             _foundry_acp_event_set_state    (FoundryAcpEvent      *self,
                                                  FoundryAcpEventState  state);
void             _foundry_acp_event_set_raw      (FoundryAcpEvent      *self,
                                                  JsonNode             *raw);

G_END_DECLS
