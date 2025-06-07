/*
 * foundry-mcp-input-stream-private.h
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <libdex.h>

#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_MCP_INPUT_STREAM (foundry_mcp_input_stream_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (FoundryMcpInputStream, foundry_mcp_input_stream, FOUNDRY, MCP_INPUT_STREAM, GDataInputStream)

FOUNDRY_AVAILABLE_IN_ALL
FoundryMcpInputStream *foundry_mcp_input_stream_new  (GInputStream          *base_stream);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture             *foundry_mcp_input_stream_read (FoundryMcpInputStream *self);

G_END_DECLS
