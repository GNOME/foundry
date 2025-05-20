/*
 * foundry-source-buffer-provider.c
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include "foundry-gtk-text-buffer.h"
#include "foundry-source-buffer-provider.h"

struct _FoundrySourceBufferProvider
{
  FoundryTextBufferProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (FoundrySourceBufferProvider, foundry_source_buffer_provider, FOUNDRY_TYPE_TEXT_BUFFER_PROVIDER)

static FoundryTextBuffer *
foundry_source_buffer_provider_create_buffer (FoundryTextBufferProvider *provider)
{
  g_autoptr(FoundryContext) context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (provider));

  return FOUNDRY_TEXT_BUFFER (foundry_text_buffer_new (context));
}

static DexFuture *
foundry_source_buffer_provider_load_fiber (FoundryContext    *context,
                                           FoundryTextBuffer *buffer,
                                           GFile             *location,
                                           FoundryOperation  *operation,
                                           const char        *charset,
                                           const char        *crlf)
{
  g_autoptr(GtkSourceFileLoader) loader = NULL;
  g_autoptr(FoundryTextManager) text_manager = NULL;
  g_autoptr(GtkSourceFile) file = NULL;
  g_autoptr(GBytes) sniff = NULL;
  g_autoptr(GError) error = NULL;
  GtkTextIter begin, end;
  const char *language;
  char *text;

  g_assert (FOUNDRY_IS_CONTEXT (context));
  g_assert (FOUNDRY_IS_TEXT_BUFFER (buffer));
  g_assert (G_IS_FILE (location));
  g_assert (!operation || FOUNDRY_IS_OPERATION (operation));

  text_manager = foundry_context_dup_text_manager (context);

  file = gtk_source_file_new ();
  gtk_source_file_set_location (file, location);

  loader = gtk_source_file_loader_new (GTK_SOURCE_BUFFER (buffer), file);

  if (charset != NULL)
    {
      const GtkSourceEncoding *encoding = gtk_source_encoding_get_from_charset (charset);

      if (encoding != NULL)
        {
          GSList candidate = { .next = NULL, .data = (gpointer)encoding };
          gtk_source_file_loader_set_candidate_encodings (loader, &candidate);
        }
    }

  if (!dex_await (gtk_source_file_loader_load (loader, G_PRIORITY_DEFAULT, operation), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);

  /* Grab the first 1KB of data for sniffing content-type */
  if (gtk_text_iter_get_offset (&end) > 1024)
    gtk_text_iter_set_offset (&end, 1024);
  text = gtk_text_iter_get_slice (&begin, &end);
  sniff = g_bytes_new_take (text, strlen (text));

  /* TODO: Check for metadata like cursor, spelling language, etc */

  /* Sniff syntax language from file and buffer contents */
  if ((language = dex_await_string (foundry_text_manager_guess_language (text_manager, location, NULL, sniff), NULL)))
    {
      GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default ();
      GtkSourceLanguage *l = gtk_source_language_manager_get_language (lm, language);

      if (l != NULL)
        gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), l);
    }

  return dex_future_new_true ();
}

static DexFuture *
foundry_source_buffer_provider_load (FoundryTextBufferProvider *provider,
                                     FoundryTextBuffer         *buffer,
                                     GFile                     *file,
                                     FoundryOperation          *operation,
                                     const char                *encoding,
                                     const char                *crlf)
{
  g_autoptr(FoundryContext) context = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_SOURCE_BUFFER_PROVIDER (provider));
  dex_return_error_if_fail (FOUNDRY_IS_GTK_TEXT_BUFFER (buffer));
  dex_return_error_if_fail (G_IS_FILE (file));
  dex_return_error_if_fail (FOUNDRY_IS_OPERATION (operation));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (provider));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (foundry_source_buffer_provider_load_fiber),
                                  6,
                                  FOUNDRY_TYPE_CONTEXT, context,
                                  FOUNDRY_TYPE_TEXT_BUFFER, buffer,
                                  G_TYPE_FILE, file,
                                  FOUNDRY_TYPE_OPERATION, operation,
                                  G_TYPE_STRING, encoding,
                                  G_TYPE_STRING, crlf);
}

static DexFuture *
foundry_source_buffer_provider_save_fiber (FoundryTextBuffer *buffer,
                                             GFile             *location,
                                             FoundryOperation  *operation,
                                             const char        *charset,
                                             const char        *crlf)
{
  g_autoptr(GtkSourceFileSaver) saver = NULL;
  g_autoptr(GtkSourceFile) file = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_TEXT_BUFFER (buffer));
  g_assert (G_IS_FILE (location));
  g_assert (!operation || FOUNDRY_IS_OPERATION (operation));

  file = gtk_source_file_new ();
  gtk_source_file_set_location (file, location);

  saver = gtk_source_file_saver_new (GTK_SOURCE_BUFFER (buffer), file);
  gtk_source_file_saver_set_flags (saver,
                                   (GTK_SOURCE_FILE_SAVER_FLAGS_IGNORE_MODIFICATION_TIME |
                                    GTK_SOURCE_FILE_SAVER_FLAGS_IGNORE_INVALID_CHARS));

  if (crlf != NULL)
    {
      if (strcmp (crlf, "\n") == 0)
        gtk_source_file_saver_set_newline_type (saver, GTK_SOURCE_NEWLINE_TYPE_LF);
      else if (strcmp (crlf, "\r") == 0)
        gtk_source_file_saver_set_newline_type (saver, GTK_SOURCE_NEWLINE_TYPE_CR);
      else if (strcmp (crlf, "\r\n") == 0)
        gtk_source_file_saver_set_newline_type (saver, GTK_SOURCE_NEWLINE_TYPE_CR_LF);
    }

  if (charset != NULL)
    {
      const GtkSourceEncoding *encoding = gtk_source_encoding_get_from_charset (charset);

      if (encoding != NULL)
        gtk_source_file_saver_set_encoding (saver, encoding);
    }

  if (!dex_await (gtk_source_file_saver_save (saver, G_PRIORITY_DEFAULT, operation), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  /* TODO: Check for metadata like cursor, spelling language, etc */

  return dex_future_new_true ();
}

static DexFuture *
foundry_source_buffer_provider_save (FoundryTextBufferProvider *provider,
                                     FoundryTextBuffer         *buffer,
                                     GFile                     *file,
                                     FoundryOperation          *operation,
                                     const char                *encoding,
                                     const char                *crlf)
{
  dex_return_error_if_fail (FOUNDRY_IS_SOURCE_BUFFER_PROVIDER (provider));
  dex_return_error_if_fail (FOUNDRY_IS_GTK_TEXT_BUFFER (buffer));
  dex_return_error_if_fail (G_IS_FILE (file));
  dex_return_error_if_fail (FOUNDRY_IS_OPERATION (operation));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (foundry_source_buffer_provider_save_fiber),
                                  5,
                                  FOUNDRY_TYPE_TEXT_BUFFER, buffer,
                                  G_TYPE_FILE, file,
                                  FOUNDRY_TYPE_OPERATION, operation,
                                  G_TYPE_STRING, encoding,
                                  G_TYPE_STRING, crlf);
}

static void
foundry_source_buffer_provider_class_init (FoundrySourceBufferProviderClass *klass)
{
  FoundryTextBufferProviderClass *text_buffer_provider_class = FOUNDRY_TEXT_BUFFER_PROVIDER_CLASS (klass);

  text_buffer_provider_class->create_buffer = foundry_source_buffer_provider_create_buffer;
  text_buffer_provider_class->load = foundry_source_buffer_provider_load;
  text_buffer_provider_class->save = foundry_source_buffer_provider_save;
}

static void
foundry_source_buffer_provider_init (FoundrySourceBufferProvider *self)
{
}
