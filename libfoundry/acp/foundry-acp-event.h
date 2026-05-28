/* foundry-acp-event.h
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

#include <json-glib/json-glib.h>

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_ACP_EVENT (foundry_acp_event_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_FINAL_TYPE (FoundryAcpEvent, foundry_acp_event, FOUNDRY, ACP_EVENT, GObject)

FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpEvent      *foundry_acp_event_new                 (const char          *session_id,
                                                             FoundryAcpEventKind  kind);
FOUNDRY_AVAILABLE_IN_1_2
char                 *foundry_acp_event_dup_session_id      (FoundryAcpEvent     *self);
FOUNDRY_AVAILABLE_IN_1_2
char                 *foundry_acp_event_dup_turn_id         (FoundryAcpEvent     *self);
FOUNDRY_AVAILABLE_IN_1_2
char                 *foundry_acp_event_dup_event_id        (FoundryAcpEvent     *self);
FOUNDRY_AVAILABLE_IN_1_2
char                 *foundry_acp_event_dup_parent_event_id (FoundryAcpEvent     *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpEventKind   foundry_acp_event_get_kind            (FoundryAcpEvent     *self);
FOUNDRY_AVAILABLE_IN_1_2
char                 *foundry_acp_event_dup_raw_kind        (FoundryAcpEvent     *self);
FOUNDRY_AVAILABLE_IN_1_2
char                 *foundry_acp_event_dup_title           (FoundryAcpEvent     *self);
FOUNDRY_AVAILABLE_IN_1_2
char                 *foundry_acp_event_dup_body            (FoundryAcpEvent     *self);
FOUNDRY_AVAILABLE_IN_1_2
gint64                foundry_acp_event_get_timestamp       (FoundryAcpEvent     *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpEventState  foundry_acp_event_get_state           (FoundryAcpEvent     *self);
FOUNDRY_AVAILABLE_IN_1_2
JsonNode             *foundry_acp_event_dup_raw             (FoundryAcpEvent     *self);

G_END_DECLS
