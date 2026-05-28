/* foundry-acp-connection-private.h
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

#include "foundry-acp-connection.h"

G_BEGIN_DECLS

FoundryAcpConnection *_foundry_acp_connection_new_for_stream (FoundryContext       *context,
                                                              GIOStream            *stream,
                                                              FoundryAcpClient     *client);
DexFuture            *_foundry_acp_connection_call           (FoundryAcpConnection *self,
                                                              const char           *method,
                                                              JsonNode             *params) G_GNUC_WARN_UNUSED_RESULT;
DexFuture            *_foundry_acp_connection_notify         (FoundryAcpConnection *self,
                                                              const char           *method,
                                                              JsonNode             *params) G_GNUC_WARN_UNUSED_RESULT;
FoundryAcpSession    *_foundry_acp_connection_lookup_session (FoundryAcpConnection *self,
                                                              const char           *session_id);
void                  _foundry_acp_connection_add_session    (FoundryAcpConnection *self,
                                                              FoundryAcpSession    *session);
void                  _foundry_acp_connection_remove_session (FoundryAcpConnection *self,
                                                              FoundryAcpSession    *session);
FoundryAcpClient     *_foundry_acp_connection_dup_client     (FoundryAcpConnection *self);
void                  _foundry_acp_connection_fail           (FoundryAcpConnection *self,
                                                              const char           *message);

G_END_DECLS
