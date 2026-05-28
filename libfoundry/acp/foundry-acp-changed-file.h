/* foundry-acp-changed-file.h
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

#define FOUNDRY_TYPE_ACP_CHANGED_FILE (foundry_acp_changed_file_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_FINAL_TYPE (FoundryAcpChangedFile, foundry_acp_changed_file, FOUNDRY, ACP_CHANGED_FILE, GObject)

FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpChangedFile      *foundry_acp_changed_file_new               (const char                 *path,
                                                                        FoundryAcpChangedFileKind   kind,
                                                                        const char                 *last_event_id,
                                                                        FoundryAcpChangedFileFlags  flags);
FOUNDRY_AVAILABLE_IN_1_2
char                       *foundry_acp_changed_file_dup_path          (FoundryAcpChangedFile      *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpChangedFileKind   foundry_acp_changed_file_get_kind          (FoundryAcpChangedFile      *self);
FOUNDRY_AVAILABLE_IN_1_2
char                       *foundry_acp_changed_file_dup_last_event_id (FoundryAcpChangedFile      *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpChangedFileFlags  foundry_acp_changed_file_get_flags         (FoundryAcpChangedFile      *self);

G_END_DECLS
