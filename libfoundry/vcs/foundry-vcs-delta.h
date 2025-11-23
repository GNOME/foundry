/* foundry-vcs-delta.h
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

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_VCS_DELTA (foundry_vcs_delta_get_type())
#define FOUNDRY_TYPE_VCS_DELTA_STATUS (foundry_vcs_delta_status_get_type())

typedef enum _FoundryVcsDeltaStatus
{
  FOUNDRY_VCS_DELTA_STATUS_UNMODIFIED = 0,
  FOUNDRY_VCS_DELTA_STATUS_ADDED,
  FOUNDRY_VCS_DELTA_STATUS_DELETED,
  FOUNDRY_VCS_DELTA_STATUS_MODIFIED,
  FOUNDRY_VCS_DELTA_STATUS_RENAMED,
  FOUNDRY_VCS_DELTA_STATUS_COPIED,
  FOUNDRY_VCS_DELTA_STATUS_IGNORED,
  FOUNDRY_VCS_DELTA_STATUS_UNTRACKED,
  FOUNDRY_VCS_DELTA_STATUS_TYPECHANGE,
  FOUNDRY_VCS_DELTA_STATUS_UNREADABLE,
  FOUNDRY_VCS_DELTA_STATUS_CONFLICTED,
} FoundryVcsDeltaStatus;

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (FoundryVcsDelta, foundry_vcs_delta, FOUNDRY, VCS_DELTA, GObject)

struct _FoundryVcsDeltaClass
{
  GObjectClass parent_class;

  char                  *(*dup_old_path) (FoundryVcsDelta *self);
  char                  *(*dup_new_path) (FoundryVcsDelta *self);
  char                  *(*dup_old_id)   (FoundryVcsDelta *self);
  char                  *(*dup_new_id)   (FoundryVcsDelta *self);
  FoundryVcsDeltaStatus  (*get_status)   (FoundryVcsDelta *self);
  guint                  (*get_old_mode) (FoundryVcsDelta *self);
  guint                  (*get_new_mode) (FoundryVcsDelta *self);
  DexFuture             *(*list_hunks)   (FoundryVcsDelta *self);

  /*< private >*/
  gpointer _reserved[7];
};

FOUNDRY_AVAILABLE_IN_1_1
GType                  foundry_vcs_delta_status_get_type (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_ALL
char                  *foundry_vcs_delta_dup_old_path    (FoundryVcsDelta *self);
FOUNDRY_AVAILABLE_IN_ALL
char                  *foundry_vcs_delta_dup_new_path    (FoundryVcsDelta *self);
FOUNDRY_AVAILABLE_IN_1_1
char                  *foundry_vcs_delta_dup_old_id      (FoundryVcsDelta *self);
FOUNDRY_AVAILABLE_IN_1_1
char                  *foundry_vcs_delta_dup_new_id      (FoundryVcsDelta *self);
FOUNDRY_AVAILABLE_IN_1_1
FoundryVcsDeltaStatus  foundry_vcs_delta_get_status      (FoundryVcsDelta *self);
FOUNDRY_AVAILABLE_IN_1_1
guint                  foundry_vcs_delta_get_old_mode    (FoundryVcsDelta *self);
FOUNDRY_AVAILABLE_IN_1_1
guint                  foundry_vcs_delta_get_new_mode    (FoundryVcsDelta *self);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture             *foundry_vcs_delta_list_hunks      (FoundryVcsDelta *self);

G_END_DECLS
