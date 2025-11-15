/* foundry-forge.h
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
#include <libpeas.h>

#include "foundry-contextual.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_FORGE (foundry_forge_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryForge, foundry_forge, FOUNDRY, FORGE, FoundryContextual)

struct _FoundryForgeClass
{
  FoundryContextualClass parent_class;

  DexFuture *(*load)         (FoundryForge      *self);
  DexFuture *(*unload)       (FoundryForge      *self);
  DexFuture *(*find_user)    (FoundryForge      *self);
  DexFuture *(*find_project) (FoundryForge      *self);

  /*< private >*/
  gpointer _reserved[20];
};

FOUNDRY_AVAILABLE_IN_1_1
PeasPluginInfo *foundry_forge_dup_plugin_info (FoundryForge      *self);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture      *foundry_forge_find_user       (FoundryForge      *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_1
DexFuture      *foundry_forge_find_project    (FoundryForge      *self) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
