/* foundry-acp-project-client.h
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

#define FOUNDRY_TYPE_ACP_PROJECT_CLIENT (foundry_acp_project_client_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_FINAL_TYPE (FoundryAcpProjectClient, foundry_acp_project_client, FOUNDRY, ACP_PROJECT_CLIENT, GObject)

FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpProjectClient    *foundry_acp_project_client_new                       (FoundryContext          *context);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpProjectClient    *foundry_acp_project_client_new_for_project_directory (GFile                   *project_directory);
FOUNDRY_AVAILABLE_IN_1_2
GFile                      *foundry_acp_project_client_dup_project_directory     (FoundryAcpProjectClient *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpPermissionPolicy *foundry_acp_project_client_dup_permission_policy     (FoundryAcpProjectClient *self);
FOUNDRY_AVAILABLE_IN_1_2
DexFuture                  *foundry_acp_project_client_refresh_changed_files     (FoundryAcpProjectClient *self,
                                                                                  FoundryAcpSession       *session) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
