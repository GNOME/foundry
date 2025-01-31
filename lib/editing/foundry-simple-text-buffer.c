/* foundry-simple-text-buffer.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "line-reader-private.h"

#include "foundry-context.h"
#include "foundry-simple-text-buffer.h"
#include "foundry-text-edit.h"
#include "foundry-text-iter.h"

struct _FoundrySimpleTextBuffer
{
  GObject         parent_instance;
  FoundryContext *context;
  GString        *contents;
  GFile          *file;
};

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_FILE,
  N_PROPS
};

static void text_buffer_iface_init (FoundryTextBufferInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundrySimpleTextBuffer, foundry_simple_text_buffer, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (FOUNDRY_TYPE_TEXT_BUFFER, text_buffer_iface_init))

static GParamSpec *properties[N_PROPS];

static void
foundry_simple_text_buffer_finalize (GObject *object)
{
  FoundrySimpleTextBuffer *self = (FoundrySimpleTextBuffer *)object;

  g_clear_weak_pointer (&self->context);

  g_clear_object (&self->file);

  g_string_free (self->contents, TRUE);
  self->contents = NULL;

  G_OBJECT_CLASS (foundry_simple_text_buffer_parent_class)->finalize (object);
}

static void
foundry_simple_text_buffer_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  FoundrySimpleTextBuffer *self = FOUNDRY_SIMPLE_TEXT_BUFFER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_simple_text_buffer_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  FoundrySimpleTextBuffer *self = FOUNDRY_SIMPLE_TEXT_BUFFER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_set_weak_pointer (&self->context, g_value_get_object (value));
      break;

    case PROP_FILE:
      if (g_set_object (&self->file, g_value_get_object (value)))
        g_object_notify_by_pspec (object, pspec);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_simple_text_buffer_class_init (FoundrySimpleTextBufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_simple_text_buffer_finalize;
  object_class->get_property = foundry_simple_text_buffer_get_property;
  object_class->set_property = foundry_simple_text_buffer_set_property;

  properties[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         FOUNDRY_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_simple_text_buffer_init (FoundrySimpleTextBuffer *self)
{
  self->contents = g_string_new (NULL);
}

FoundryTextBuffer *
foundry_simple_text_buffer_new (void)
{
  return g_object_new (FOUNDRY_TYPE_SIMPLE_TEXT_BUFFER, NULL);
}

static GBytes *
foundry_simple_text_buffer_dup_contents (FoundryTextBuffer *text_buffer)
{
  FoundrySimpleTextBuffer *self = FOUNDRY_SIMPLE_TEXT_BUFFER (text_buffer);
  char *copy;

  copy = g_malloc (self->contents->len + 1);
  memcpy (copy, self->contents->str, self->contents->len);
  copy[self->contents->len] = 0;

  return g_bytes_new (copy, self->contents->len);
}

typedef struct
{
  GBytes *bytes;
  GFile *file;
} Save;

static void
save_free (Save *save)
{
  g_clear_pointer (&save->bytes, g_bytes_unref);
  g_clear_object (&save->file);
  g_free (save);
}

static DexFuture *
foundry_simple_text_buffer_save_fiber (gpointer user_data)
{
  Save *save = user_data;
  g_autoptr(GOutputStream) stream = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (save != NULL);
  g_assert (save->bytes != NULL);
  g_assert (G_IS_FILE (save->file));

  if (!(stream = dex_await_object (dex_file_replace (save->file, NULL, FALSE, 0, 0), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!dex_await (dex_output_stream_write_bytes (stream, save->bytes, 0), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
foundry_simple_text_buffer_save (FoundryTextBuffer *text_buffer,
                                 GFile             *file)
{
  FoundrySimpleTextBuffer *self = (FoundrySimpleTextBuffer *)text_buffer;
  Save *save;

  g_assert (FOUNDRY_IS_SIMPLE_TEXT_BUFFER (self));
  g_assert (G_IS_FILE (file));

  if (g_set_object (&self->file, file))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE]);

  save = g_new0 (Save, 1);
  save->bytes = foundry_text_buffer_dup_contents (text_buffer);
  save->file = g_object_ref (file);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_simple_text_buffer_save_fiber,
                              save,
                              (GDestroyNotify) save_free);
}

typedef struct
{
  FoundrySimpleTextBuffer *self;
  GFile *file;
} Load;

static void
load_free (Load *load)
{
  g_clear_object (&load->self);
  g_clear_object (&load->file);
  g_free (load);
}

static DexFuture *
foundry_simple_text_buffer_load_fiber (gpointer user_data)
{
  Load *load = user_data;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  const guint8 *data;
  gsize len;

  g_assert (load != NULL);
  g_assert (FOUNDRY_IS_SIMPLE_TEXT_BUFFER (load->self));
  g_assert (G_IS_FILE (load->file));

  if (!(bytes = dex_await_object (dex_file_load_contents_bytes (load->file), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  data = g_bytes_get_data (bytes, &len);

  if (!g_utf8_validate_len ((const char *)data, len, NULL))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "Data is not UTF-8");

  g_string_truncate (load->self->contents, 0);
  g_string_append_len (load->self->contents, (char *)data, len);

  return dex_future_new_true ();
}

static DexFuture *
foundry_simple_text_buffer_load (FoundryTextBuffer *text_buffer,
                                 GFile             *file)
{
  FoundrySimpleTextBuffer *self = (FoundrySimpleTextBuffer *)text_buffer;
  Load *load;

  g_assert (FOUNDRY_IS_SIMPLE_TEXT_BUFFER (self));
  g_assert (G_IS_FILE (file));

  if (g_set_object (&self->file, file))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE]);

  load = g_new0 (Load, 1);
  load->self = g_object_ref (self);
  load->file = g_object_ref (file);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_simple_text_buffer_load_fiber,
                              load,
                              (GDestroyNotify) load_free);
}

static DexFuture *
foundry_simple_text_buffer_settle (FoundryTextBuffer *text_buffer)
{
  return dex_future_new_true ();
}

static void
order (gsize *a,
       gsize *b)
{
  if (*a > *b)
    {
      gsize tmp = *a;
      *a = *b;
      *b = tmp;
    }
}

static void
foundry_simple_text_buffer_get_offset_at (FoundrySimpleTextBuffer *self,
                                          gsize                   *iter,
                                          guint                    line,
                                          int                      line_offset)
{
  LineReader reader;
  const char *str;
  gsize line_len;

  g_assert (FOUNDRY_IS_SIMPLE_TEXT_BUFFER (self));
  g_assert (iter != NULL);

  *iter = 0;

  line_reader_init (&reader, self->contents->str, self->contents->len);

  while ((str = line_reader_next (&reader, &line_len)))
    {
      if (line == 0)
        {
          gsize n_chars = g_utf8_strlen (str, line_len);

          if (line_offset < 0 || line_offset >= n_chars)
            *iter = str - self->contents->str + line_len;
          else
            *iter = g_utf8_offset_to_pointer (str, line_offset) - self->contents->str;

          return;
        }

      line--;
    }

  *iter = self->contents->len;
}

static gboolean
foundry_simple_text_buffer_apply_edit (FoundryTextBuffer *text_editor,
                                       FoundryTextEdit   *edit)
{
  FoundrySimpleTextBuffer *self = (FoundrySimpleTextBuffer *)text_editor;
  g_autofree char *replacement = NULL;
  guint begin_line;
  guint end_line;
  int begin_line_offset;
  int end_line_offset;
  gsize begin;
  gsize end;

  g_assert (FOUNDRY_IS_SIMPLE_TEXT_BUFFER (self));
  g_assert (FOUNDRY_IS_TEXT_EDIT (edit));

  foundry_text_edit_get_range (edit,
                               &begin_line, &begin_line_offset,
                               &end_line, &end_line_offset);

  foundry_simple_text_buffer_get_offset_at (self, &begin, begin_line, begin_line_offset);
  foundry_simple_text_buffer_get_offset_at (self, &end, end_line, end_line_offset);

  order (&begin, &end);

  g_string_erase (self->contents, begin, end - begin);

  if ((replacement = foundry_text_edit_dup_replacement (edit)))
    g_string_insert (self->contents, begin, replacement);

  return TRUE;
}

static FoundryTextIterVTable iter_vtable = {
};

static void
foundry_simple_text_buffer_get_start_iter (FoundryTextBuffer *buffer,
                                           FoundryTextIter   *iter)
{
  foundry_text_iter_init (iter, buffer, &iter_vtable);
}

static void
text_buffer_iface_init (FoundryTextBufferInterface *iface)
{
  iface->dup_contents = foundry_simple_text_buffer_dup_contents;
  iface->settle = foundry_simple_text_buffer_settle;
  iface->save = foundry_simple_text_buffer_save;
  iface->load = foundry_simple_text_buffer_load;
  iface->apply_edit = foundry_simple_text_buffer_apply_edit;
  iface->get_start_iter = foundry_simple_text_buffer_get_start_iter;
}
