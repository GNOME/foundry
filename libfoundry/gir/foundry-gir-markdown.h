/* foundry-gir-markdown.h
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

#include "foundry-gir.h"
#include "foundry-gir-node.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_GIR_MARKDOWN (foundry_gir_markdown_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryGirMarkdown, foundry_gir_markdown, FOUNDRY, GIR_MARKDOWN, GObject)

FOUNDRY_AVAILABLE_IN_1_1
FoundryGirMarkdown *foundry_gir_markdown_new      (FoundryGir          *gir);
FOUNDRY_AVAILABLE_IN_1_1
FoundryGir         *foundry_gir_markdown_get_gir  (FoundryGirMarkdown  *self);
FOUNDRY_AVAILABLE_IN_1_1
FoundryGirNode     *foundry_gir_markdown_get_node (FoundryGirMarkdown  *self);
FOUNDRY_AVAILABLE_IN_1_1
void                foundry_gir_markdown_set_node (FoundryGirMarkdown  *self,
                                                   FoundryGirNode      *node);
FOUNDRY_AVAILABLE_IN_1_1
char               *foundry_gir_markdown_generate (FoundryGirMarkdown  *self,
                                                   GError             **error);

G_END_DECLS
