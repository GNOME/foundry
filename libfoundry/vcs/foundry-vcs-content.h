/* foundry-vcs-content.h
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#define FOUNDRY_TYPE_VCS_CONTENT (foundry_vcs_content_get_type())

FOUNDRY_AVAILABLE_IN_1_3
G_DECLARE_FINAL_TYPE (FoundryVcsContent, foundry_vcs_content, FOUNDRY, VCS_CONTENT, GObject)

FOUNDRY_AVAILABLE_IN_1_3
GBytes   *foundry_vcs_content_dup_bytes   (FoundryVcsContent *self);
FOUNDRY_AVAILABLE_IN_1_3
char     *foundry_vcs_content_dup_id      (FoundryVcsContent *self);
FOUNDRY_AVAILABLE_IN_1_3
gboolean  foundry_vcs_content_get_present (FoundryVcsContent *self);
FOUNDRY_AVAILABLE_IN_1_3
guint64   foundry_vcs_content_get_size    (FoundryVcsContent *self);

G_END_DECLS
