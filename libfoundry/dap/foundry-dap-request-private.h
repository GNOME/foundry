/* foundry-dap-request-private.h
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

#include "foundry-dap-request.h"
#include "foundry-dap-protocol-message-private.h"

G_BEGIN_DECLS

struct _FoundryDapRequest
{
  FoundryDapProtocolMessage parent_instance;
};

struct _FoundryDapRequestClass
{
  FoundryDapProtocolMessageClass parent_class;

  const char *(*get_command)       (FoundryDapRequest *self);
  GType       (*get_response_type) (FoundryDapRequest *self);
};

const char *_foundry_dap_request_get_command       (FoundryDapRequest *self);
GType       _foundry_dap_request_get_response_type (FoundryDapRequest *self);

G_END_DECLS
