/* foundry-acp-session.h
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

#include <libdex.h>

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_ACP_SESSION (foundry_acp_session_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_FINAL_TYPE (FoundryAcpSession, foundry_acp_session, FOUNDRY, ACP_SESSION, GObject)

FOUNDRY_AVAILABLE_IN_1_2
char                        *foundry_acp_session_dup_id                        (FoundryAcpSession              *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpSessionState       foundry_acp_session_get_state                     (FoundryAcpSession              *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpConnection        *foundry_acp_session_dup_connection                (FoundryAcpSession              *self);
FOUNDRY_AVAILABLE_IN_1_2
GListModel                  *foundry_acp_session_list_events                   (FoundryAcpSession              *self);
FOUNDRY_AVAILABLE_IN_1_2
GListModel                  *foundry_acp_session_list_active_terminals         (FoundryAcpSession              *self);
FOUNDRY_AVAILABLE_IN_1_2
GListModel                  *foundry_acp_session_list_changed_files            (FoundryAcpSession              *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpEvent             *foundry_acp_session_dup_current_activity          (FoundryAcpSession              *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpPermissionRequest *foundry_acp_session_dup_active_permission_request (FoundryAcpSession              *self);
FOUNDRY_AVAILABLE_IN_1_2
DexFuture                   *foundry_acp_session_prompt                        (FoundryAcpSession              *self,
                                                                                FoundryAcpContentBlock * const *blocks,
                                                                                guint                           n_blocks) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture                   *foundry_acp_session_cancel                        (FoundryAcpSession              *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture                   *foundry_acp_session_close                         (FoundryAcpSession              *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture                   *foundry_acp_session_set_mode                      (FoundryAcpSession              *self,
                                                                                const char                     *mode_id) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture                   *foundry_acp_session_set_config_option             (FoundryAcpSession              *self,
                                                                                const char                     *config_id,
                                                                                const char                     *value_id) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
