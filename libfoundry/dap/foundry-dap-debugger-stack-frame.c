/* foundry-dap-debugger-stack-frame.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-dap-debugger-stack-frame-private.h"

struct _FoundryDapDebuggerStackFrame
{
  FoundryDebuggerStackFrame parent_instance;
  GWeakRef debugger_wr;
};

G_DEFINE_FINAL_TYPE (FoundryDapDebuggerStackFrame, foundry_dap_debugger_stack_frame, FOUNDRY_TYPE_DEBUGGER_STACK_FRAME)

static void
foundry_dap_debugger_stack_frame_finalize (GObject *object)
{
  FoundryDapDebuggerStackFrame *self = (FoundryDapDebuggerStackFrame *)object;

  g_weak_ref_clear (&self->debugger_wr);

  G_OBJECT_CLASS (foundry_dap_debugger_stack_frame_parent_class)->finalize (object);
}

static void
foundry_dap_debugger_stack_frame_class_init (FoundryDapDebuggerStackFrameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_dap_debugger_stack_frame_finalize;
}

static void
foundry_dap_debugger_stack_frame_init (FoundryDapDebuggerStackFrame *self)
{
  g_weak_ref_init (&self->debugger_wr, NULL);
}

FoundryDebuggerStackFrame *
foundry_dap_debugger_stack_frame_new (FoundryDapDebugger *debugger,
                                      JsonNode           *node)
{
  g_autoptr(FoundryDapDebuggerStackFrame) self = NULL;

  self = g_object_new (FOUNDRY_TYPE_DAP_DEBUGGER_STACK_FRAME, NULL);
  g_weak_ref_set (&self->debugger_wr, debugger);

  return FOUNDRY_DEBUGGER_STACK_FRAME (g_steal_pointer (&self));
}
