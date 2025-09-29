/* foundry-dap-debugger-variable.c
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

#include "config.h"

#include "foundry-dap-debugger-variable-private.h"
#include "foundry-json-node.h"

struct _FoundryDapDebuggerVariable
{
  FoundryDebuggerVariable parent_instance;
  JsonNode *node;
};

G_DEFINE_FINAL_TYPE (FoundryDapDebuggerVariable, foundry_dap_debugger_variable, FOUNDRY_TYPE_DEBUGGER_VARIABLE)

static void
foundry_dap_debugger_variable_finalize (GObject *object)
{
  FoundryDapDebuggerVariable *self = (FoundryDapDebuggerVariable *)object;

  g_clear_pointer (&self->node, json_node_unref);

  G_OBJECT_CLASS (foundry_dap_debugger_variable_parent_class)->finalize (object);
}

static void
foundry_dap_debugger_variable_class_init (FoundryDapDebuggerVariableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_dap_debugger_variable_finalize;
}

static void
foundry_dap_debugger_variable_init (FoundryDapDebuggerVariable *self)
{
}

FoundryDebuggerVariable *
foundry_dap_debugger_variable_new (JsonNode *node)
{
  FoundryDapDebuggerVariable *self;

  self = g_object_new (FOUNDRY_TYPE_DAP_DEBUGGER_VARIABLE, NULL);
  self->node = json_node_ref (node);

  return FOUNDRY_DEBUGGER_VARIABLE (self);
}
