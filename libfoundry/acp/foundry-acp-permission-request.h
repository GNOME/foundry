/* foundry-acp-permission-request.h
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

#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_ACP_PERMISSION_REQUEST (foundry_acp_permission_request_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_FINAL_TYPE (FoundryAcpPermissionRequest, foundry_acp_permission_request, FOUNDRY, ACP_PERMISSION_REQUEST, GObject)

FOUNDRY_AVAILABLE_IN_1_2
char       *foundry_acp_permission_request_dup_session_id     (FoundryAcpPermissionRequest *self);
FOUNDRY_AVAILABLE_IN_1_2
char       *foundry_acp_permission_request_dup_request_id     (FoundryAcpPermissionRequest *self);
FOUNDRY_AVAILABLE_IN_1_2
char       *foundry_acp_permission_request_dup_tool_call_id   (FoundryAcpPermissionRequest *self);
FOUNDRY_AVAILABLE_IN_1_2
char       *foundry_acp_permission_request_dup_title          (FoundryAcpPermissionRequest *self);
FOUNDRY_AVAILABLE_IN_1_2
char       *foundry_acp_permission_request_dup_description    (FoundryAcpPermissionRequest *self);
FOUNDRY_AVAILABLE_IN_1_2
char       *foundry_acp_permission_request_dup_tool_name      (FoundryAcpPermissionRequest *self);
FOUNDRY_AVAILABLE_IN_1_2
char       *foundry_acp_permission_request_dup_command        (FoundryAcpPermissionRequest *self);
FOUNDRY_AVAILABLE_IN_1_2
char       *foundry_acp_permission_request_dup_path           (FoundryAcpPermissionRequest *self);
FOUNDRY_AVAILABLE_IN_1_2
char       *foundry_acp_permission_request_dup_default_option (FoundryAcpPermissionRequest *self);
FOUNDRY_AVAILABLE_IN_1_2
char       *foundry_acp_permission_request_dup_risk_level     (FoundryAcpPermissionRequest *self);
FOUNDRY_AVAILABLE_IN_1_2
GListModel *foundry_acp_permission_request_list_options       (FoundryAcpPermissionRequest *self);
FOUNDRY_AVAILABLE_IN_1_2
JsonNode   *foundry_acp_permission_request_dup_raw            (FoundryAcpPermissionRequest *self);

G_END_DECLS
