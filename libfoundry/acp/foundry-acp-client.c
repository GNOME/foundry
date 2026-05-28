/* foundry-acp-client.c
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

#include "config.h"

#include "foundry-acp-client.h"
#include "foundry-acp-enums.h"
#include "foundry-acp-permission-request.h"
#include "foundry-acp-session.h"
#include "foundry-acp-session-update.h"

G_DEFINE_INTERFACE (FoundryAcpClient, foundry_acp_client, G_TYPE_OBJECT)

static void
foundry_acp_client_default_init (FoundryAcpClientInterface *iface)
{
}

static DexFuture *
foundry_acp_client_unsupported (const char *method)
{
  return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                FOUNDRY_ACP_ERROR_METHOD_NOT_FOUND,
                                "ACP client method `%s` is not implemented",
                                method);
}

/**
 * foundry_acp_client_session_update:
 * @self: a [iface@Foundry.AcpClient]
 * @session: a [class@Foundry.AcpSession]
 * @update: a [class@Foundry.AcpSessionUpdate]
 *
 * Delivers an ACP `session/update` notification to the client.
 *
 * Returns: (transfer full): a [class@Dex.Future] resolving when local delivery
 *   is complete
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_client_session_update (FoundryAcpClient        *self,
                                   FoundryAcpSession       *session,
                                   FoundryAcpSessionUpdate *update)
{
  FoundryAcpClientInterface *iface;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));
  dex_return_error_if_fail (update != NULL);

  iface = FOUNDRY_ACP_CLIENT_GET_IFACE (self);

  if (iface->session_update != NULL)
    return iface->session_update (self, session, update);

  return dex_future_new_true ();
}

/**
 * foundry_acp_client_request_permission:
 * @self: a [iface@Foundry.AcpClient]
 * @session: a [class@Foundry.AcpSession]
 * @request: a [class@Foundry.AcpPermissionRequest]
 *
 * Requests permission for an agent tool call.
 *
 * The returned future must resolve to a [class@Foundry.AcpPermissionResponse].
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_client_request_permission (FoundryAcpClient            *self,
                                       FoundryAcpSession           *session,
                                       FoundryAcpPermissionRequest *request)
{
  FoundryAcpClientInterface *iface;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));
  dex_return_error_if_fail (request != NULL);

  iface = FOUNDRY_ACP_CLIENT_GET_IFACE (self);

  if (iface->request_permission != NULL)
    return iface->request_permission (self, session, request);

  return foundry_acp_client_unsupported ("session/request_permission");
}

/**
 * foundry_acp_client_read_text_file:
 * @self: a [iface@Foundry.AcpClient]
 * @session: a [class@Foundry.AcpSession]
 * @path: an absolute path
 * @line: 1-based start line, or zero for the beginning
 * @limit: maximum line count, or zero for no explicit limit
 *
 * Handles an ACP `fs/read_text_file` request.
 *
 * The returned future must resolve to a UTF-8 string.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_client_read_text_file (FoundryAcpClient  *self,
                                   FoundryAcpSession *session,
                                   const char        *path,
                                   guint              line,
                                   guint              limit)
{
  FoundryAcpClientInterface *iface;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));
  dex_return_error_if_fail (path != NULL);

  iface = FOUNDRY_ACP_CLIENT_GET_IFACE (self);

  if (iface->read_text_file != NULL)
    return iface->read_text_file (self, session, path, line, limit);

  return foundry_acp_client_unsupported ("fs/read_text_file");
}

/**
 * foundry_acp_client_write_text_file:
 * @self: a [iface@Foundry.AcpClient]
 * @session: a [class@Foundry.AcpSession]
 * @path: an absolute path
 * @content: UTF-8 content
 *
 * Handles an ACP `fs/write_text_file` request.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_client_write_text_file (FoundryAcpClient  *self,
                                    FoundryAcpSession *session,
                                    const char        *path,
                                    const char        *content)
{
  FoundryAcpClientInterface *iface;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));
  dex_return_error_if_fail (path != NULL);
  dex_return_error_if_fail (content != NULL);

  iface = FOUNDRY_ACP_CLIENT_GET_IFACE (self);

  if (iface->write_text_file != NULL)
    return iface->write_text_file (self, session, path, content);

  return foundry_acp_client_unsupported ("fs/write_text_file");
}

/**
 * foundry_acp_client_create_terminal:
 * @self: a [iface@Foundry.AcpClient]
 * @session: a [class@Foundry.AcpSession]
 * @command: the command to execute
 * @argv: (array zero-terminated=1) (nullable): command arguments
 * @cwd: (nullable): working directory
 * @environ: (array zero-terminated=1) (nullable): environment
 * @output_byte_limit: output byte limit, or -1 for default
 *
 * Handles an ACP `terminal/create` request.
 *
 * The returned future must resolve to a [class@Foundry.AcpTerminal].
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_client_create_terminal (FoundryAcpClient   *self,
                                    FoundryAcpSession  *session,
                                    const char         *command,
                                    const char * const *argv,
                                    const char         *cwd,
                                    const char * const *environ,
                                    gssize              output_byte_limit)
{
  FoundryAcpClientInterface *iface;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));
  dex_return_error_if_fail (command != NULL);

  iface = FOUNDRY_ACP_CLIENT_GET_IFACE (self);

  if (iface->create_terminal != NULL)
    return iface->create_terminal (self, session, command, argv, cwd, environ, output_byte_limit);

  return foundry_acp_client_unsupported ("terminal/create");
}

/**
 * foundry_acp_client_terminal_output:
 * @self: a [iface@Foundry.AcpClient]
 * @session: a [class@Foundry.AcpSession]
 * @terminal_id: the protocol terminal identifier
 *
 * Handles an ACP `terminal/output` request.
 *
 * The returned future must resolve to a [class@Foundry.AcpTerminalOutput].
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_client_terminal_output (FoundryAcpClient  *self,
                                    FoundryAcpSession *session,
                                    const char        *terminal_id)
{
  FoundryAcpClientInterface *iface;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));
  dex_return_error_if_fail (terminal_id != NULL);

  iface = FOUNDRY_ACP_CLIENT_GET_IFACE (self);

  if (iface->terminal_output != NULL)
    return iface->terminal_output (self, session, terminal_id);

  return foundry_acp_client_unsupported ("terminal/output");
}

/**
 * foundry_acp_client_terminal_wait_for_exit:
 * @self: a [iface@Foundry.AcpClient]
 * @session: a [class@Foundry.AcpSession]
 * @terminal_id: the protocol terminal identifier
 *
 * Handles an ACP `terminal/wait_for_exit` request.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_client_terminal_wait_for_exit (FoundryAcpClient  *self,
                                           FoundryAcpSession *session,
                                           const char        *terminal_id)
{
  FoundryAcpClientInterface *iface;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));
  dex_return_error_if_fail (terminal_id != NULL);

  iface = FOUNDRY_ACP_CLIENT_GET_IFACE (self);

  if (iface->terminal_wait_for_exit != NULL)
    return iface->terminal_wait_for_exit (self, session, terminal_id);

  return foundry_acp_client_unsupported ("terminal/wait_for_exit");
}

/**
 * foundry_acp_client_terminal_kill:
 * @self: a [iface@Foundry.AcpClient]
 * @session: a [class@Foundry.AcpSession]
 * @terminal_id: the protocol terminal identifier
 *
 * Handles an ACP `terminal/kill` request.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_client_terminal_kill (FoundryAcpClient  *self,
                                  FoundryAcpSession *session,
                                  const char        *terminal_id)
{
  FoundryAcpClientInterface *iface;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));
  dex_return_error_if_fail (terminal_id != NULL);

  iface = FOUNDRY_ACP_CLIENT_GET_IFACE (self);

  if (iface->terminal_kill != NULL)
    return iface->terminal_kill (self, session, terminal_id);

  return foundry_acp_client_unsupported ("terminal/kill");
}

/**
 * foundry_acp_client_terminal_release:
 * @self: a [iface@Foundry.AcpClient]
 * @session: a [class@Foundry.AcpSession]
 * @terminal_id: the protocol terminal identifier
 *
 * Handles an ACP `terminal/release` request.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_client_terminal_release (FoundryAcpClient  *self,
                                     FoundryAcpSession *session,
                                     const char        *terminal_id)
{
  FoundryAcpClientInterface *iface;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));
  dex_return_error_if_fail (terminal_id != NULL);

  iface = FOUNDRY_ACP_CLIENT_GET_IFACE (self);

  if (iface->terminal_release != NULL)
    return iface->terminal_release (self, session, terminal_id);

  return foundry_acp_client_unsupported ("terminal/release");
}

/**
 * foundry_acp_client_refresh_changed_files:
 * @self: a [iface@Foundry.AcpClient]
 * @session: a [class@Foundry.AcpSession]
 *
 * Refreshes @session's changed-file model after a prompt or terminal has
 * completed.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_client_refresh_changed_files (FoundryAcpClient  *self,
                                          FoundryAcpSession *session)
{
  FoundryAcpClientInterface *iface;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_CLIENT (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (session));

  iface = FOUNDRY_ACP_CLIENT_GET_IFACE (self);

  if (iface->refresh_changed_files != NULL)
    return iface->refresh_changed_files (self, session);

  return dex_future_new_true ();
}
