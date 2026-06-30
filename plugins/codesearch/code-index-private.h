/* code-index-private.h
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#include "code-array-private.h"

G_BEGIN_DECLS

typedef struct _CodeIndexDocumentIter
{
  CodeIndex *index;
  guint      document_id;
  guint      end_document_id;
} CodeIndexDocumentIter;

void     _code_index_document_iter_init   (CodeIndexDocumentIter *iter,
                                           CodeIndex             *index);
gboolean _code_index_document_iter_next   (CodeIndexDocumentIter *iter,
                                           CodeDocument          *out_document);
void     _code_index_get_all_document_ids (CodeIndex             *index,
                                           CodePostingList       *out_document_ids);
gboolean _code_index_get_posting_list     (CodeIndex             *index,
                                           guint                  trigram_id,
                                           CodePostingList       *out_document_ids);

G_END_DECLS
