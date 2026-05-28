/* foundry-acp-permission-option.h
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

#define FOUNDRY_TYPE_ACP_PERMISSION_OPTION (foundry_acp_permission_option_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_FINAL_TYPE (FoundryAcpPermissionOption, foundry_acp_permission_option, FOUNDRY, ACP_PERMISSION_OPTION, GObject)

FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpPermissionOption *foundry_acp_permission_option_new             (const char                 *id,
                                                                           const char                 *label,
                                                                           const char                 *description,
                                                                           gboolean                    destructive);
FOUNDRY_AVAILABLE_IN_1_2
char                       *foundry_acp_permission_option_dup_id          (FoundryAcpPermissionOption *self);
FOUNDRY_AVAILABLE_IN_1_2
char                       *foundry_acp_permission_option_dup_label       (FoundryAcpPermissionOption *self);
FOUNDRY_AVAILABLE_IN_1_2
char                       *foundry_acp_permission_option_dup_description (FoundryAcpPermissionOption *self);
FOUNDRY_AVAILABLE_IN_1_2
gboolean                    foundry_acp_permission_option_get_destructive (FoundryAcpPermissionOption *self);
FOUNDRY_AVAILABLE_IN_1_2
JsonNode                   *foundry_acp_permission_option_dup_raw         (FoundryAcpPermissionOption *self);

G_END_DECLS
