/* parsed-tree.h
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

#include <glib-object.h>
#include <tree_sitter/api.h>

G_BEGIN_DECLS

#define PARSED_TREE_TYPE (parsed_tree_get_type())

typedef struct _ParsedTree ParsedTree;

GType        parsed_tree_get_type   (void) G_GNUC_CONST;
ParsedTree  *parsed_tree_new        (TSParser *parser,
                                     TSTree   *tree,
                                     GBytes   *source_bytes);
ParsedTree  *parsed_tree_ref        (ParsedTree *self);
void         parsed_tree_unref      (ParsedTree *self);
TSParser    *parsed_tree_get_parser (ParsedTree *self);
TSTree      *parsed_tree_get_tree   (ParsedTree *self);
char        *parsed_tree_get_text   (ParsedTree *self,
                                     guint32    offset,
                                     guint32    length);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ParsedTree, parsed_tree_unref)

G_END_DECLS
