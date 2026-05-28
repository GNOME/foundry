/* foundry-acp-enums.h
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

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_ACP_PROTOCOL_VERSION 1

#define FOUNDRY_ACP_ERROR (foundry_acp_error_quark())

#define FOUNDRY_TYPE_ACP_AGENT_CAPABILITY_FLAGS (foundry_acp_agent_capability_flags_get_type())
#define FOUNDRY_TYPE_ACP_CLIENT_CAPABILITY_FLAGS (foundry_acp_client_capability_flags_get_type())
#define FOUNDRY_TYPE_ACP_CHANGED_FILE_FLAGS (foundry_acp_changed_file_flags_get_type())
#define FOUNDRY_TYPE_ACP_CHANGED_FILE_KIND (foundry_acp_changed_file_kind_get_type())
#define FOUNDRY_TYPE_ACP_CONNECTION_STATE (foundry_acp_connection_state_get_type())
#define FOUNDRY_TYPE_ACP_EVENT_KIND (foundry_acp_event_kind_get_type())
#define FOUNDRY_TYPE_ACP_EVENT_STATE (foundry_acp_event_state_get_type())
#define FOUNDRY_TYPE_ACP_SESSION_STATE (foundry_acp_session_state_get_type())
#define FOUNDRY_TYPE_ACP_SESSION_UPDATE_KIND (foundry_acp_session_update_kind_get_type())
#define FOUNDRY_TYPE_ACP_TERMINAL_STATE (foundry_acp_terminal_state_get_type())
#define FOUNDRY_TYPE_ACP_STOP_REASON (foundry_acp_stop_reason_get_type())

FOUNDRY_AVAILABLE_IN_1_2
GQuark foundry_acp_error_quark                      (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_1_2
GType  foundry_acp_agent_capability_flags_get_type  (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_1_2
GType  foundry_acp_client_capability_flags_get_type (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_1_2
GType  foundry_acp_changed_file_flags_get_type      (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_1_2
GType  foundry_acp_changed_file_kind_get_type       (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_1_2
GType  foundry_acp_connection_state_get_type        (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_1_2
GType  foundry_acp_event_kind_get_type              (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_1_2
GType  foundry_acp_event_state_get_type             (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_1_2
GType  foundry_acp_session_state_get_type           (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_1_2
GType  foundry_acp_session_update_kind_get_type     (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_1_2
GType  foundry_acp_terminal_state_get_type          (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_1_2
GType  foundry_acp_stop_reason_get_type             (void) G_GNUC_CONST;

G_END_DECLS
