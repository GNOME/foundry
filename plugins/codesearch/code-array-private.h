/* code-array-private.h
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

#include <string.h>

#include "code-index.h"

G_BEGIN_DECLS

typedef guint8 CodeByte;

typedef struct _CodePostingQuery CodePostingQuery;

#define EGG_ARRAY_TYPE_NAME CodeStringArray
#define EGG_ARRAY_NAME code_string_array
#define EGG_ARRAY_ELEMENT_TYPE char *
#define EGG_ARRAY_FREE_FUNC g_free
#include "../../contrib/eggarrayimpl.c"

#define EGG_ARRAY_TYPE_NAME CodeByteArray
#define EGG_ARRAY_NAME code_byte_array
#define EGG_ARRAY_ELEMENT_TYPE CodeByte
#include "../../contrib/eggarrayimpl.c"

typedef struct _CodeTrigramPosting
{
  CodeByteArray buffer;
  guint32       id;
  guint32       position;
  guint         last_document_id;
} CodeTrigramPosting;

static inline void
code_trigram_posting_clear (CodeTrigramPosting *posting)
{
  code_byte_array_clear (&posting->buffer);
  posting->id = 0;
  posting->position = 0;
  posting->last_document_id = 0;
}

#define EGG_ARRAY_TYPE_NAME CodeDocumentArray
#define EGG_ARRAY_NAME code_document_array
#define EGG_ARRAY_ELEMENT_TYPE CodeDocument
#define EGG_ARRAY_BY_VALUE
#include "../../contrib/eggarrayimpl.c"

#define EGG_ARRAY_TYPE_NAME CodeTrigramArray
#define EGG_ARRAY_NAME code_trigram_array
#define EGG_ARRAY_ELEMENT_TYPE CodeTrigram
#define EGG_ARRAY_BY_VALUE
#include "../../contrib/eggarrayimpl.c"

#define EGG_ARRAY_TYPE_NAME CodeTrigramIdArray
#define EGG_ARRAY_NAME code_trigram_id_array
#define EGG_ARRAY_ELEMENT_TYPE guint
#include "../../contrib/eggarrayimpl.c"

#define EGG_ARRAY_TYPE_NAME CodeTrigramPostingArray
#define EGG_ARRAY_NAME code_trigram_posting_array
#define EGG_ARRAY_ELEMENT_TYPE CodeTrigramPosting
#define EGG_ARRAY_BY_VALUE
#define EGG_ARRAY_FREE_FUNC code_trigram_posting_clear
#include "../../contrib/eggarrayimpl.c"

#define EGG_ARRAY_TYPE_NAME CodePostingList
#define EGG_ARRAY_NAME code_posting_list
#define EGG_ARRAY_ELEMENT_TYPE guint
#include "../../contrib/eggarrayimpl.c"

#define EGG_ARRAY_TYPE_NAME CodePostingQueryArray
#define EGG_ARRAY_NAME code_posting_query_array
#define EGG_ARRAY_ELEMENT_TYPE CodePostingQuery *
#include "../../contrib/eggarrayimpl.c"

static inline void
code_byte_array_append_data (CodeByteArray *self,
                             const guint8  *data,
                             gsize          len)
{
  gsize size;

  g_return_if_fail (self != NULL);
  g_return_if_fail (data != NULL || len == 0);

  if (len == 0)
    return;

  size = code_byte_array_get_size (self);

  if G_UNLIKELY (len > G_MAXSIZE - size)
    g_error ("requesting array size of %zu, but maximum size is %zu",
             len, G_MAXSIZE - size);

  code_byte_array_reserve (self, size + len);
  memcpy (self->end, data, len);
  self->end += len;
}

static inline void
code_byte_array_append_zeroes (CodeByteArray *self,
                               gsize          len)
{
  gsize size;

  g_return_if_fail (self != NULL);

  if (len == 0)
    return;

  size = code_byte_array_get_size (self);

  if G_UNLIKELY (len > G_MAXSIZE - size)
    g_error ("requesting array size of %zu, but maximum size is %zu",
             len, G_MAXSIZE - size);

  code_byte_array_reserve (self, size + len);
  memset (self->end, 0, len);
  self->end += len;
}

static inline GBytes *
code_byte_array_steal_as_bytes (CodeByteArray *self)
{
  guint8 *data;
  gsize size;

  g_return_val_if_fail (self != NULL, NULL);

  size = code_byte_array_get_size (self);
  data = code_byte_array_steal (self);

  return g_bytes_new_take (data, size);
}

static inline void
code_trigram_array_append_data (CodeTrigramArray *self,
                                const CodeTrigram *data,
                                gsize             len)
{
  gsize size;

  g_return_if_fail (self != NULL);
  g_return_if_fail (data != NULL || len == 0);

  if (len == 0)
    return;

  size = code_trigram_array_get_size (self);

  if G_UNLIKELY (len > G_MAXSIZE / sizeof (CodeTrigram) - size)
    g_error ("requesting array size of %zu, but maximum size is %zu",
             len, G_MAXSIZE / sizeof (CodeTrigram) - size);

  code_trigram_array_reserve (self, size + len);
  memcpy (self->end, data, len * sizeof (CodeTrigram));
  self->end += len;
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (CodeByteArray, code_byte_array_clear)
G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (CodeDocumentArray, code_document_array_clear)
G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (CodeTrigramArray, code_trigram_array_clear)
G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (CodeTrigramIdArray, code_trigram_id_array_clear)
G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (CodeTrigramPostingArray, code_trigram_posting_array_clear)
G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (CodePostingList, code_posting_list_clear)
G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (CodePostingQueryArray, code_posting_query_array_clear)
G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (CodeStringArray, code_string_array_clear)

G_END_DECLS
