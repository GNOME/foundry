/* foundry-acp-enums.c
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

#include "config.h"

#include "foundry-acp-enums.h"

G_DEFINE_FLAGS_TYPE (FoundryAcpAgentCapabilityFlags, foundry_acp_agent_capability_flags,
                     G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_AGENT_CAPABILITY_NONE, "none"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_AGENT_CAPABILITY_RESUME_SESSION, "resume-session"))

G_DEFINE_FLAGS_TYPE (FoundryAcpClientCapabilityFlags, foundry_acp_client_capability_flags,
                     G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CLIENT_CAPABILITY_NONE, "none"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CLIENT_CAPABILITY_FS_READ_TEXT_FILE, "fs-read-text-file"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CLIENT_CAPABILITY_FS_WRITE_TEXT_FILE, "fs-write-text-file"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CLIENT_CAPABILITY_TERMINAL, "terminal"))

G_DEFINE_FLAGS_TYPE (FoundryAcpChangedFileFlags, foundry_acp_changed_file_flags,
                     G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CHANGED_FILE_NONE, "none"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CHANGED_FILE_STAGED, "staged"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CHANGED_FILE_UNSTAGED, "unstaged"),
                     G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CHANGED_FILE_UNTRACKED, "untracked"))

G_DEFINE_ENUM_TYPE (FoundryAcpChangedFileKind, foundry_acp_changed_file_kind,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CHANGED_FILE_UNKNOWN, "unknown"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CHANGED_FILE_CREATED, "created"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CHANGED_FILE_MODIFIED, "modified"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CHANGED_FILE_DELETED, "deleted"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CHANGED_FILE_PATCHED, "patched"))

G_DEFINE_ENUM_TYPE (FoundryAcpConnectionState, foundry_acp_connection_state,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CONNECTION_NEW, "new"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CONNECTION_STARTING, "starting"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CONNECTION_INITIALIZING, "initializing"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CONNECTION_AUTH_REQUIRED, "auth-required"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CONNECTION_READY, "ready"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CONNECTION_CLOSING, "closing"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CONNECTION_CLOSED, "closed"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_CONNECTION_FAILED, "failed"))

G_DEFINE_ENUM_TYPE (FoundryAcpEventKind, foundry_acp_event_kind,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_UNKNOWN, "unknown"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_MESSAGE_CHUNK, "message-chunk"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_MESSAGE, "message"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_STEP, "step"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_TOOL_CALL, "tool-call"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_TOOL_UPDATE, "tool-update"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_TOOL_RESULT, "tool-result"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_PERMISSION_REQUEST, "permission-request"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_PERMISSION_RESPONSE, "permission-response"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_TERMINAL_CREATED, "terminal-created"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_TERMINAL_OUTPUT, "terminal-output"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_TERMINAL_EXITED, "terminal-exited"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_TERMINAL_RELEASED, "terminal-released"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_FILE_READ, "file-read"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_FILE_WRITE, "file-write"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_FILE_PATCH, "file-patch"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_FILE_CREATED, "file-created"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_FILE_DELETED, "file-deleted"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_MODE_CHANGED, "mode-changed"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_CONFIG_CHANGED, "config-changed"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_ERROR, "error"))

G_DEFINE_ENUM_TYPE (FoundryAcpEventState, foundry_acp_event_state,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_PENDING, "pending"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_RUNNING, "running"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_COMPLETED, "completed"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_FAILED, "failed"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_EVENT_CANCELLED, "cancelled"))

G_DEFINE_ENUM_TYPE (FoundryAcpSessionUpdateKind, foundry_acp_session_update_kind,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_UNKNOWN, "unknown"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_MESSAGE_CHUNK, "message-chunk"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_MESSAGE, "message"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_STEP, "step"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_TOOL_CALL, "tool-call"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_TOOL_UPDATE, "tool-update"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_TOOL_RESULT, "tool-result"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_TERMINAL_CREATED, "terminal-created"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_TERMINAL_OUTPUT, "terminal-output"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_TERMINAL_EXITED, "terminal-exited"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_FILE_READ, "file-read"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_FILE_WRITE, "file-write"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_FILE_PATCH, "file-patch"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_FILE_CREATED, "file-created"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_FILE_DELETED, "file-deleted"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_PROGRESS, "progress"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_UPDATE_ERROR, "error"))

G_DEFINE_ENUM_TYPE (FoundryAcpSessionState, foundry_acp_session_state,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_NEW, "new"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_LOADING, "loading"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_IDLE, "idle"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_RUNNING, "running"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_CANCELLING, "cancelling"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_SESSION_CLOSED, "closed"))

G_DEFINE_ENUM_TYPE (FoundryAcpTerminalState, foundry_acp_terminal_state,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_TERMINAL_RUNNING, "running"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_TERMINAL_EXITED, "exited"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_TERMINAL_FAILED, "failed"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_TERMINAL_CANCELLED, "cancelled"))

G_DEFINE_ENUM_TYPE (FoundryAcpStopReason, foundry_acp_stop_reason,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_STOP_END_TURN, "end-turn"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_STOP_MAX_TOKENS, "max-tokens"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_STOP_MAX_TURN_REQUESTS, "max-turn-requests"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_STOP_REFUSAL, "refusal"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_STOP_CANCELLED, "cancelled"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_ACP_STOP_UNKNOWN, "unknown"))

/**
 * foundry_acp_error_quark:
 *
 * Gets the ACP error domain.
 *
 * Returns: a [alias@GLib.Quark]
 *
 * Since: 1.2
 */
GQuark
foundry_acp_error_quark (void)
{
  return g_quark_from_static_string ("foundry-acp-error");
}
