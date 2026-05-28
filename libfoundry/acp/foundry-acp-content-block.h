/* foundry-acp-content-block.h
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

#define FOUNDRY_TYPE_ACP_CONTENT_BLOCK (foundry_acp_content_block_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_FINAL_TYPE (FoundryAcpContentBlock, foundry_acp_content_block, FOUNDRY, ACP_CONTENT_BLOCK, GObject)

FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpContentBlock *foundry_acp_content_block_new_text          (const char             *text);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpContentBlock *foundry_acp_content_block_new_resource_link (const char             *uri,
                                                                     const char             *name,
                                                                     const char             *mime_type);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpContentBlock *foundry_acp_content_block_new_text_resource (const char             *uri,
                                                                     const char             *mime_type,
                                                                     const char             *text);
FOUNDRY_AVAILABLE_IN_1_2
char                   *foundry_acp_content_block_dup_text          (FoundryAcpContentBlock *self);
FOUNDRY_AVAILABLE_IN_1_2
char                   *foundry_acp_content_block_dup_uri           (FoundryAcpContentBlock *self);
FOUNDRY_AVAILABLE_IN_1_2
char                   *foundry_acp_content_block_dup_name          (FoundryAcpContentBlock *self);
FOUNDRY_AVAILABLE_IN_1_2
char                   *foundry_acp_content_block_dup_mime_type     (FoundryAcpContentBlock *self);

G_END_DECLS
