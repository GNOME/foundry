/* foundry-forge-project.h
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

#define FOUNDRY_TYPE_FORGE_PROJECT (foundry_forge_project_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryForgeProject, foundry_forge_project, FOUNDRY, FORGE_PROJECT, GObject)

struct _FoundryForgeProjectClass
{
  GObjectClass parent_class;

  char      *(*dup_title)              (FoundryForgeProject *self);
  char      *(*dup_online_url)         (FoundryForgeProject *self);
  char      *(*dup_description)        (FoundryForgeProject *self);
  char      *(*dup_avatar_url)         (FoundryForgeProject *self);
  DexFuture *(*load_avatar)            (FoundryForgeProject *self);
  DexFuture *(*list_issues)            (FoundryForgeProject *self,
                                        FoundryForgeQuery   *query);
  DexFuture *(*list_merge_requests)    (FoundryForgeProject *self,
                                        FoundryForgeQuery   *query);
  char      *(*dup_issues_url)         (FoundryForgeProject *self);
  char      *(*dup_merge_requests_url) (FoundryForgeProject *self);

  /*< private >*/
  gpointer _reserved[22];
};

FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_project_dup_avatar_url         (FoundryForgeProject *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_project_dup_title              (FoundryForgeProject *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_project_dup_description        (FoundryForgeProject *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_project_dup_online_url         (FoundryForgeProject *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_project_dup_issues_url         (FoundryForgeProject *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_project_dup_merge_requests_url (FoundryForgeProject *self);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_forge_project_load_avatar            (FoundryForgeProject *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_forge_project_list_issues            (FoundryForgeProject *self,
                                                         FoundryForgeQuery   *query) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_forge_project_list_merge_requests    (FoundryForgeProject *self,
                                                         FoundryForgeQuery   *query) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
