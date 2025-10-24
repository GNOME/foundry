/* foundry-forge-issue.h
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

#include <glib-object.h>

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_FORGE_ISSUE (foundry_forge_issue_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryForgeIssue, foundry_forge_issue, FOUNDRY, FORGE_ISSUE, GObject)

struct _FoundryForgeIssueClass
{
  GObjectClass parent_class;

  char      *(*dup_id)         (FoundryForgeIssue *self);
  char      *(*dup_title)      (FoundryForgeIssue *self);
  char      *(*dup_state)      (FoundryForgeIssue *self);
  char      *(*dup_online_url) (FoundryForgeIssue *self);
  GDateTime *(*dup_created_at) (FoundryForgeIssue *self);

  /*< private >*/
  gpointer _reserved[18];
};

FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_issue_dup_id         (FoundryForgeIssue *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_issue_dup_state      (FoundryForgeIssue *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_issue_dup_title      (FoundryForgeIssue *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_issue_dup_online_url (FoundryForgeIssue *self);
FOUNDRY_AVAILABLE_IN_1_1
GDateTime *foundry_forge_issue_dup_created_at (FoundryForgeIssue *self);

G_END_DECLS
