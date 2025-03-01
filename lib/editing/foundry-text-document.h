/* foundry-text-document.h
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

#include "foundry-contextual.h"
#include "foundry-text-buffer.h"
#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_TEXT_DOCUMENT (foundry_text_document_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (FoundryTextDocument, foundry_text_document, FOUNDRY, TEXT_DOCUMENT, FoundryContextual)

FOUNDRY_AVAILABLE_IN_ALL
GFile             *foundry_text_document_dup_file          (FoundryTextDocument *self);
FOUNDRY_AVAILABLE_IN_ALL
char              *foundry_text_document_dup_title         (FoundryTextDocument *self);
FOUNDRY_AVAILABLE_IN_ALL
FoundryTextBuffer *foundry_text_document_dup_buffer        (FoundryTextDocument *self);
FOUNDRY_AVAILABLE_IN_ALL
GIcon             *foundry_text_document_dup_icon          (FoundryTextDocument *self);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture         *foundry_text_document_when_changed      (FoundryTextDocument *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_ALL
DexFuture         *foundry_text_document_list_code_actions (FoundryTextDocument *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_ALL
DexFuture         *foundry_text_document_list_diagnostics  (FoundryTextDocument *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_ALL
DexFuture         *foundry_text_document_list_symbols      (FoundryTextDocument *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_ALL
gboolean           foundry_text_document_apply_edit        (FoundryTextDocument *self,
                                                            FoundryTextEdit     *edit);

G_END_DECLS
