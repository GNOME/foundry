/* foundry-debugger.h
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

#include "foundry-contextual.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_DEBUGGER (foundry_debugger_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (FoundryDebugger, foundry_debugger, FOUNDRY, DEBUGGER, FoundryContextual)

struct _FoundryDebuggerClass
{
  FoundryContextualClass parent_class;

  char       *(*dup_name)           (FoundryDebugger       *self);
  DexFuture  *(*initialize)         (FoundryDebugger       *self);
  DexFuture  *(*connect_to_target)  (FoundryDebugger       *self,
                                     FoundryDebuggerTarget *target);
  GListModel *(*list_address_space) (FoundryDebugger       *self);
  GListModel *(*list_modules)       (FoundryDebugger       *self);
  GListModel *(*list_traps)         (FoundryDebugger       *self);

  /*< private >*/
  gpointer _reserved[32];
};

FOUNDRY_AVAILABLE_IN_ALL
char       *foundry_debugger_dup_name           (FoundryDebugger       *self);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture  *foundry_debugger_initialize         (FoundryDebugger       *self);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture  *foundry_debugger_connect_to_target  (FoundryDebugger       *self,
                                                 FoundryDebuggerTarget *target);
FOUNDRY_AVAILABLE_IN_ALL
GListModel *foundry_debugger_list_address_space (FoundryDebugger       *self);
FOUNDRY_AVAILABLE_IN_ALL
GListModel *foundry_debugger_list_modules       (FoundryDebugger       *self);
FOUNDRY_AVAILABLE_IN_ALL
GListModel *foundry_debugger_list_traps         (FoundryDebugger       *self);

G_END_DECLS
