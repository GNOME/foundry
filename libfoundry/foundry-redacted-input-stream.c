/* foundry-redacted-input-stream.c
 *
 * Copyright 2026 Christian Hergert
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

#include "foundry-redacted-input-stream-private.h"

#define REDACTION_TEXT "[REDACTED]"

typedef struct _FoundryRedactingConverter FoundryRedactingConverter;
typedef struct _FoundryRedactingConverterClass FoundryRedactingConverterClass;

struct _FoundryRedactingConverter
{
  GObject    parent_instance;
  GPtrArray *secrets;
  gsize      max_secret_len;
};

struct _FoundryRedactingConverterClass
{
  GObjectClass parent_class;
};

struct _FoundryRedactedInputStream
{
  GConverterInputStream parent_instance;
};

GType       foundry_redacting_converter_get_type   (void);
static void foundry_redacting_converter_iface_init (GConverterIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryRedactingConverter,
                               foundry_redacting_converter,
                               G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_CONVERTER, foundry_redacting_converter_iface_init))
G_DEFINE_FINAL_TYPE (FoundryRedactedInputStream, foundry_redacted_input_stream, G_TYPE_CONVERTER_INPUT_STREAM)

static int
compare_secret (gconstpointer a,
                gconstpointer b)
{
  GBytes *bytes_a = *(GBytes **)a;
  GBytes *bytes_b = *(GBytes **)b;
  gsize len_a = g_bytes_get_size (bytes_a);
  gsize len_b = g_bytes_get_size (bytes_b);

  if (len_a < len_b)
    return 1;
  else if (len_a > len_b)
    return -1;

  return g_bytes_compare (bytes_a, bytes_b);
}

static GBytes *
find_secret (FoundryRedactingConverter *self,
             const guint8              *input,
             gsize                      input_len)
{
  g_assert (self != NULL);
  g_assert (input != NULL || input_len == 0);

  for (guint i = 0; i < self->secrets->len; i++)
    {
      gconstpointer secret_data;
      GBytes *secret;
      gsize secret_len;

      secret = g_ptr_array_index (self->secrets, i);
      secret_data = g_bytes_get_data (secret, &secret_len);

      if (secret_len <= input_len && memcmp (input, secret_data, secret_len) == 0)
        return secret;
    }

  return NULL;
}

static GConverterResult
foundry_redacting_converter_convert (GConverter       *converter,
                                     const void       *inbuf,
                                     gsize             inbuf_size,
                                     void             *outbuf,
                                     gsize             outbuf_size,
                                     GConverterFlags   flags,
                                     gsize            *bytes_read,
                                     gsize            *bytes_written,
                                     GError          **error)
{
  static const char replacement[] = REDACTION_TEXT;
  FoundryRedactingConverter *self = (FoundryRedactingConverter *)converter;
  const guint8 *input = inbuf;
  guint8 *output = outbuf;
  gsize input_pos = 0;
  gsize output_pos = 0;
  gsize safe_end;
  gboolean draining;

  g_assert (self != NULL);
  g_assert (inbuf != NULL || inbuf_size == 0);
  g_assert (outbuf != NULL);
  g_assert (outbuf_size > 0);
  g_assert (bytes_read != NULL);
  g_assert (bytes_written != NULL);

  *bytes_read = 0;
  *bytes_written = 0;
  draining = (flags & (G_CONVERTER_INPUT_AT_END | G_CONVERTER_FLUSH)) != 0;

  if (draining || self->max_secret_len == 0)
    safe_end = inbuf_size;
  else if (inbuf_size >= self->max_secret_len)
    safe_end = inbuf_size - self->max_secret_len + 1;
  else
    safe_end = 0;

  while (input_pos < safe_end)
    {
      GBytes *secret;
      gsize secret_len;

      if ((secret = find_secret (self, input + input_pos, inbuf_size - input_pos)))
        {
          secret_len = g_bytes_get_size (secret);

          if (sizeof replacement - 1 > outbuf_size - output_pos)
            break;

          memcpy (output + output_pos, replacement, sizeof replacement - 1);
          input_pos += secret_len;
          output_pos += sizeof replacement - 1;
        }
      else
        {
          if (output_pos == outbuf_size)
            break;

          output[output_pos++] = input[input_pos++];
        }
    }

  *bytes_read = input_pos;
  *bytes_written = output_pos;

  if (draining && input_pos == inbuf_size)
    {
      if (flags & G_CONVERTER_INPUT_AT_END)
        return G_CONVERTER_FINISHED;

      return G_CONVERTER_FLUSHED;
    }

  if (input_pos > 0 || output_pos > 0)
    return G_CONVERTER_CONVERTED;

  if (safe_end > 0)
    g_set_error_literal (error,
                         G_IO_ERROR,
                         G_IO_ERROR_NO_SPACE,
                         "Output buffer is too small for redacted data");
  else
    g_set_error_literal (error,
                         G_IO_ERROR,
                         G_IO_ERROR_PARTIAL_INPUT,
                         "More input is required to redact the stream");

  return G_CONVERTER_ERROR;
}

static void
foundry_redacting_converter_reset (GConverter *converter)
{
  g_assert (G_IS_CONVERTER (converter));
}

static void
foundry_redacting_converter_finalize (GObject *object)
{
  FoundryRedactingConverter *self = (FoundryRedactingConverter *)object;

  g_clear_pointer (&self->secrets, g_ptr_array_unref);

  G_OBJECT_CLASS (foundry_redacting_converter_parent_class)->finalize (object);
}

static void
foundry_redacting_converter_class_init (FoundryRedactingConverterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_redacting_converter_finalize;
}

static void
foundry_redacting_converter_init (FoundryRedactingConverter *self)
{
  self->secrets = g_ptr_array_new_with_free_func ((GDestroyNotify)g_bytes_unref);
}

static void
foundry_redacting_converter_iface_init (GConverterIface *iface)
{
  iface->convert = foundry_redacting_converter_convert;
  iface->reset = foundry_redacting_converter_reset;
}

static FoundryRedactingConverter *
foundry_redacting_converter_new (const char * const *secrets)
{
  g_autoptr(GHashTable) seen = NULL;
  FoundryRedactingConverter *self;

  g_assert (secrets != NULL);

  self = g_object_new (foundry_redacting_converter_get_type (), NULL);
  seen = g_hash_table_new ((GHashFunc)g_bytes_hash, (GEqualFunc)g_bytes_equal);

  for (guint i = 0; secrets[i] != NULL; i++)
    {
      GBytes *bytes;
      gsize len = strlen (secrets[i]);

      if (len == 0)
        continue;

      bytes = g_bytes_new (secrets[i], len);
      if (g_hash_table_contains (seen, bytes))
        {
          g_bytes_unref (bytes);
          continue;
        }

      g_hash_table_add (seen, bytes);
      g_ptr_array_add (self->secrets, bytes);
      self->max_secret_len = MAX (self->max_secret_len, len);
    }

  g_ptr_array_sort (self->secrets, compare_secret);

  return self;
}

static void
foundry_redacted_input_stream_class_init (FoundryRedactedInputStreamClass *klass)
{
}

static void
foundry_redacted_input_stream_init (FoundryRedactedInputStream *self)
{
}

FoundryRedactedInputStream *
_foundry_redacted_input_stream_new (GInputStream       *base_stream,
                                    const char * const *secrets)
{
  g_autoptr(GConverter) converter = NULL;

  g_return_val_if_fail (G_IS_INPUT_STREAM (base_stream), NULL);
  g_return_val_if_fail (secrets != NULL, NULL);

  converter = G_CONVERTER (foundry_redacting_converter_new (secrets));

  return g_object_new (FOUNDRY_TYPE_REDACTED_INPUT_STREAM,
                       "base-stream", base_stream,
                       "converter", converter,
                       NULL);
}
