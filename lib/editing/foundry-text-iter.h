/* foundry-text-iter.h
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

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

typedef struct _FoundryTextIterVTable
{
  gsize    (*get_offset)      (FoundryTextIter *iter);
  gsize    (*get_line)        (FoundryTextIter *iter);
  gsize    (*get_line_offset) (FoundryTextIter *iter);
  gboolean (*forward_char)    (FoundryTextIter *iter);
  gboolean (*backward_char)   (FoundryTextIter *iter);
} FoundryTextIterVTable;

struct _FoundryTextIter
{
  const FoundryTextIterVTable *vtable;

  /*< private >*/
  gpointer _reserved[15];
};

void     foundry_text_iter_init            (FoundryTextIter             *iter,
                                            const FoundryTextIterVTable *vtable);
FOUNDRY_AVAILABLE_IN_ALL
gsize    foundry_text_iter_get_offset      (FoundryTextIter             *iter);
FOUNDRY_AVAILABLE_IN_ALL
gsize    foundry_text_iter_get_line        (FoundryTextIter             *iter);
FOUNDRY_AVAILABLE_IN_ALL
gsize    foundry_text_iter_get_line_offset (FoundryTextIter             *iter);
FOUNDRY_AVAILABLE_IN_ALL
gboolean foundry_text_iter_forward_char    (FoundryTextIter             *iter);
FOUNDRY_AVAILABLE_IN_ALL
gboolean foundry_text_iter_backward_char   (FoundryTextIter             *iter);

G_END_DECLS
