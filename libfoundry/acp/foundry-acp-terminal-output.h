/* foundry-acp-terminal-output.h
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

#define FOUNDRY_TYPE_ACP_TERMINAL_OUTPUT (foundry_acp_terminal_output_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_FINAL_TYPE (FoundryAcpTerminalOutput, foundry_acp_terminal_output, FOUNDRY, ACP_TERMINAL_OUTPUT, GObject)

FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpTerminalOutput *foundry_acp_terminal_output_new             (const char               *text,
                                                                       gboolean                  truncated,
                                                                       gboolean                  has_exit_status,
                                                                       int                       exit_code,
                                                                       const char               *signal_name);
FOUNDRY_AVAILABLE_IN_1_2
char                     *foundry_acp_terminal_output_dup_text        (FoundryAcpTerminalOutput *self);
FOUNDRY_AVAILABLE_IN_1_2
gboolean                  foundry_acp_terminal_output_get_truncated   (FoundryAcpTerminalOutput *self);
FOUNDRY_AVAILABLE_IN_1_2
gboolean                  foundry_acp_terminal_output_has_exit_status (FoundryAcpTerminalOutput *self);
FOUNDRY_AVAILABLE_IN_1_2
int                       foundry_acp_terminal_output_get_exit_code   (FoundryAcpTerminalOutput *self);
FOUNDRY_AVAILABLE_IN_1_2
char                     *foundry_acp_terminal_output_dup_signal      (FoundryAcpTerminalOutput *self);

G_END_DECLS
