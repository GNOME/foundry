/* code-index.c
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

#include "config.h"

#include <stdlib.h>

#include "code-index.h"
#include "code-index-private.h"
#include "code-sparse-set.h"

#define CODE_INDEX_MAGIC     {0xC,0x0,0xD,0xE}
#define CODE_INDEX_ALIGNMENT 8

G_DEFINE_BOXED_TYPE (CodeIndex, code_index,
                     code_index_ref, code_index_unref)
G_DEFINE_BOXED_TYPE (CodeIndexBuilder, code_index_builder,
                     code_index_builder_ref, code_index_builder_unref)

static inline void
write_uint (CodeByteArray *bytes,
            guint          value)
{
  do
    {
      guint8 b = ((value > 0x7F) << 7) | (value & 0x7F);
      value >>= 7;
      code_byte_array_append (bytes, b);
    }
  while (value > 0);
}

static gboolean
_code_trigram_iter_next_char (CodeTrigramIter *iter,
                              gunichar        *ch)
{
  if (iter->pos >= iter->end)
    return FALSE;

  /* Since we're reading files they may not be in modified UTF-8 format.
   * If they're in regular UTF-8 there could be embedded Nil bytes. Handle
   * those specifically because g_utf8_*() will not.
   */

  if G_UNLIKELY (iter->pos[0] == 0)
    {
      *ch = 0;
      iter->pos++;
      return TRUE;
    }

  *ch = g_utf8_get_char_validated (iter->pos, iter->end - iter->pos);

  if (*ch == (gunichar)-1 || *ch == (gunichar)-2)
    {
      iter->pos = iter->end;
      return FALSE;
    }

  iter->pos = g_utf8_next_char (iter->pos);

  return TRUE;
}

void
code_trigram_iter_init (CodeTrigramIter *iter,
                        const char      *text,
                        goffset          len)
{
  if (len < 0)
    len = strlen (text);

  iter->pos = text;
  iter->end = text + len;

  if (_code_trigram_iter_next_char (iter, &iter->trigram.y))
    _code_trigram_iter_next_char (iter, &iter->trigram.z);
}

gboolean
code_trigram_iter_next (CodeTrigramIter *iter,
                        CodeTrigram     *trigram)
{
  if G_UNLIKELY (iter->pos >= iter->end)
    return FALSE;

  iter->trigram.x = iter->trigram.y;
  iter->trigram.y = iter->trigram.z;

  if (!_code_trigram_iter_next_char (iter, &iter->trigram.z))
    return FALSE;

  trigram->x = !g_unichar_isspace (iter->trigram.x) ? iter->trigram.x : '_';
  trigram->y = !g_unichar_isspace (iter->trigram.y) ? iter->trigram.y : '_';
  trigram->z = !g_unichar_isspace (iter->trigram.z) ? iter->trigram.z : '_';

  return TRUE;
}

guint
code_trigram_encode (const CodeTrigram *trigram)
{
  return ((trigram->x & 0xFF) << 16) |
         ((trigram->y & 0xFF) <<  8) |
         ((trigram->z & 0xFF) <<  0);
}

CodeTrigram
code_trigram_decode (guint encoded)
{
  return (CodeTrigram) {
    .x = ((encoded & 0xFF0000) >> 16),
    .y = ((encoded & 0xFF00) >>  8),
    .z = (encoded & 0xFF),
  };
}

typedef struct _CodeIndexBuilderDocument
{
  const char *path;
  char       *collate;
  guint32     id;
  guint32     position;
} CodeIndexBuilderDocument;

static void code_index_builder_document_clear (CodeIndexBuilderDocument *document);

#define EGG_ARRAY_TYPE_NAME CodeIndexBuilderDocumentArray
#define EGG_ARRAY_NAME code_index_builder_document_array
#define EGG_ARRAY_ELEMENT_TYPE CodeIndexBuilderDocument
#define EGG_ARRAY_BY_VALUE
#define EGG_ARRAY_FREE_FUNC code_index_builder_document_clear
#include "../../contrib/eggarrayimpl.c"

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (CodeIndexBuilderDocumentArray,
                                  code_index_builder_document_array_clear)

typedef struct _CodeIndexHeader
{
  guint8  magic[4];
  guint32 n_documents;
  guint32 documents;
  guint32 n_documents_bytes;
  guint32 n_trigrams;
  guint32 trigrams;
  guint32 n_trigrams_bytes;
  guint32 trigrams_data;
  guint32 trigrams_data_bytes;
} CodeIndexHeader;

typedef struct _CodeIndexTrigram
{
  guint32 trigram_id;
  guint32 position;
  guint32 end;
} CodeIndexTrigram;

struct _CodeIndexBuilder
{
  GStringChunk                  *paths;
  CodeSparseSet                  trigrams_set;
  CodeSparseSet                  uncommitted_set;
  CodeIndexBuilderDocumentArray  documents;
  CodeTrigramPostingArray        trigrams;
  const char                    *current_path;
};

static void
code_index_builder_document_clear (CodeIndexBuilderDocument *document)
{
  g_clear_pointer (&document->collate, g_free);
  document->path = 0;
  document->position = 0;
  document->id = 0;
}

static void
code_index_builder_finalize (CodeIndexBuilder *builder)
{
  code_sparse_set_clear (&builder->trigrams_set);
  code_sparse_set_clear (&builder->uncommitted_set);
  g_clear_pointer (&builder->paths, g_string_chunk_free);
  code_index_builder_document_array_clear (&builder->documents);
  code_trigram_posting_array_clear (&builder->trigrams);
}

void
code_index_builder_unref (CodeIndexBuilder *builder)
{
  g_atomic_rc_box_release_full (builder, (GDestroyNotify)code_index_builder_finalize);
}

CodeIndexBuilder *
code_index_builder_new (void)
{
  CodeIndexBuilderDocument zero = {0};
  CodeIndexBuilder *builder;

  builder = g_atomic_rc_box_new0 (CodeIndexBuilder);
  code_index_builder_document_array_init (&builder->documents);
  code_trigram_posting_array_init (&builder->trigrams);
  builder->paths = g_string_chunk_new (4096*4);
  code_sparse_set_init (&builder->trigrams_set, 1<<24);
  code_sparse_set_init (&builder->uncommitted_set, 1<<24);

  code_index_builder_document_array_append (&builder->documents, &zero);

  return builder;
}

CodeIndexBuilder *
code_index_builder_ref (CodeIndexBuilder *builder)
{
  return g_atomic_rc_box_acquire (builder);
}

guint
code_index_builder_get_n_documents (CodeIndexBuilder *builder)
{
  gsize n_documents = code_index_builder_document_array_get_size (&builder->documents);

  g_return_val_if_fail (n_documents <= G_MAXUINT, G_MAXUINT);

  return n_documents;
}

guint
code_index_builder_get_n_trigrams (CodeIndexBuilder *builder)
{
  return builder->trigrams_set.len;
}

guint
code_index_builder_get_uncommitted (CodeIndexBuilder *builder)
{
  return builder->uncommitted_set.len;
}

void
code_index_builder_add (CodeIndexBuilder  *builder,
                        const CodeTrigram *trigram)
{
  guint trigram_id = code_trigram_encode (trigram);

  code_sparse_set_add (&builder->uncommitted_set, trigram_id);
}

void
code_index_builder_begin (CodeIndexBuilder *builder,
                          const char       *path)
{
  builder->current_path = g_string_chunk_insert_const (builder->paths, path);
}

void
code_index_builder_commit (CodeIndexBuilder *builder)
{
  gsize n_documents = code_index_builder_document_array_get_size (&builder->documents);
  CodeIndexBuilderDocument document;

  g_return_if_fail (n_documents < G_MAXUINT32);

  document = (CodeIndexBuilderDocument) {
    .path = builder->current_path,
    .collate = g_utf8_collate_key_for_filename (builder->current_path, -1),
    .id = n_documents,
    .position = 0,
  };

  code_index_builder_document_array_append (&builder->documents, &document);

  for (guint i = 0; i < builder->uncommitted_set.len; i++)
    {
      CodeTrigramPosting *trigrams;
      guint trigram_id = builder->uncommitted_set.dense[i].value;
      guint trigrams_index;

      if (!code_sparse_set_get (&builder->trigrams_set, trigram_id, &trigrams_index))
        {
          CodeTrigramPosting t;
          gsize n_trigrams = code_trigram_posting_array_get_size (&builder->trigrams);

          g_assert (n_trigrams <= G_MAXUINT);

          code_byte_array_init (&t.buffer);
          t.id = trigram_id;
          t.last_document_id = 0;
          t.position = 0;

          trigrams_index = n_trigrams;
          code_sparse_set_add_with_data (&builder->trigrams_set, trigram_id, trigrams_index);
          code_trigram_posting_array_append (&builder->trigrams, &t);
        }

      trigrams = code_trigram_posting_array_get (&builder->trigrams, trigrams_index);
      write_uint (&trigrams->buffer, document.id - trigrams->last_document_id);
      trigrams->last_document_id = document.id;
    }

  builder->current_path = NULL;

  code_sparse_set_reset (&builder->uncommitted_set);
}

void
code_index_builder_rollback (CodeIndexBuilder *builder)
{
  builder->current_path = NULL;

  code_sparse_set_reset (&builder->uncommitted_set);
}

static int
sort_by_trigram (gconstpointer a,
                 gconstpointer b)
{
  const CodeTrigramPosting *atri = a;
  const CodeTrigramPosting *btri = b;

  if (atri->id < btri->id)
    return -1;
  else if (atri->id > btri->id)
    return 1;
  else
    return 0;
}

static gboolean
uint32_from_size (guint32    *out_value,
                  gsize       value,
                  const char *field,
                  GError    **error)
{
  if G_UNLIKELY (value > G_MAXUINT32)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Index is too large: %s is %" G_GSIZE_FORMAT
                   ", but the current index format stores 32-bit values",
                   field, value);
      return FALSE;
    }

  *out_value = value;

  return TRUE;
}

static gboolean
uint32_add_size (gsize       begin,
                 gsize       size,
                 guint32    *out_value,
                 const char *field,
                 GError    **error)
{
  if G_UNLIKELY (begin > G_MAXUINT32 || size > G_MAXUINT32 - begin)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Index is too large: %s exceeds 32-bit offset storage",
                   field);
      return FALSE;
    }

  *out_value = begin + size;

  return TRUE;
}

static gboolean
index_buffer_append_data (CodeByteArray *buffer,
                          const guint8  *data,
                          gsize          len,
                          const char    *field,
                          GError       **error)
{
  guint32 end;

  if (!uint32_add_size (code_byte_array_get_size (buffer), len, &end, field, error))
    return FALSE;

  code_byte_array_append_data (buffer, data, len);

  return TRUE;
}

static gboolean
realign (CodeByteArray *buffer,
         gsize         *out_position,
         const char    *field,
         GError       **error)
{
  static const guint8 zero[CODE_INDEX_ALIGNMENT] = {0};
  gsize len = code_byte_array_get_size (buffer);
  gsize rem = len % CODE_INDEX_ALIGNMENT;

  if (rem > 0)
    {
      gsize padding = CODE_INDEX_ALIGNMENT - rem;

      if (!index_buffer_append_data (buffer, zero, padding, field, error))
        return FALSE;
    }

  *out_position = code_byte_array_get_size (buffer);

  return TRUE;
}

static gboolean
validate_header_range (gsize       file_size,
                       guint32     offset,
                       gsize       size,
                       const char *field,
                       GError    **error)
{
  if G_UNLIKELY (offset > file_size || size > file_size - offset)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Index is too large: %s exceeds serialized index size",
                   field);
      return FALSE;
    }

  return TRUE;
}

static gboolean
validate_index_header_for_write (const CodeIndexHeader *header,
                                 gsize                  file_size,
                                 GError               **error)
{
  gsize documents_table_size;
  gsize trigrams_table_size;

  if G_UNLIKELY (file_size > G_MAXUINT32)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Index is too large: index size is %" G_GSIZE_FORMAT
                   ", but the current index format stores 32-bit offsets",
                   file_size);
      return FALSE;
    }

  if G_UNLIKELY (header->n_documents > G_MAXSIZE / sizeof (guint32) ||
                 header->n_trigrams > G_MAXSIZE / sizeof (CodeIndexTrigram))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "Index table size exceeds addressable memory");
      return FALSE;
    }

  documents_table_size = (gsize)header->n_documents * sizeof (guint32);
  trigrams_table_size = (gsize)header->n_trigrams * sizeof (CodeIndexTrigram);

  if G_UNLIKELY (header->documents % CODE_INDEX_ALIGNMENT != 0 ||
                 header->trigrams_data % CODE_INDEX_ALIGNMENT != 0 ||
                 header->trigrams % CODE_INDEX_ALIGNMENT != 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "Index section offsets are not correctly aligned");
      return FALSE;
    }

  if G_UNLIKELY (header->n_documents_bytes < documents_table_size)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "Index document section is smaller than the document table");
      return FALSE;
    }

  if G_UNLIKELY (header->n_trigrams_bytes != trigrams_table_size)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "Index trigram section size does not match the trigram count");
      return FALSE;
    }

  return validate_header_range (file_size,
                                header->documents,
                                documents_table_size,
                                "document table",
                                error) &&
         validate_header_range (file_size,
                                header->trigrams_data,
                                header->trigrams_data_bytes,
                                "posting data section",
                                error) &&
         validate_header_range (file_size,
                                header->trigrams,
                                trigrams_table_size,
                                "trigram table",
                                error);
}

DexFuture *
code_index_builder_write (CodeIndexBuilder *builder,
                          GOutputStream    *stream,
                          int               io_priority)
{
  g_auto(CodeByteArray) buffer;
  g_autoptr(GError) error = NULL;
  DexFuture *future;
  GBytes *bytes;
  CodeIndexHeader header = {
    .magic = CODE_INDEX_MAGIC,
  };
  gsize begin_documents_pos;
  gsize position;
  gsize n_documents;
  gsize n_trigrams;

  g_return_val_if_fail (builder != NULL, NULL);
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), NULL);

  code_byte_array_init (&buffer);

  n_documents = code_index_builder_document_array_get_size (&builder->documents);
  n_trigrams = code_trigram_posting_array_get_size (&builder->trigrams);

  if (!uint32_from_size (&header.n_documents, n_documents, "document count", &error) ||
      !uint32_from_size (&header.n_trigrams, n_trigrams, "trigram count", &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (n_trigrams > 1)
    qsort (code_trigram_posting_array_get_data (&builder->trigrams),
           n_trigrams,
           sizeof (CodeTrigramPosting),
           sort_by_trigram);

  if (!index_buffer_append_data (&buffer,
                                 (const guint8 *)&header,
                                 sizeof header,
                                 "index header",
                                 &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!realign (&buffer, &begin_documents_pos, "document path alignment", &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (gsize i = 1; i < n_documents; i++)
    {
      CodeIndexBuilderDocument *document;
      gsize path_len;

      document = code_index_builder_document_array_get (&builder->documents, i);
      path_len = strlen (document->path) + 1;

      if (!uint32_from_size (&document->position,
                             code_byte_array_get_size (&buffer),
                             "document path offset",
                             &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!index_buffer_append_data (&buffer,
                                     (const guint8 *)document->path,
                                     path_len,
                                     "document path data",
                                     &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (!realign (&buffer, &position, "document table alignment", &error) ||
      !uint32_from_size (&header.documents, position, "document table offset", &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (gsize i = 0; i < n_documents; i++)
    {
      CodeIndexBuilderDocument *document;

      document = code_index_builder_document_array_get (&builder->documents, i);

      if (!index_buffer_append_data (&buffer,
                                     (const guint8 *)&document->position,
                                     sizeof document->position,
                                     "document table",
                                     &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (!uint32_from_size (&header.n_documents_bytes,
                         code_byte_array_get_size (&buffer) - begin_documents_pos,
                         "document section size",
                         &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!realign (&buffer, &position, "posting data alignment", &error) ||
      !uint32_from_size (&header.trigrams_data, position, "posting data offset", &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (gsize i = 0; i < n_trigrams; i++)
    {
      CodeTrigramPosting *trigrams = code_trigram_posting_array_get (&builder->trigrams, i);
      gsize buffer_len = code_byte_array_get_size (&trigrams->buffer);

      g_assert (buffer_len > 0);

      if (!uint32_from_size (&trigrams->position,
                             code_byte_array_get_size (&buffer),
                             "posting list offset",
                             &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!index_buffer_append_data (&buffer,
                                     code_byte_array_get_data (&trigrams->buffer),
                                     buffer_len,
                                     "posting list data",
                                     &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (!uint32_from_size (&header.trigrams_data_bytes,
                         code_byte_array_get_size (&buffer) - header.trigrams_data,
                         "posting data size",
                         &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!realign (&buffer, &position, "trigram table alignment", &error) ||
      !uint32_from_size (&header.trigrams, position, "trigram table offset", &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (gsize i = 0; i < n_trigrams; i++)
    {
      CodeTrigramPosting *trigrams = code_trigram_posting_array_get (&builder->trigrams, i);
      guint32 end;

      if (!uint32_add_size (trigrams->position,
                            code_byte_array_get_size (&trigrams->buffer),
                            &end,
                            "posting list end offset",
                            &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!index_buffer_append_data (&buffer,
                                     (const guint8 *)&trigrams->id,
                                     sizeof trigrams->id,
                                     "trigram table id",
                                     &error) ||
          !index_buffer_append_data (&buffer,
                                     (const guint8 *)&trigrams->position,
                                     sizeof trigrams->position,
                                     "trigram table position",
                                     &error) ||
          !index_buffer_append_data (&buffer,
                                     (const guint8 *)&end,
                                     sizeof end,
                                     "trigram table end",
                                     &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (!uint32_from_size (&header.n_trigrams_bytes,
                         code_byte_array_get_size (&buffer) - header.trigrams,
                         "trigram table size",
                         &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!validate_index_header_for_write (&header, code_byte_array_get_size (&buffer), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  memcpy (code_byte_array_get_data (&buffer), &header, sizeof header);

  bytes = code_byte_array_steal_as_bytes (&buffer);
  future = dex_output_stream_write_bytes (stream, bytes, io_priority);
  g_bytes_unref (bytes);

  return future;
}

static DexFuture *
code_index_builder_write_file_cb (DexFuture *completed,
                                  gpointer   user_data)
{
  g_autoptr(GOutputStream) stream = dex_await_object (dex_ref (completed), NULL);
  CodeIndexBuilder *builder = user_data;

  return code_index_builder_write (builder, stream, 0);
}

DexFuture *
code_index_builder_write_file (CodeIndexBuilder *builder,
                               GFile            *file,
                               int               io_priority)
{
  DexFuture *future;

  g_return_val_if_fail (builder != NULL, NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  future = dex_file_replace (file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, io_priority);
  future = dex_future_then (future,
                            code_index_builder_write_file_cb,
                            code_index_builder_ref (builder),
                            (GDestroyNotify)code_index_builder_unref);

  return future;
}

DexFuture *
code_index_builder_write_filename (CodeIndexBuilder *builder,
                                   const char       *filename,
                                   int               io_priority)
{
  g_autoptr(GFile) file = NULL;

  g_return_val_if_fail (builder != NULL, NULL);
  g_return_val_if_fail (filename != NULL, NULL);

  file = g_file_new_for_path (filename);

  return code_index_builder_write_file (builder, file, io_priority);
}

struct _CodeIndex
{
  GMappedFile             *map;
  CodeIndexTrigram        *trigrams;
  guint32                 *documents;
  CodeIndexDocumentLoader  loader;
  gpointer                 loader_data;
  GDestroyNotify           loader_data_destroy;
  CodeIndexHeader          header;
};

#define SIZE_OVERFLOWS(a,b) (G_UNLIKELY ((b) > 0 && (a) > G_MAXSIZE / (b)))

static inline gboolean
has_space_for (gsize length,
               gsize offset,
               gsize n_items,
               gsize item_size)
{
  gsize avail;
  gsize needed;

  if (n_items == 0)
    return offset <= length;

  if (offset >= length)
    return FALSE;

  avail = length - offset;

  if (SIZE_OVERFLOWS (n_items, item_size))
    return FALSE;

  needed = n_items * item_size;

  return needed <= avail;
}

static inline gboolean
has_range (gsize length,
           gsize offset,
           gsize size)
{
  if (offset > length)
    return FALSE;

  return size <= length - offset;
}

static DexFuture *
code_index_default_loader (CodeIndex  *index,
                           const char *path,
                           gpointer    user_data)
{
  g_autoptr(GMappedFile) mapped = NULL;
  g_autoptr(GError) error = NULL;

  if (!(mapped = g_mapped_file_new (path, FALSE, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_boxed (G_TYPE_BYTES,
                                    g_mapped_file_get_bytes (mapped));
}


CodeIndex *
code_index_new (const char  *filename,
                GError     **error)
{
  static const guint8 magic[] = CODE_INDEX_MAGIC;
  CodeIndex *index;
  GMappedFile *mf;
  const char *data;
  gsize len;

  if (!(mf = g_mapped_file_new (filename, FALSE, error)))
    return NULL;

  if (g_mapped_file_get_length (mf) < sizeof index->header)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_DATA,
                           "Not a codeindex");
      g_mapped_file_unref (mf);
      return NULL;
    }

  data = g_mapped_file_get_contents (mf);
  len = g_mapped_file_get_length (mf);

  index = g_atomic_rc_box_new0 (CodeIndex);

  memcpy (&index->header, data, sizeof index->header);
  index->map = mf;

  index->loader = code_index_default_loader;
  index->loader_data = NULL;
  index->loader_data_destroy = NULL;

  if (memcmp (&index->header.magic, magic, sizeof magic) != 0 ||
      SIZE_OVERFLOWS (index->header.n_documents, sizeof (guint32)) ||
      SIZE_OVERFLOWS (index->header.n_trigrams, sizeof (CodeIndexTrigram)) ||
      index->header.n_documents_bytes < index->header.n_documents * sizeof (guint32) ||
      index->header.n_trigrams_bytes != index->header.n_trigrams * sizeof (CodeIndexTrigram) ||
      !has_space_for (len,
                      index->header.trigrams,
                      index->header.n_trigrams,
                      sizeof (CodeIndexTrigram)) ||
      !has_space_for (len,
                      index->header.documents,
                      index->header.n_documents,
                      sizeof (guint32)) ||
      !has_range (len, index->header.trigrams_data, index->header.trigrams_data_bytes) ||
      index->header.trigrams % CODE_INDEX_ALIGNMENT != 0 ||
      index->header.trigrams_data % CODE_INDEX_ALIGNMENT != 0 ||
      index->header.documents % CODE_INDEX_ALIGNMENT != 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_DATA,
                           "Not a codeindex");
      code_index_unref (index);
      return NULL;
    }

  index->trigrams = (CodeIndexTrigram *)(gpointer)&data[index->header.trigrams];
  index->documents = (guint32 *)(gpointer)&data[index->header.documents];

  return index;
}

CodeIndex *
code_index_ref (CodeIndex *index)
{
  return g_atomic_rc_box_acquire (index);
}

static void
code_index_finalize (CodeIndex *index)
{
  g_clear_pointer (&index->map, g_mapped_file_unref);
}

void
code_index_unref (CodeIndex *index)
{
  return g_atomic_rc_box_release_full (index, (GDestroyNotify)code_index_finalize);
}

const char *
code_index_get_document_path (CodeIndex *index,
                              guint      document_id)
{
  const char *data;
  const char *path;
  const char *end;
  const char *iter;
  guint position;
  gsize len;

  g_return_val_if_fail (document_id > 0, NULL);
  g_return_val_if_fail (document_id < index->header.n_documents, NULL);

  position = index->documents[document_id];
  data = g_mapped_file_get_contents (index->map);
  len = g_mapped_file_get_length (index->map);

  g_return_val_if_fail (position < len, NULL);

  end = &data[len];
  path = &data[position];

  for (iter = path; iter < end; iter++)
    {
      if (*iter == 0)
        return path;
    }

  g_return_val_if_reached (NULL);
}

static int
find_trigram_by_id_cmp (gconstpointer keyptr,
                        gconstpointer trigramptr)
{
  const guint *key = keyptr;
  const CodeIndexTrigram *trigram = trigramptr;

  if (*key < trigram->trigram_id)
    return -1;
  else if (*key > trigram->trigram_id)
    return 1;
  else
    return 0;
}

static const CodeIndexTrigram *
code_index_find_trigram_by_id (CodeIndex *index,
                               guint      trigram_id)
{
  return bsearch (&trigram_id,
                  index->trigrams,
                  index->header.n_trigrams,
                  sizeof *index->trigrams,
                  find_trigram_by_id_cmp);
}

static inline gboolean
code_index_iter_init_raw (CodeIndexIter          *iter,
                          CodeIndex              *index,
                          const guint8           *data,
                          gsize                   len,
                          const CodeIndexTrigram *trigrams)
{
  if (trigrams->position >= len || trigrams->end > len || trigrams->end < trigrams->position)
    return FALSE;

  iter->index = index;
  iter->pos = &data[trigrams->position];
  iter->end = &data[trigrams->end];
  iter->last = 0;

  return TRUE;
}

gboolean
code_index_iter_init (CodeIndexIter     *iter,
                      CodeIndex         *index,
                      const CodeTrigram *trigram)
{
  const CodeIndexTrigram *trigrams;
  const guint8 *data;
  guint trigram_id;
  gsize len;

  trigram_id = code_trigram_encode (trigram);

  if (!(trigrams = code_index_find_trigram_by_id (index, trigram_id)))
    return FALSE;

  data = (const guint8 *)g_mapped_file_get_contents (index->map);
  len = g_mapped_file_get_length (index->map);
  if (trigrams->position >= len || trigrams->end > len || trigrams->end < trigrams->position)
    return FALSE;

  return code_index_iter_init_raw (iter, index, data, len, trigrams);
}

static gboolean
code_index_iter_next_id (CodeIndexIter *iter,
                         guint         *out_document_id)
{
  guint u = 0, o = 0;
  guint8 b;

  do
    {
      if (iter->pos >= iter->end || o > 28)
        return FALSE;

      b = *iter->pos;;
      u |= ((guint32)(b & 0x7F) << o);
      o += 7;

      iter->pos++;
    }
  while ((b & 0x80) != 0);

  if (u == 0 || u > G_MAXUINT - iter->last)
    return FALSE;

  iter->last += u;

  if (iter->last >= iter->index->header.n_documents)
    return FALSE;

  *out_document_id = iter->last;

  return TRUE;
}

gboolean
code_index_iter_next (CodeIndexIter *iter,
                      CodeDocument  *out_document)
{
  guint document_id;

  if (code_index_iter_next_id (iter, &document_id))
    {
      out_document->id = document_id;
      out_document->path = code_index_get_document_path (iter->index, document_id);

      return out_document->path != NULL;
    }

  return FALSE;
}

gboolean
code_index_iter_seek_to (CodeIndexIter *iter,
                         guint          document_id)
{
  guint ignored;

  do
    {
      if (iter->last >= document_id)
        break;
    }
  while (code_index_iter_next_id (iter, &ignored));

  return iter->last == document_id;
}

void
_code_index_document_iter_init (CodeIndexDocumentIter *iter,
                                CodeIndex             *index)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (index != NULL);

  iter->index = index;
  iter->document_id = 1;
  iter->end_document_id = index->header.n_documents;
}

gboolean
_code_index_document_iter_next (CodeIndexDocumentIter *iter,
                                CodeDocument          *out_document)
{
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (out_document != NULL, FALSE);

  while (iter->document_id < iter->end_document_id)
    {
      guint document_id = iter->document_id++;
      const char *path = code_index_get_document_path (iter->index, document_id);

      if (path == NULL)
        return FALSE;

      out_document->id = document_id;
      out_document->path = path;

      return TRUE;
    }

  return FALSE;
}

void
_code_index_get_all_document_ids (CodeIndex       *index,
                                  CodePostingList *out_document_ids)
{
  g_return_if_fail (index != NULL);
  g_return_if_fail (out_document_ids != NULL);

  if (index->header.n_documents <= 1)
    return;

  code_posting_list_reserve (out_document_ids, index->header.n_documents - 1);

  for (guint i = 1; i < index->header.n_documents; i++)
    code_posting_list_append (out_document_ids, i);
}

gboolean
_code_index_get_posting_list (CodeIndex       *index,
                              guint            trigram_id,
                              CodePostingList *out_document_ids)
{
  const CodeIndexTrigram *trigrams;
  const guint8 *data;
  CodeIndexIter iter;
  guint document_id;
  gsize old_size;
  gsize len;

  g_return_val_if_fail (index != NULL, FALSE);
  g_return_val_if_fail (out_document_ids != NULL, FALSE);

  old_size = code_posting_list_get_size (out_document_ids);

  if (!(trigrams = code_index_find_trigram_by_id (index, trigram_id)))
    return FALSE;

  data = (const guint8 *)g_mapped_file_get_contents (index->map);
  len = g_mapped_file_get_length (index->map);

  if (!code_index_iter_init_raw (&iter, index, data, len, trigrams))
    return FALSE;

  while (code_index_iter_next_id (&iter, &document_id))
    code_posting_list_append (out_document_ids, document_id);

  if G_UNLIKELY (iter.pos != iter.end)
    {
      code_posting_list_set_size (out_document_ids, old_size);
      return FALSE;
    }

  return code_posting_list_get_size (out_document_ids) > old_size;
}

gboolean
code_index_builder_merge (CodeIndexBuilder *builder,
                          CodeIndex        *index)
{
  guint document_id_offset;
  gsize n_documents;
  const guint8 *data;
  gsize len;

  n_documents = code_index_builder_document_array_get_size (&builder->documents);

  g_assert (n_documents >= 1);

  /* get our starting document id */
  if (n_documents - 1 > G_MAXUINT32)
    return FALSE;

  document_id_offset = n_documents - 1;

  /* Make sure enough space for document ids */
  if (G_MAXUINT32 - document_id_offset < index->header.n_documents)
    return FALSE;

  /* Add all of the documents to the index */
  for (guint i = 1; i < index->header.n_documents; i++)
    {
      const char *path = code_index_get_document_path (index, i);
      CodeIndexBuilderDocument document = {
        .path = g_string_chunk_insert_const (builder->paths, path),
        .collate = g_utf8_collate_key_for_filename (path, -1),
        .id = code_index_builder_document_array_get_size (&builder->documents),
        .position = 0,
      };

      code_index_builder_document_array_append (&builder->documents, &document);
    }

  data = (const guint8 *)g_mapped_file_get_contents (index->map);
  len = g_mapped_file_get_length (index->map);

  /* Get the array of trigrams so we can iterate them */
  for (guint i = 0; i < index->header.n_trigrams; i++)
    {
      const CodeIndexTrigram *trigrams = &index->trigrams[i];
      CodeTrigramPosting *builder_trigrams;
      CodeIndexIter iter;
      guint trigrams_index;
      guint id;

      if (!code_index_iter_init_raw (&iter, index, data, len, trigrams))
        continue;

      if (!code_sparse_set_get (&builder->trigrams_set, trigrams->trigram_id, &trigrams_index))
        {
          CodeTrigramPosting t;
          gsize n_trigrams = code_trigram_posting_array_get_size (&builder->trigrams);

          g_assert (n_trigrams <= G_MAXUINT);

          code_byte_array_init (&t.buffer);
          t.id = trigrams->trigram_id;
          t.last_document_id = 0;
          t.position = 0;

          trigrams_index = n_trigrams;
          code_sparse_set_add_with_data (&builder->trigrams_set,
                                         trigrams->trigram_id,
                                         trigrams_index);
          code_trigram_posting_array_append (&builder->trigrams, &t);
        }

      builder_trigrams = code_trigram_posting_array_get (&builder->trigrams, trigrams_index);

      while (code_index_iter_next_id (&iter, &id))
        {
          id += document_id_offset;
          write_uint (&builder_trigrams->buffer, id - builder_trigrams->last_document_id);
          builder_trigrams->last_document_id = id;
        }

    }

  return TRUE;
}

void
code_index_stat (CodeIndex     *index,
                 CodeIndexStat *stat)
{
  stat->n_documents = index->header.n_documents;
  stat->n_documents_bytes = index->header.n_documents_bytes;
  stat->n_trigrams = index->header.n_trigrams;
  stat->n_trigrams_bytes = index->header.n_trigrams_bytes;
  stat->trigrams_data_bytes = index->header.trigrams_data_bytes;
}

/**
 * code_index_set_document_loader:
 * @self: a #CodeIndex
 * @loader: (scope async) (nullable): a #CodeIndexDocumentLoader or %NULL for the default
 * @loader_data: closure data for @loader
 * @loader_data_destroy: destroy callback for @loader_data
 *
 * Sets the document loader for @index.
 *
 * This allows the query system to load the contents of the document using
 * an abstracted loader which might fetch the contents from another location
 * that specified within the index.
 *
 * This is useful when using relative paths to shrink the index size.
 *
 * It can also be useful when loading contents not in a file-system such as
 * indexed commits from Git.
 */
void
code_index_set_document_loader (CodeIndex               *index,
                                CodeIndexDocumentLoader  loader,
                                gpointer                 loader_data,
                                GDestroyNotify           loader_data_destroy)
{
  g_return_if_fail (index != NULL);

  if (index->loader_data_destroy)
    index->loader_data_destroy (index->loader_data);

  if (loader == NULL)
    {
      loader = code_index_default_loader;
      loader_data = NULL;
      loader_data_destroy = NULL;
    }

  index->loader = loader;
  index->loader_data = loader_data;
  index->loader_data_destroy = loader_data_destroy;
}

/**
 * code_index_load_document_path:
 * @index: a #CodeIndex
 * @path: a path for a document
 *
 * Loads the document expected at @path
 *
 * Returns: (transfer full): a #DexFuture that resolves to a #GBytes
 *   or rejects with an error.
 */
DexFuture *
code_index_load_document_path (CodeIndex  *index,
                               const char *path)
{
  g_return_val_if_fail (index != NULL, NULL);

  return index->loader (index, path, index->loader_data);
}

/**
 * code_index_load_document:
 * @index: a #CodeIndex
 * @document_id: the document identifier
 *
 * Finds the path of @document_id and loads it using
 * code_index_load_document_path().
 *
 * Returns: (transfer full): a #DexFuture that resolves to a #GBytes
 *   or rejects with an error.
 */
DexFuture *
code_index_load_document (CodeIndex *index,
                          guint      document_id)
{
  const char *path;

  g_return_val_if_fail (index != NULL, NULL);

  if (!(path = code_index_get_document_path (index, document_id)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "Failed to locate document %u.",
                                  document_id);

  return code_index_load_document_path (index, path);
}
