/* foundry-forge-user.h
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

#define FOUNDRY_TYPE_FORGE_USER (foundry_forge_user_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryForgeUser, foundry_forge_user, FOUNDRY, FORGE_USER, GObject)

struct _FoundryForgeUserClass
{
  GObjectClass parent_class;

  char      *(*dup_handle)     (FoundryForgeUser *self);
  char      *(*dup_name)       (FoundryForgeUser *self);
  char      *(*dup_avatar_url) (FoundryForgeUser *self);
  char      *(*dup_online_url) (FoundryForgeUser *self);
  char      *(*dup_bio)        (FoundryForgeUser *self);
  char      *(*dup_location)   (FoundryForgeUser *self);
  DexFuture *(*load_avatar)    (FoundryForgeUser *self);

  /*< private >*/
  gpointer _reserved[24];
};

FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_user_dup_handle     (FoundryForgeUser *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_user_dup_name       (FoundryForgeUser *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_user_dup_avatar_url (FoundryForgeUser *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_user_dup_online_url (FoundryForgeUser *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_user_dup_bio        (FoundryForgeUser *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_forge_user_dup_location   (FoundryForgeUser *self);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_forge_user_load_avatar    (FoundryForgeUser *self) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
