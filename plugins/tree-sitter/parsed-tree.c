/* parsed-tree.c
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

#include "config.h"

#include "parsed-tree.h"

struct _ParsedTree
{
  TSParser *parser;
  TSTree   *tree;
  GBytes   *source_bytes;
};

G_DEFINE_BOXED_TYPE (ParsedTree, parsed_tree,
                     parsed_tree_ref, parsed_tree_unref)

static void
parsed_tree_finalize (ParsedTree *self)
{
  if (self->tree != NULL)
    ts_tree_delete (self->tree);

  if (self->parser != NULL)
    ts_parser_delete (self->parser);

  if (self->source_bytes != NULL)
    g_bytes_unref (self->source_bytes);
}

ParsedTree *
parsed_tree_new (TSParser *parser,
                 TSTree   *tree,
                 GBytes   *source_bytes)
{
  ParsedTree *self;

  g_return_val_if_fail (parser != NULL, NULL);
  g_return_val_if_fail (tree != NULL, NULL);
  g_return_val_if_fail (source_bytes != NULL, NULL);

  self = g_atomic_rc_box_new0 (ParsedTree);
  self->parser = parser;
  self->tree = tree;
  self->source_bytes = g_bytes_ref (source_bytes);

  return self;
}

ParsedTree *
parsed_tree_ref (ParsedTree *self)
{
  return g_atomic_rc_box_acquire (self);
}

void
parsed_tree_unref (ParsedTree *self)
{
  g_atomic_rc_box_release_full (self, (GDestroyNotify)parsed_tree_finalize);
}

TSParser *
parsed_tree_get_parser (ParsedTree *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->parser;
}

TSTree *
parsed_tree_get_tree (ParsedTree *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->tree;
}

char *
parsed_tree_get_text (ParsedTree *self,
                      guint32    offset,
                      guint32    length)
{
  const char *source_text = NULL;
  gsize source_length = 0;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->source_bytes != NULL, NULL);

  source_text = (const char *)g_bytes_get_data (self->source_bytes, &source_length);

  if (offset >= source_length)
    return NULL;

  if (offset + length > source_length)
    length = source_length - offset;

  if (length == 0)
    return NULL;

  return g_strndup (source_text + offset, length);
}
