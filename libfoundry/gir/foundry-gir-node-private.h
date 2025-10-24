/* foundry-gir-node-private.h
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

#include "foundry-gir-node.h"

G_BEGIN_DECLS

typedef enum _FoundryGirTraverseResult
{
  FOUNDRY_GIR_TRAVERSE_STOP = 0,
  FOUNDRY_GIR_TRAVERSE_CONTINUE = 1,
  FOUNDRY_GIR_TRAVERSE_MATCH = 2,
} FoundryGirTraverseResult;

typedef FoundryGirTraverseResult (*FoundryGirTraverse) (FoundryGirNode *node,
                                                        gpointer        user_data);

void            _foundry_gir_node_add_attribute (FoundryGirNode     *node,
                                                 const char         *name,
                                                 const char         *value);
void            _foundry_gir_node_add_child     (FoundryGirNode     *parent,
                                                 FoundryGirNode     *child);
void            _foundry_gir_node_append_text   (FoundryGirNode     *node,
                                                 const char         *text,
                                                 size_t              text_len);
FoundryGirNode *_foundry_gir_node_traverse      (FoundryGirNode     *node,
                                                 FoundryGirTraverse  traverse,
                                                 gpointer            user_data);

G_END_DECLS
