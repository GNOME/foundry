/* foundry-file-dialog-private.h
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

#include <libdex.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct
{
  DexPromise *promise;
  GListModel *files;
  const char *encoding;
} OpenMultipleTextFiles;

static inline void
open_multiple_text_files_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  OpenMultipleTextFiles *state = user_data;
  g_autoptr(GError) error = NULL;

  state->files = gtk_file_dialog_open_multiple_text_files_finish (
      GTK_FILE_DIALOG (object), result, &state->encoding, &error);

  if (error != NULL)
    dex_promise_reject (state->promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_boolean (state->promise, TRUE);
}

static inline GListModel *
foundry_file_dialog_open_multiple_text_files (GtkFileDialog  *dialog,
                                              GtkWindow      *parent,
                                              const char    **encoding,
                                              GError        **error)
{
  g_autoptr(DexPromise) promise = NULL;
  OpenMultipleTextFiles state;

  g_assert (GTK_IS_FILE_DIALOG (dialog));
  g_assert (GTK_IS_WINDOW (parent));
  g_assert (encoding != NULL);

  *encoding = NULL;

  promise = dex_promise_new_cancellable ();

  state.promise = promise;
  state.files = NULL;
  state.encoding = NULL;

  gtk_file_dialog_open_multiple_text_files (dialog,
                                            parent,
                                            dex_promise_get_cancellable (promise),
                                            open_multiple_text_files_cb,
                                            &state);

  if (!dex_await (dex_ref (DEX_FUTURE (promise)), error))
    return NULL;

  *encoding = state.encoding;

  return g_steal_pointer (&state.files);
}

static inline void
foundry_file_dialog_select_folder_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  g_autoptr(DexPromise) promise = user_data;
  GError *error = NULL;
  GFile *file = NULL;

  g_assert (GTK_IS_FILE_DIALOG (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!(file = gtk_file_dialog_select_folder_finish (GTK_FILE_DIALOG (object), result, &error)))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_object (promise, g_steal_pointer (&file));
}

static inline DexFuture *
foundry_file_dialog_select_folder (GtkFileDialog *dialog,
                                   GtkWindow     *parent)
{
  g_autoptr(DexPromise) promise = NULL;

  dex_return_error_if_fail (GTK_IS_FILE_DIALOG (dialog));
  dex_return_error_if_fail (GTK_IS_WINDOW (parent));

  promise = dex_promise_new_cancellable ();

  gtk_file_dialog_select_folder (dialog,
                                 parent,
                                 dex_promise_get_cancellable (promise),
                                 foundry_file_dialog_select_folder_cb,
                                 dex_ref (promise));

  return DEX_FUTURE (g_steal_pointer (&promise));
}

typedef struct
{
  DexPromise *promise;
  const char *encoding;
  const char *line_ending;
  GFile *file;
} SaveTextFile;

static inline void
save_text_file_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  SaveTextFile *state = user_data;
  g_autoptr(GError) error = NULL;

  state->file = gtk_file_dialog_save_text_file_finish (GTK_FILE_DIALOG (object), result, &state->encoding, &state->line_ending, &error);

  if (error != NULL)
    dex_promise_reject (state->promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_boolean (state->promise, TRUE);
}

static inline GFile *
foundry_file_dialog_save_text_file (GtkFileDialog  *dialog,
                                    GtkWindow      *parent,
                                    const char    **encoding,
                                    const char    **line_ending,
                                    GError        **error)
{
  SaveTextFile state;

  g_assert (GTK_IS_FILE_DIALOG (dialog));
  g_assert (GTK_IS_WINDOW (parent));
  g_assert (encoding != NULL);

  *encoding = NULL;
  *line_ending = NULL;

  state.promise = dex_promise_new_cancellable ();
  state.file = NULL;
  state.encoding = *encoding;
  state.line_ending = *line_ending;

  gtk_file_dialog_save_text_file (dialog,
                                  parent,
                                  dex_promise_get_cancellable (state.promise),
                                  save_text_file_cb,
                                  &state);

  if (!dex_await (dex_ref (DEX_FUTURE (state.promise)), error))
    return NULL;

  *encoding = state.encoding;
  *line_ending = state.line_ending;

  dex_clear (&state.promise);

  return g_steal_pointer (&state.file);
}

static inline void
foundry_file_dialog_open_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr(DexPromise) promise = user_data;
  GError *error = NULL;
  GFile *file = NULL;

  g_assert (GTK_IS_FILE_DIALOG (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!(file = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (object), result, &error)))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_object (promise, g_steal_pointer (&file));
}

static inline DexFuture *
foundry_file_dialog_open (GtkFileDialog *dialog,
                          GtkWindow     *parent)
{
  g_autoptr(DexPromise) promise = NULL;

  dex_return_error_if_fail (GTK_IS_FILE_DIALOG (dialog));
  dex_return_error_if_fail (GTK_IS_WINDOW (parent));

  promise = dex_promise_new_cancellable ();

  gtk_file_dialog_open (dialog,
                        parent,
                        dex_promise_get_cancellable (promise),
                        foundry_file_dialog_open_cb,
                        dex_ref (promise));

  return DEX_FUTURE (g_steal_pointer (&promise));
}

G_END_DECLS
