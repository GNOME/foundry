/* foundry-acp-terminal.h
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

#define FOUNDRY_TYPE_ACP_TERMINAL (foundry_acp_terminal_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_FINAL_TYPE (FoundryAcpTerminal, foundry_acp_terminal, FOUNDRY, ACP_TERMINAL, GObject)

FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpTerminal       *foundry_acp_terminal_new                   (const char         *id);
FOUNDRY_AVAILABLE_IN_1_2
char                     *foundry_acp_terminal_dup_id                (FoundryAcpTerminal *self);
FOUNDRY_AVAILABLE_IN_1_2
char                     *foundry_acp_terminal_dup_turn_id           (FoundryAcpTerminal *self);
FOUNDRY_AVAILABLE_IN_1_2
char                     *foundry_acp_terminal_dup_command           (FoundryAcpTerminal *self);
FOUNDRY_AVAILABLE_IN_1_2
char                    **foundry_acp_terminal_dup_argv              (FoundryAcpTerminal *self);
FOUNDRY_AVAILABLE_IN_1_2
char                     *foundry_acp_terminal_dup_cwd               (FoundryAcpTerminal *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpTerminalState   foundry_acp_terminal_get_state             (FoundryAcpTerminal *self);
FOUNDRY_AVAILABLE_IN_1_2
gint64                    foundry_acp_terminal_get_created_at        (FoundryAcpTerminal *self);
FOUNDRY_AVAILABLE_IN_1_2
gint64                    foundry_acp_terminal_get_started_at        (FoundryAcpTerminal *self);
FOUNDRY_AVAILABLE_IN_1_2
gint64                    foundry_acp_terminal_get_exited_at         (FoundryAcpTerminal *self);
FOUNDRY_AVAILABLE_IN_1_2
gssize                    foundry_acp_terminal_get_output_byte_limit (FoundryAcpTerminal *self);
FOUNDRY_AVAILABLE_IN_1_2
gboolean                  foundry_acp_terminal_get_truncated         (FoundryAcpTerminal *self);
FOUNDRY_AVAILABLE_IN_1_2
char                     *foundry_acp_terminal_dup_latest_output     (FoundryAcpTerminal *self);
FOUNDRY_AVAILABLE_IN_1_2
char                     *foundry_acp_terminal_dup_scrollback        (FoundryAcpTerminal *self);
FOUNDRY_AVAILABLE_IN_1_2
gboolean                  foundry_acp_terminal_has_exit_status       (FoundryAcpTerminal *self);
FOUNDRY_AVAILABLE_IN_1_2
int                       foundry_acp_terminal_get_exit_status       (FoundryAcpTerminal *self);
FOUNDRY_AVAILABLE_IN_1_2
char                     *foundry_acp_terminal_dup_exit_signal       (FoundryAcpTerminal *self);

G_END_DECLS
