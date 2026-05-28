/* foundry-acp-session-private.h
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

#include "foundry-acp-session.h"

G_BEGIN_DECLS

FoundryAcpSession  *_foundry_acp_session_new                           (FoundryAcpConnection        *connection,
                                                                        const char                  *id);
const char         *_foundry_acp_session_get_id                        (FoundryAcpSession           *self);
const char         *_foundry_acp_session_get_current_turn_id           (FoundryAcpSession           *self);
void                _foundry_acp_session_set_state                     (FoundryAcpSession           *self,
                                                                        FoundryAcpSessionState       state);
void                _foundry_acp_session_append_event                  (FoundryAcpSession           *self,
                                                                        FoundryAcpEvent             *event);
void                _foundry_acp_session_append_update                 (FoundryAcpSession           *self,
                                                                        FoundryAcpSessionUpdate     *update);
FoundryAcpTerminal *_foundry_acp_session_lookup_terminal               (FoundryAcpSession           *self,
                                                                        const char                  *terminal_id);
void                _foundry_acp_session_add_terminal                  (FoundryAcpSession           *self,
                                                                        FoundryAcpTerminal          *terminal);
void                _foundry_acp_session_remove_terminal               (FoundryAcpSession           *self,
                                                                        const char                  *terminal_id);
void                _foundry_acp_session_note_changed_file             (FoundryAcpSession           *self,
                                                                        const char                  *path,
                                                                        FoundryAcpChangedFileKind    kind,
                                                                        const char                  *last_event_id,
                                                                        FoundryAcpChangedFileFlags   flags);
void                _foundry_acp_session_set_active_permission_request (FoundryAcpSession           *self,
                                                                        FoundryAcpPermissionRequest *request);

G_END_DECLS
