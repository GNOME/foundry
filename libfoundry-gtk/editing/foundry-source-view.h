/* foundry-source-view.h
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

#include <foundry.h>
#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define FOUNDRY_TYPE_SOURCE_VIEW (foundry_source_view_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (FoundrySourceView, foundry_source_view, FOUNDRY, SOURCE_VIEW, GtkSourceView)

FOUNDRY_AVAILABLE_IN_ALL
GtkWidget            *foundry_source_view_new             (FoundryTextDocument        *document);
FOUNDRY_AVAILABLE_IN_ALL
FoundryTextDocument  *foundry_source_view_dup_document    (FoundrySourceView          *self);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture            *foundry_source_view_rename          (FoundrySourceView          *self,
                                                           const GtkTextIter          *iter,
                                                           const char                 *new_name);
FOUNDRY_AVAILABLE_IN_ALL
PangoFontDescription *foundry_source_view_dup_font        (FoundrySourceView          *self);
FOUNDRY_AVAILABLE_IN_ALL
void                  foundry_source_view_set_font        (FoundrySourceView          *self,
                                                           const PangoFontDescription *font);
FOUNDRY_AVAILABLE_IN_ALL
double                foundry_source_view_get_line_height (FoundrySourceView          *self);
FOUNDRY_AVAILABLE_IN_ALL
void                  foundry_source_view_set_line_height (FoundrySourceView          *self,
                                                           double                      line_height);

G_END_DECLS
