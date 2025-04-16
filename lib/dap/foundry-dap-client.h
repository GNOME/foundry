/* foundry-dap-client.h
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <libdex.h>

#include "foundry-dap-protocol-message.h"
#include "foundry-dap-request.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_DAP_CLIENT (foundry_dap_client_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (FoundryDapClient, foundry_dap_client, FOUNDRY, DAP_CLIENT, GObject)

FOUNDRY_AVAILABLE_IN_ALL
FoundryDapClient *foundry_dap_client_new   (GIOStream                 *stream);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture        *foundry_dap_client_send  (FoundryDapClient          *self,
                                            FoundryDapProtocolMessage *message);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture        *foundry_dap_client_call  (FoundryDapClient          *self,
                                            FoundryDapRequest         *request);

G_END_DECLS
