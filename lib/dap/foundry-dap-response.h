/* foundry-dap-response.h
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

#include <json-glib/json-glib.h>

#include "foundry-dap-protocol-message.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_DAP_RESPONSE (foundry_dap_response_get_type())

FOUNDRY_AVAILABLE_IN_ALL
FOUNDRY_DECLARE_INTERNAL_TYPE (FoundryDapResponse, foundry_dap_response, FOUNDRY, DAP_RESPONSE, FoundryDapProtocolMessage)

FOUNDRY_AVAILABLE_IN_ALL
gint64    foundry_dap_response_get_request_seq (FoundryDapResponse *self);
FOUNDRY_AVAILABLE_IN_ALL
JsonNode *foundry_dap_response_get_body        (FoundryDapResponse *self);

G_END_DECLS
