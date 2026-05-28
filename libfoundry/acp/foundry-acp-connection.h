/* foundry-acp-connection.h
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

#define FOUNDRY_TYPE_ACP_CONNECTION (foundry_acp_connection_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_FINAL_TYPE (FoundryAcpConnection, foundry_acp_connection, FOUNDRY, ACP_CONNECTION, GObject)

FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpConnection      *foundry_acp_connection_new               (FoundryContext                  *context,
                                                                     FoundryAcpAgent                 *agent,
                                                                     FoundryAcpClient                *client);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpConnection      *foundry_acp_connection_new_for_pipeline  (FoundryContext                  *context,
                                                                     FoundryAcpAgent                 *agent,
                                                                     FoundryAcpClient                *client,
                                                                     FoundryBuildPipeline            *pipeline);
FOUNDRY_AVAILABLE_IN_1_2
DexFuture                 *foundry_acp_connection_start             (FoundryAcpConnection            *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture                 *foundry_acp_connection_initialize        (FoundryAcpConnection            *self,
                                                                     const char                      *client_name,
                                                                     const char                      *client_title,
                                                                     const char                      *client_version,
                                                                     FoundryAcpClientCapabilityFlags  client_capabilities) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture                 *foundry_acp_connection_authenticate      (FoundryAcpConnection            *self,
                                                                     const char                      *method_id) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture                 *foundry_acp_connection_new_session       (FoundryAcpConnection            *self,
                                                                     const char                      *cwd) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture                 *foundry_acp_connection_await             (FoundryAcpConnection            *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture                 *foundry_acp_connection_close             (FoundryAcpConnection            *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpConnectionState  foundry_acp_connection_get_state         (FoundryAcpConnection            *self);
FOUNDRY_AVAILABLE_IN_1_2
char                      *foundry_acp_connection_dup_agent_name    (FoundryAcpConnection            *self);
FOUNDRY_AVAILABLE_IN_1_2
char                      *foundry_acp_connection_dup_agent_title   (FoundryAcpConnection            *self);
FOUNDRY_AVAILABLE_IN_1_2
char                      *foundry_acp_connection_dup_agent_version (FoundryAcpConnection            *self);

G_END_DECLS
