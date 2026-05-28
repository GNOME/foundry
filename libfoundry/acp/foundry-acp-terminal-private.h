/* foundry-acp-terminal-private.h
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

#include "foundry-acp-terminal.h"
#include "foundry-acp-terminal-output.h"

G_BEGIN_DECLS

void _foundry_acp_terminal_set_turn_id           (FoundryAcpTerminal       *self,
                                                  const char               *turn_id);
void _foundry_acp_terminal_set_command           (FoundryAcpTerminal       *self,
                                                  const char               *command);
void _foundry_acp_terminal_set_argv              (FoundryAcpTerminal       *self,
                                                  const char * const       *argv);
void _foundry_acp_terminal_set_cwd               (FoundryAcpTerminal       *self,
                                                  const char               *cwd);
void _foundry_acp_terminal_set_state             (FoundryAcpTerminal       *self,
                                                  FoundryAcpTerminalState   state);
void _foundry_acp_terminal_set_started_at        (FoundryAcpTerminal       *self,
                                                  gint64                    started_at);
void _foundry_acp_terminal_set_exited_at         (FoundryAcpTerminal       *self,
                                                  gint64                    exited_at);
void _foundry_acp_terminal_set_output_byte_limit (FoundryAcpTerminal       *self,
                                                  gssize                    output_byte_limit);
void _foundry_acp_terminal_set_truncated         (FoundryAcpTerminal       *self,
                                                  gboolean                  truncated);
void _foundry_acp_terminal_apply_output          (FoundryAcpTerminal       *self,
                                                  FoundryAcpTerminalOutput *output);
void _foundry_acp_terminal_clear_exit_status     (FoundryAcpTerminal       *self);

G_END_DECLS
