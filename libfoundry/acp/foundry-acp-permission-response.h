/* foundry-acp-permission-response.h
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

#define FOUNDRY_TYPE_ACP_PERMISSION_RESPONSE (foundry_acp_permission_response_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_FINAL_TYPE (FoundryAcpPermissionResponse, foundry_acp_permission_response, FOUNDRY, ACP_PERMISSION_RESPONSE, GObject)

FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpPermissionResponse *foundry_acp_permission_response_new_selected  (const char                   *option_id);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpPermissionResponse *foundry_acp_permission_response_new_cancelled (void);
FOUNDRY_AVAILABLE_IN_1_2
gboolean                      foundry_acp_permission_response_get_cancelled (FoundryAcpPermissionResponse *self);
FOUNDRY_AVAILABLE_IN_1_2
char                         *foundry_acp_permission_response_dup_option_id (FoundryAcpPermissionResponse *self);

G_END_DECLS
