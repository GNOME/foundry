/* foundry-simple-text-buffer-provider.c
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

#include "foundry-simple-text-buffer.h"
#include "foundry-simple-text-buffer-provider.h"

struct _FoundrySimpleTextBufferProvider
{
  FoundryTextBufferProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (FoundrySimpleTextBufferProvider, foundry_simple_text_buffer_provider, FOUNDRY_TYPE_TEXT_BUFFER_PROVIDER)

static FoundryTextBuffer *
foundry_simple_text_buffer_provider_create_buffer (FoundryTextBufferProvider *provider)
{
  g_autoptr(FoundryContext) context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (provider));

  return g_object_new (FOUNDRY_TYPE_SIMPLE_TEXT_BUFFER,
                       "context", context,
                       NULL);
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
foundry_simple_text_buffer_provider_save_fiber (gpointer user_data)
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
foundry_simple_text_buffer_provider_save (FoundryTextBufferProvider *provider,
                                          FoundryTextBuffer         *buffer,
                                          GFile                     *file,
                                          FoundryOperation          *operation,
                                          const char                *encoding,
                                          const char                *crlf)
{
  Save *save;

  g_assert (FOUNDRY_IS_SIMPLE_TEXT_BUFFER_PROVIDER (provider));
  g_assert (G_IS_FILE (file));

  save = g_new0 (Save, 1);
  save->bytes = foundry_text_buffer_dup_contents (buffer);
  save->file = g_object_ref (file);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_simple_text_buffer_provider_save_fiber,
                              save,
                              (GDestroyNotify) save_free);
}

typedef struct
{
  FoundrySimpleTextBuffer *buffer;
  GFile *file;
} Load;

static void
load_free (Load *load)
{
  g_clear_object (&load->buffer);
  g_clear_object (&load->file);
  g_free (load);
}

static DexFuture *
foundry_simple_text_buffer_provider_load_fiber (gpointer user_data)
{
  Load *load = user_data;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  const guint8 *data;
  gsize len;

  g_assert (load != NULL);
  g_assert (FOUNDRY_IS_SIMPLE_TEXT_BUFFER (load->buffer));
  g_assert (G_IS_FILE (load->file));

  if (!(bytes = dex_await_boxed (dex_file_load_contents_bytes (load->file), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  data = g_bytes_get_data (bytes, &len);

  if (!g_utf8_validate_len ((const char *)data, len, NULL))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "Data is not UTF-8");

  foundry_simple_text_buffer_set_text (load->buffer, (const char *)data, len);

  return dex_future_new_true ();
}

static DexFuture *
foundry_simple_text_buffer_provider_load (FoundryTextBufferProvider *provider,
                                          FoundryTextBuffer         *buffer,
                                          GFile                     *file,
                                          FoundryOperation          *operation,
                                          const char                *encoding,
                                          const char                *crlf)
{
  Load *load;

  dex_return_error_if_fail (FOUNDRY_IS_SIMPLE_TEXT_BUFFER_PROVIDER (provider));
  dex_return_error_if_fail (FOUNDRY_IS_SIMPLE_TEXT_BUFFER (buffer));
  dex_return_error_if_fail (G_IS_FILE (file));

  load = g_new0 (Load, 1);
  load->buffer = g_object_ref (FOUNDRY_SIMPLE_TEXT_BUFFER (buffer));
  load->file = g_object_ref (file);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_simple_text_buffer_provider_load_fiber,
                              load,
                              (GDestroyNotify) load_free);
}


static void
foundry_simple_text_buffer_provider_class_init (FoundrySimpleTextBufferProviderClass *klass)
{
  FoundryTextBufferProviderClass *text_buffer_provider_class = FOUNDRY_TEXT_BUFFER_PROVIDER_CLASS (klass);

  text_buffer_provider_class->create_buffer = foundry_simple_text_buffer_provider_create_buffer;
  text_buffer_provider_class->load = foundry_simple_text_buffer_provider_load;
  text_buffer_provider_class->save = foundry_simple_text_buffer_provider_save;
}

static void
foundry_simple_text_buffer_provider_init (FoundrySimpleTextBufferProvider *self)
{
}

FoundryTextBufferProvider *
foundry_simple_text_buffer_provider_new (FoundryContext *context)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);

  return g_object_new (FOUNDRY_TYPE_SIMPLE_TEXT_BUFFER_PROVIDER,
                       "context", context,
                       NULL);
}
