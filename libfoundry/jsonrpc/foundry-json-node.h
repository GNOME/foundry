/* foundry-json-node.h
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

#include "foundry-util.h"

G_BEGIN_DECLS

#ifndef __GI_SCANNER__

FOUNDRY_ALIGNED_BEGIN(8)
typedef struct
{
  char bytes[8];
} FoundryJsonNodeMagic
FOUNDRY_ALIGNED_END(8);

typedef struct
{
  FoundryJsonNodeMagic magic;
  const char *val;
} FoundryJsonNodePutString;

typedef struct
{
  FoundryJsonNodeMagic magic;
  double val;
} FoundryJsonNodePutDouble;

typedef struct
{
  FoundryJsonNodeMagic magic;
  gboolean val;
} FoundryJsonNodePutBoolean;

#define _FOUNDRY_JSON_NODE_MAGIC(s) ("@!^%" s)
#define _FOUNDRY_JSON_NODE_MAGIC_C(a,b,c,d) {'@','!','^','%',a,b,c,d}

#define _FOUNDRY_JSON_NODE_PUT_STRING_MAGIC    _FOUNDRY_JSON_NODE_MAGIC("PUTS")
#define _FOUNDRY_JSON_NODE_PUT_STRING_MAGIC_C  _FOUNDRY_JSON_NODE_MAGIC_C('P','U','T','S')
#define FOUNDRY_JSON_NODE_PUT_STRING(_val) \
  (&((FoundryJsonNodePutString) { .magic = {_FOUNDRY_JSON_NODE_PUT_STRING_MAGIC_C}, .val = _val }))

#define _FOUNDRY_JSON_NODE_PUT_DOUBLE_MAGIC    _FOUNDRY_JSON_NODE_MAGIC("PUTD")
#define _FOUNDRY_JSON_NODE_PUT_DOUBLE_MAGIC_C  _FOUNDRY_JSON_NODE_MAGIC_C('P','U','T','D')
#define FOUNDRY_JSON_NODE_PUT_DOUBLE(_val) \
  (&((FoundryJsonNodePutDouble) { .magic = {_FOUNDRY_JSON_NODE_PUT_DOUBLE_MAGIC_C}, .val = _val }))

#define _FOUNDRY_JSON_NODE_PUT_BOOLEAN_MAGIC   _FOUNDRY_JSON_NODE_MAGIC("PUTB")
#define _FOUNDRY_JSON_NODE_PUT_BOOLEAN_MAGIC_C _FOUNDRY_JSON_NODE_MAGIC_C('P','U','T','B')
#define FOUNDRY_JSON_NODE_PUT_BOOLEAN(_val) \
  (&((FoundryJsonNodePutBoolean) { .magic = {_FOUNDRY_JSON_NODE_PUT_BOOLEAN_MAGIC_C}, .val = _val }))

#define FOUNDRY_JSON_OBJECT_NEW(first,...)  \
  foundry_json_object_new(first, __VA_ARGS__, NULL)

JsonNode *foundry_json_object_new (const char *first_field,
                                   ...) G_GNUC_NULL_TERMINATED G_GNUC_WARN_UNUSED_RESULT;

#endif

G_END_DECLS
