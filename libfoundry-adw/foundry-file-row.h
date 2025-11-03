/* foundry-file-row.h
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

#include <adwaita.h>
#include <foundry.h>

G_BEGIN_DECLS

#define FOUNDRY_TYPE_FILE_ROW (foundry_file_row_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryFileRow, foundry_file_row, FOUNDRY, FILE_ROW, AdwEntryRow)

struct _FoundryFileRowClass
{
  AdwEntryRowClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

FOUNDRY_AVAILABLE_IN_1_1
GtkWidget *foundry_file_row_new           (void);
FOUNDRY_AVAILABLE_IN_1_1
GFile     *foundry_file_row_dup_file      (FoundryFileRow *self);
FOUNDRY_AVAILABLE_IN_1_1
void       foundry_file_row_set_file      (FoundryFileRow *self,
                                           GFile          *file);
FOUNDRY_AVAILABLE_IN_1_1
GFileType  foundry_file_row_get_file_type (FoundryFileRow *self);
FOUNDRY_AVAILABLE_IN_1_1
void       foundry_file_row_set_file_type (FoundryFileRow *self,
                                           GFileType       file_type);

G_END_DECLS
