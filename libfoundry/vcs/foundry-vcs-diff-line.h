/* foundry-vcs-diff-line.h
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <glib-object.h>

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_VCS_DIFF_LINE (foundry_vcs_diff_line_get_type())
#define FOUNDRY_TYPE_VCS_DIFF_LINE_ORIGIN (foundry_vcs_diff_line_origin_get_type())

typedef enum _FoundryVcsDiffLineOrigin
{
  FOUNDRY_VCS_DIFF_LINE_ORIGIN_CONTEXT       = ' ',
  FOUNDRY_VCS_DIFF_LINE_ORIGIN_ADDED         = '+',
  FOUNDRY_VCS_DIFF_LINE_ORIGIN_DELETED       = '-',

  FOUNDRY_VCS_DIFF_LINE_ORIGIN_CONTEXT_EOFNL = '=',   /* context line missing newline at EOF */
  FOUNDRY_VCS_DIFF_LINE_ORIGIN_ADD_EOFNL     = '>',   /* added line missing newline at EOF */
  FOUNDRY_VCS_DIFF_LINE_ORIGIN_DEL_EOFNL     = '<',   /* removed line missing newline at EOF */
} FoundryVcsDiffLineOrigin;

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryVcsDiffLine, foundry_vcs_diff_line, FOUNDRY, VCS_DIFF_LINE, GObject)

struct _FoundryVcsDiffLineClass
{
  GObjectClass parent_class;

  FoundryVcsDiffLineOrigin  (*get_origin)      (FoundryVcsDiffLine *self);
  guint                     (*get_old_line)    (FoundryVcsDiffLine *self);
  guint                     (*get_new_line)    (FoundryVcsDiffLine *self);
  char                     *(*dup_text)        (FoundryVcsDiffLine *self);
  gboolean                  (*get_has_newline) (FoundryVcsDiffLine *self);
  gsize                     (*get_length)      (FoundryVcsDiffLine *self);

  /*< private >*/
  gpointer _reserved[17];
};

FOUNDRY_AVAILABLE_IN_1_1
GType                     foundry_vcs_diff_line_origin_get_type (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_1_1
FoundryVcsDiffLineOrigin  foundry_vcs_diff_line_get_origin      (FoundryVcsDiffLine *self);
FOUNDRY_AVAILABLE_IN_1_1
guint                     foundry_vcs_diff_line_get_old_line    (FoundryVcsDiffLine *self);
FOUNDRY_AVAILABLE_IN_1_1
guint                     foundry_vcs_diff_line_get_new_line    (FoundryVcsDiffLine *self);
FOUNDRY_AVAILABLE_IN_1_1
char                     *foundry_vcs_diff_line_dup_text        (FoundryVcsDiffLine *self);
FOUNDRY_AVAILABLE_IN_1_1
gboolean                  foundry_vcs_diff_line_get_has_newline (FoundryVcsDiffLine *self);
FOUNDRY_AVAILABLE_IN_1_1
gsize                     foundry_vcs_diff_line_get_length      (FoundryVcsDiffLine *self);

G_END_DECLS
