/* foundry-debugger-variable.h
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

#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_DEBUGGER_VARIABLE (foundry_debugger_variable_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (FoundryDebuggerVariable, foundry_debugger_variable, FOUNDRY, DEBUGGER_VARIABLE, GObject)

struct _FoundryDebuggerVariableClass
{
  GObjectClass parent_class;

  char *(*dup_name)      (FoundryDebuggerVariable *self);
  char *(*dup_value)     (FoundryDebuggerVariable *self);
  char *(*dup_type_name) (FoundryDebuggerVariable *self);

  /*< private >*/
  gpointer _reserved[12];
};

FOUNDRY_AVAILABLE_IN_1_1
char *foundry_debugger_variable_dup_name      (FoundryDebuggerVariable *self);
FOUNDRY_AVAILABLE_IN_1_1
char *foundry_debugger_variable_dup_value     (FoundryDebuggerVariable *self);
FOUNDRY_AVAILABLE_IN_1_1
char *foundry_debugger_variable_dup_type_name (FoundryDebuggerVariable *self);

G_END_DECLS
