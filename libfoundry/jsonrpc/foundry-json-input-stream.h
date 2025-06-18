/* foundry-json-input-stream.h
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

#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_JSON_INPUT_STREAM (foundry_json_input_stream_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (FoundryJsonInputStream, foundry_json_input_stream, FOUNDRY, JSON_INPUT_STREAM, GDataInputStream)

FOUNDRY_AVAILABLE_IN_ALL
FoundryJsonInputStream *foundry_json_input_stream_new       (GInputStream           *base_stream,
                                                             gboolean                close_base_stream);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture              *foundry_json_input_stream_read_upto (FoundryJsonInputStream *self,
                                                             const char             *stop_chars,
                                                             gssize                  stop_chars_len);

G_END_DECLS
