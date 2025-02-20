/* foundry-text-buffer.c
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

#include "foundry-context.h"
#include "foundry-operation.h"
#include "foundry-text-buffer.h"
#include "foundry-text-edit.h"

G_DEFINE_INTERFACE (FoundryTextBuffer, foundry_text_buffer, G_TYPE_OBJECT)

static void
foundry_text_buffer_default_init (FoundryTextBufferInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("context", NULL, NULL,
                                                            FOUNDRY_TYPE_CONTEXT,
                                                            (G_PARAM_READWRITE |
                                                             G_PARAM_CONSTRUCT_ONLY |
                                                             G_PARAM_STATIC_STRINGS)));
}

/**
 * foundry_text_buffer_dup_contents:
 * @self: a #FoundryTextBuffer
 *
 * Gets the contents of the buffer as a [struct@GLib.Bytes].
 *
 * Returns: (transfer full) (nullable): a #GBytes or %NULL
 */
GBytes *
foundry_text_buffer_dup_contents (FoundryTextBuffer *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEXT_BUFFER (self), NULL);

  return FOUNDRY_TEXT_BUFFER_GET_IFACE (self)->dup_contents (self);
}

/**
 * foundry_text_buffer_settle:
 * @self: a #FoundryTextBuffer
 *
 * Gets a #DexFuture that will resolve after short delay when changes
 * have completed.
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_text_buffer_settle (FoundryTextBuffer *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_TEXT_BUFFER (self));

  return FOUNDRY_TEXT_BUFFER_GET_IFACE (self)->settle (self);
}

/**
 * foundry_text_buffer_load:
 * @self: a #FoundryTextBuffer
 * @file: a [iface@Gio.File]
 * @operation: (nullable): a [class@Foundry.Operation]
 *
 * Loads @file into the buffer.
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_text_buffer_load (FoundryTextBuffer *self,
                          GFile             *file,
                          FoundryOperation  *operation)
{
  dex_return_error_if_fail (FOUNDRY_IS_TEXT_BUFFER (self));
  dex_return_error_if_fail (G_IS_FILE (file));
  dex_return_error_if_fail (!operation || FOUNDRY_IS_OPERATION (operation));

  return FOUNDRY_TEXT_BUFFER_GET_IFACE (self)->load (self, file, operation);
}

/**
 * foundry_text_buffer_save:
 * @self: a #FoundryTextBuffer
 * @file: a [iface@Gio.File]
 * @operation: (nullable): a [class@Foundry.Operation]
 *
 * Saves @file into the buffer.
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_text_buffer_save (FoundryTextBuffer *self,
                          GFile             *file,
                          FoundryOperation  *operation)
{
  dex_return_error_if_fail (FOUNDRY_IS_TEXT_BUFFER (self));
  dex_return_error_if_fail (G_IS_FILE (file));
  dex_return_error_if_fail (!operation || FOUNDRY_IS_OPERATION (operation));

  return FOUNDRY_TEXT_BUFFER_GET_IFACE (self)->save (self, file, operation);
}

gboolean
foundry_text_buffer_apply_edit (FoundryTextBuffer  *self,
                                FoundryTextEdit    *edit)
{
  g_return_val_if_fail (FOUNDRY_IS_TEXT_BUFFER (self), FALSE);
  g_return_val_if_fail (FOUNDRY_IS_TEXT_EDIT (edit), FALSE);

  return FOUNDRY_TEXT_BUFFER_GET_IFACE (self)->apply_edit (self, edit);
}

void
foundry_text_buffer_get_start_iter (FoundryTextBuffer *self,
                                    FoundryTextIter   *iter)
{
  g_return_if_fail (FOUNDRY_IS_TEXT_BUFFER (self));
  g_return_if_fail (iter != NULL);

  FOUNDRY_TEXT_BUFFER_GET_IFACE (self)->iter_init (self, iter);
}
