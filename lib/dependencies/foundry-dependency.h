/* foundry-dependency.h
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

#include "foundry-contextual.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_DEPENDENCY (foundry_dependency_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (FoundryDependency, foundry_dependency, FOUNDRY, DEPENDENCY, FoundryContextual)

struct _FoundryDependencyClass
{
  FoundryContextualClass parent_class;

  char      *(*dup_name)     (FoundryDependency *self);
  char      *(*dup_kind)     (FoundryDependency *self);
  char      *(*dup_location) (FoundryDependency *self);
  DexFuture *(*update)       (FoundryDependency *self,
                              DexCancellable    *cancellable,
                              int                pty_fd);

  /*< private >*/
  gpointer _reserved[8];
};

FOUNDRY_AVAILABLE_IN_ALL
char      *foundry_dependency_dup_kind     (FoundryDependency *self);
FOUNDRY_AVAILABLE_IN_ALL
char      *foundry_dependency_dup_name     (FoundryDependency *self);
FOUNDRY_AVAILABLE_IN_ALL
char      *foundry_dependency_dup_location (FoundryDependency *self);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture *foundry_dependency_update       (FoundryDependency *self,
                                            DexCancellable    *cancellable,
                                            int                pty_fd);

G_END_DECLS
