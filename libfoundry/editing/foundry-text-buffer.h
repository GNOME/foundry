/* foundry-text-buffer.h
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

#pragma once

#include <libdex.h>

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_TEXT_BUFFER (foundry_text_buffer_get_type())
#define FOUNDRY_TYPE_TEXT_BUFFER_COMMIT_NOTIFY_FLAGS (foundry_text_buffer_commit_notify_flags_get_type())

/**
 * FoundryTextBufferNotifyFlags:
 * @FOUNDRY_TEXT_BUFFER_NOTIFY_BEFORE_INSERT: Be notified before text
 *   is inserted into the underlying buffer.
 * @FOUNDRY_TEXT_BUFFER_NOTIFY_AFTER_INSERT: Be notified after text
 *   has been inserted into the underlying buffer.
 * @FOUNDRY_TEXT_BUFFER_NOTIFY_BEFORE_DELETE: Be notified before text
 *   is deleted from the underlying buffer.
 * @FOUNDRY_TEXT_BUFFER_NOTIFY_AFTER_DELETE: Be notified after text
 *   has been deleted from the underlying buffer.
 *
 * Since: 1.1
 */
typedef enum _FoundryTextBufferNotifyFlags
{
  FOUNDRY_TEXT_BUFFER_NOTIFY_BEFORE_INSERT = 1 << 0,
  FOUNDRY_TEXT_BUFFER_NOTIFY_AFTER_INSERT  = 1 << 1,
  FOUNDRY_TEXT_BUFFER_NOTIFY_BEFORE_DELETE = 1 << 2,
  FOUNDRY_TEXT_BUFFER_NOTIFY_AFTER_DELETE  = 1 << 3,
} FoundryTextBufferNotifyFlags;

/**
 * FoundryTextBufferCommitNotify:
 * @buffer: the text buffer being notified
 * @flags: the type of commit notification
 * @position: the position of the text operation
 * @length: the length of the text operation in characters
 * @user_data: (closure): user data passed to the callback
 *
 * This callback maps to the same semantics as in GTK.
 *
 * Since: 1.1
 */
typedef void (*FoundryTextBufferCommitNotify) (FoundryTextBuffer            *buffer,
                                               FoundryTextBufferNotifyFlags  flags,
                                               guint                         position,
                                               guint                         length,
                                               gpointer                      user_data);

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (FoundryTextBuffer, foundry_text_buffer, FOUNDRY, TEXT_BUFFER, GObject)

struct _FoundryTextBufferInterface
{
  GTypeInterface parent_iface;

  GBytes    *(*dup_contents)         (FoundryTextBuffer             *self);
  char      *(*dup_language_id)      (FoundryTextBuffer             *self);
  DexFuture *(*settle)               (FoundryTextBuffer             *self);
  gboolean   (*apply_edit)           (FoundryTextBuffer             *self,
                                      FoundryTextEdit               *edit);
  void       (*iter_init)            (FoundryTextBuffer             *self,
                                      FoundryTextIter               *iter);
  gint64     (*get_change_count)     (FoundryTextBuffer             *self);
  guint      (*add_commit_notify)    (FoundryTextBuffer             *self,
                                      FoundryTextBufferNotifyFlags   flags,
                                      FoundryTextBufferCommitNotify  commit_notify,
                                      gpointer                       user_data,
                                      GDestroyNotify                 destroy);
  void       (*remove_commit_notify) (FoundryTextBuffer             *self,
                                      guint                          commit_notify_handler);
};

FOUNDRY_AVAILABLE_IN_1_1
GType      foundry_text_buffer_notify_flags_get_type (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_ALL
GBytes    *foundry_text_buffer_dup_contents          (FoundryTextBuffer             *self);
FOUNDRY_AVAILABLE_IN_ALL
char      *foundry_text_buffer_dup_language_id       (FoundryTextBuffer             *self);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture *foundry_text_buffer_settle                (FoundryTextBuffer             *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_ALL
gboolean   foundry_text_buffer_apply_edit            (FoundryTextBuffer             *self,
                                                      FoundryTextEdit               *edit);
FOUNDRY_AVAILABLE_IN_ALL
void       foundry_text_buffer_get_start_iter        (FoundryTextBuffer             *self,
                                                      FoundryTextIter               *iter);
FOUNDRY_AVAILABLE_IN_1_1
void       foundry_text_buffer_get_iter_at_offset    (FoundryTextBuffer             *self,
                                                      FoundryTextIter               *iter,
                                                      gsize                          offset);
FOUNDRY_AVAILABLE_IN_ALL
void       foundry_text_buffer_emit_changed          (FoundryTextBuffer             *self);
FOUNDRY_AVAILABLE_IN_ALL
gint64     foundry_text_buffer_get_change_count      (FoundryTextBuffer             *self);
FOUNDRY_AVAILABLE_IN_1_1
guint      foundry_text_buffer_add_commit_notify     (FoundryTextBuffer             *self,
                                                      FoundryTextBufferNotifyFlags   flags,
                                                      FoundryTextBufferCommitNotify  commit_notify,
                                                      gpointer                       user_data,
                                                      GDestroyNotify                 destroy);
FOUNDRY_AVAILABLE_IN_1_1
void       foundry_text_buffer_remove_commit_notify  (FoundryTextBuffer             *self,
                                                      guint                          commit_notify_handler);

G_END_DECLS
