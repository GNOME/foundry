/* foundry-acp-client.h
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

#define FOUNDRY_TYPE_ACP_CLIENT (foundry_acp_client_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_INTERFACE (FoundryAcpClient, foundry_acp_client, FOUNDRY, ACP_CLIENT, GObject)

struct _FoundryAcpClientInterface
{
  GTypeInterface parent_iface;

  DexFuture *(*session_update)         (FoundryAcpClient            *self,
                                        FoundryAcpSession           *session,
                                        FoundryAcpSessionUpdate     *update);
  DexFuture *(*request_permission)     (FoundryAcpClient            *self,
                                        FoundryAcpSession           *session,
                                        FoundryAcpPermissionRequest *request);
  DexFuture *(*read_text_file)         (FoundryAcpClient            *self,
                                        FoundryAcpSession           *session,
                                        const char                  *path,
                                        guint                        line,
                                        guint                        limit);
  DexFuture *(*write_text_file)        (FoundryAcpClient            *self,
                                        FoundryAcpSession           *session,
                                        const char                  *path,
                                        const char                  *content);
  DexFuture *(*create_terminal)        (FoundryAcpClient            *self,
                                        FoundryAcpSession           *session,
                                        const char                  *command,
                                        const char * const          *argv,
                                        const char                  *cwd,
                                        const char * const          *environ,
                                        gssize                       output_byte_limit);
  DexFuture *(*terminal_output)        (FoundryAcpClient            *self,
                                        FoundryAcpSession           *session,
                                        const char                  *terminal_id);
  DexFuture *(*terminal_wait_for_exit) (FoundryAcpClient            *self,
                                        FoundryAcpSession           *session,
                                        const char                  *terminal_id);
  DexFuture *(*terminal_kill)          (FoundryAcpClient            *self,
                                        FoundryAcpSession           *session,
                                        const char                  *terminal_id);
  DexFuture *(*terminal_release)       (FoundryAcpClient            *self,
                                        FoundryAcpSession           *session,
                                        const char                  *terminal_id);
  DexFuture *(*refresh_changed_files)  (FoundryAcpClient            *self,
                                        FoundryAcpSession           *session);
};

FOUNDRY_AVAILABLE_IN_1_2
DexFuture *foundry_acp_client_session_update         (FoundryAcpClient            *self,
                                                      FoundryAcpSession           *session,
                                                      FoundryAcpSessionUpdate     *update) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture *foundry_acp_client_request_permission     (FoundryAcpClient            *self,
                                                      FoundryAcpSession           *session,
                                                      FoundryAcpPermissionRequest *request) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture *foundry_acp_client_read_text_file         (FoundryAcpClient            *self,
                                                      FoundryAcpSession           *session,
                                                      const char                  *path,
                                                      guint                        line,
                                                      guint                        limit) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture *foundry_acp_client_write_text_file        (FoundryAcpClient            *self,
                                                      FoundryAcpSession           *session,
                                                      const char                  *path,
                                                      const char                  *content) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture *foundry_acp_client_create_terminal        (FoundryAcpClient            *self,
                                                      FoundryAcpSession           *session,
                                                      const char                  *command,
                                                      const char * const          *argv,
                                                      const char                  *cwd,
                                                      const char * const          *environ,
                                                      gssize                       output_byte_limit) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture *foundry_acp_client_terminal_output        (FoundryAcpClient            *self,
                                                      FoundryAcpSession           *session,
                                                      const char                  *terminal_id) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture *foundry_acp_client_terminal_wait_for_exit (FoundryAcpClient            *self,
                                                      FoundryAcpSession           *session,
                                                      const char                  *terminal_id) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture *foundry_acp_client_terminal_kill          (FoundryAcpClient            *self,
                                                      FoundryAcpSession           *session,
                                                      const char                  *terminal_id) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture *foundry_acp_client_terminal_release       (FoundryAcpClient            *self,
                                                      FoundryAcpSession           *session,
                                                      const char                  *terminal_id) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture *foundry_acp_client_refresh_changed_files  (FoundryAcpClient            *self,
                                                      FoundryAcpSession           *session) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
