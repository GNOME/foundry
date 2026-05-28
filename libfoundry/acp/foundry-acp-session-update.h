/* foundry-acp-session-update.h
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

#define FOUNDRY_TYPE_ACP_SESSION_UPDATE (foundry_acp_session_update_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_FINAL_TYPE (FoundryAcpSessionUpdate, foundry_acp_session_update, FOUNDRY, ACP_SESSION_UPDATE, GObject)

FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpSessionUpdateKind  foundry_acp_session_update_get_update_kind          (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_update_dup_kind                 (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_update_dup_text                 (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
GPtrArray                   *foundry_acp_session_update_dup_content_blocks       (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_update_dup_tool_call_id         (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_update_dup_tool_name            (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_update_dup_tool_title           (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_update_dup_tool_status          (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_update_dup_path                 (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_update_dup_old_path             (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
gboolean                     foundry_acp_session_update_get_line_range           (FoundryAcpSessionUpdate *self,
                                                                                  guint                   *begin_line,
                                                                                  guint                   *end_line);
FOUNDRY_AVAILABLE_IN_1_2
gboolean                     foundry_acp_session_update_get_byte_count           (FoundryAcpSessionUpdate *self,
                                                                                  gint64                  *byte_count);
FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_update_dup_edit_summary         (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_update_dup_terminal_id          (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_update_dup_command              (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_update_dup_cwd                  (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
gboolean                     foundry_acp_session_update_get_terminal_exit_status (FoundryAcpSessionUpdate *self,
                                                                                  int                     *exit_status);
FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_update_dup_progress_text        (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_update_dup_error_message        (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_update_dup_error_domain         (FoundryAcpSessionUpdate *self);
FOUNDRY_AVAILABLE_IN_1_2
gboolean                     foundry_acp_session_update_get_error_code           (FoundryAcpSessionUpdate *self,
                                                                                  int                     *error_code);
FOUNDRY_AVAILABLE_IN_1_2
JsonNode                    *foundry_acp_session_update_dup_raw                  (FoundryAcpSessionUpdate *self);

G_END_DECLS
