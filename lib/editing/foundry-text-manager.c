/* foundry-text-manager.c
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

#include <libpeas.h>

#include "foundry-extension.h"
#include "foundry-inhibitor.h"
#include "foundry-language-guesser.h"
#include "foundry-text-buffer-provider.h"
#include "foundry-text-manager.h"
#include "foundry-service-private.h"

struct _FoundryTextManager
{
  FoundryService             parent_instance;
  PeasExtensionSet          *language_guessers;
  FoundryTextBufferProvider *text_buffer_provider;
};

struct _FoundryTextManagerClass
{
  FoundryServiceClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryTextManager, foundry_text_manager, FOUNDRY_TYPE_SERVICE)

static DexFuture *
foundry_text_manager_start (FoundryService *service)
{
  FoundryTextManager *self = (FoundryTextManager *)service;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryExtension) text_buffer_provider = NULL;

  g_assert (FOUNDRY_IS_TEXT_MANAGER (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  self->language_guessers = peas_extension_set_new (peas_engine_get_default (),
                                                    FOUNDRY_TYPE_LANGUAGE_GUESSER,
                                                    "context", context,
                                                    NULL);

  text_buffer_provider = foundry_extension_new (context,
                                                peas_engine_get_default (),
                                                FOUNDRY_TYPE_TEXT_BUFFER_PROVIDER,
                                                "Text-Buffer-Provider", NULL);

  /* You can only setup the buffer provider once at startup since that is what
   * will get used for all buffers that get displayed in UI/etc. They need to
   * be paired with their display counterpart (so GtkTextBuffer/GtkTextView).
   */
  if (!g_set_object (&self->text_buffer_provider,
                     foundry_extension_get_extension (text_buffer_provider)))
    g_debug ("No text buffer provider plugin registered!");

  return dex_future_new_true ();
}

static DexFuture *
foundry_text_manager_stop (FoundryService *service)
{
  FoundryTextManager *self = (FoundryTextManager *)service;

  g_assert (FOUNDRY_IS_TEXT_MANAGER (self));

  g_clear_object (&self->language_guessers);

  return dex_future_new_true ();
}

static void
foundry_text_manager_class_init (FoundryTextManagerClass *klass)
{
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  service_class->start = foundry_text_manager_start;
  service_class->stop = foundry_text_manager_stop;
}

static void
foundry_text_manager_init (FoundryTextManager *self)
{
}

/**
 * foundry_text_manager_load:
 * @self: a #FoundryTextManager
 * @file: a #GFile
 *
 * Loads the file as a text buffer.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Foundry.TextBuffer].
 */
DexFuture *
foundry_text_manager_load (FoundryTextManager *self,
                           GFile              *file)
{
  dex_return_error_if_fail (FOUNDRY_IS_TEXT_MANAGER (self));
  dex_return_error_if_fail (G_IS_FILE (file));

  return NULL;
}

typedef struct _GuessLanguage
{
  FoundryInhibitor *inhibitor;
  GPtrArray        *guessers;
  GFile            *file;
  char             *content_type;
  GBytes           *contents;
} GuessLanguage;

static void
guess_language_free (GuessLanguage *state)
{
  g_clear_object (&state->file);
  g_clear_object (&state->inhibitor);
  g_clear_pointer (&state->content_type, g_free);
  g_clear_pointer (&state->contents, g_bytes_unref);
  g_clear_pointer (&state->guessers, g_ptr_array_unref);
  g_free (state);
}

static DexFuture *
foundry_text_manager_guess_language_fiber (gpointer data)
{
  GuessLanguage *state = data;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_INHIBITOR (state->inhibitor));
  g_assert (state->file || state->content_type || state->contents);
  g_assert (state->guessers != NULL);

  if (state->file != NULL && state->content_type == NULL)
    {
      g_autoptr(GFileInfo) info = dex_await_object (dex_file_query_info (state->file,
                                                                         G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                                                         G_FILE_QUERY_INFO_NONE,
                                                                         G_PRIORITY_DEFAULT),
                                                    NULL);

      if (info != NULL)
        state->content_type = g_strdup (g_file_info_get_content_type (info));
    }

  for (guint i = 0; i < state->guessers->len; i++)
    {
      FoundryLanguageGuesser *guesser = g_ptr_array_index (state->guessers, i);
      g_autofree char *language = NULL;

      if ((language = dex_await_string (foundry_language_guesser_guess (guesser, state->file, state->content_type, state->contents), NULL)))
        return dex_future_new_take_string (g_steal_pointer (&language));
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_FOUND,
                                "Failed to locate suitable language");
}

/**
 * foundry_text_manager_guess_language:
 * @self: a [class@Foundry.TextManager]
 * @file: (nullable): a [iface@Gio.File] or %NULL
 * @content_type: (nullable): the content-type as a string or %NULL
 * @contents: (nullable): a [struct@GLib.Bytes] of file contents or %NULL
 *
 * Attempts to guess the language of @file, @content_type, or @contents.
 *
 * One of @file, content_type, or @contents must be set.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a string
 *   containing the language identifier, or rejects with error.
 */
DexFuture *
foundry_text_manager_guess_language (FoundryTextManager *self,
                                     GFile              *file,
                                     const char         *content_type,
                                     GBytes             *contents)
{
  g_autoptr(FoundryInhibitor) inhibitor = NULL;
  g_autoptr(GError) error = NULL;
  GuessLanguage *state;

  dex_return_error_if_fail (FOUNDRY_IS_TEXT_MANAGER (self));
  dex_return_error_if_fail (!file || G_IS_FILE (file));
  dex_return_error_if_fail (file || content_type || contents);

  if (!(inhibitor = foundry_contextual_inhibit (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  state = g_new0 (GuessLanguage, 1);
  g_set_object (&state->file, file);
  g_set_str (&state->content_type, content_type);
  state->contents = contents ? g_bytes_ref (contents) : NULL;
  state->guessers = g_ptr_array_new_with_free_func (g_object_unref);
  state->inhibitor = g_steal_pointer (&inhibitor);

  if (self->language_guessers != NULL)
    {
      GListModel *model = G_LIST_MODEL (self->language_guessers);
      guint n_items = g_list_model_get_n_items (model);

      for (guint i = 0; i < n_items; i++)
        g_ptr_array_add (state->guessers, g_list_model_get_item (model, i));
    }

  return dex_scheduler_spawn (NULL, 0,
                              foundry_text_manager_guess_language_fiber,
                              state,
                              (GDestroyNotify) guess_language_free);
}
