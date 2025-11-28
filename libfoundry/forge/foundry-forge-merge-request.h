/* foundry-forge-merge-request.h
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
#include "foundry-forge-user.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_FORGE_MERGE_REQUEST (foundry_forge_merge_request_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryForgeMergeRequest, foundry_forge_merge_request, FOUNDRY, FORGE_MERGE_REQUEST, GObject)

struct _FoundryForgeMergeRequestClass
{
  GObjectClass parent_class;

  char             *(*dup_id)         (FoundryForgeMergeRequest *self);
  char             *(*dup_title)      (FoundryForgeMergeRequest *self);
  char             *(*dup_state)      (FoundryForgeMergeRequest *self);
  char             *(*dup_online_url) (FoundryForgeMergeRequest *self);
  GDateTime        *(*dup_created_at) (FoundryForgeMergeRequest *self);
  FoundryForgeUser *(*dup_author)     (FoundryForgeMergeRequest *self);

  /*< private >*/
  gpointer _reserved[17];
};

FOUNDRY_AVAILABLE_IN_1_1
char             *foundry_forge_merge_request_dup_id         (FoundryForgeMergeRequest *self);
FOUNDRY_AVAILABLE_IN_1_1
char             *foundry_forge_merge_request_dup_state      (FoundryForgeMergeRequest *self);
FOUNDRY_AVAILABLE_IN_1_1
char             *foundry_forge_merge_request_dup_title      (FoundryForgeMergeRequest *self);
FOUNDRY_AVAILABLE_IN_1_1
char             *foundry_forge_merge_request_dup_online_url (FoundryForgeMergeRequest *self);
FOUNDRY_AVAILABLE_IN_1_1
GDateTime        *foundry_forge_merge_request_dup_created_at (FoundryForgeMergeRequest *self);
FOUNDRY_AVAILABLE_IN_1_1
FoundryForgeUser *foundry_forge_merge_request_dup_author     (FoundryForgeMergeRequest *self);

G_END_DECLS
