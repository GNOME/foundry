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

#include <stdio.h>

#include "foundry-dap-debugger-source-private.h"
#include "foundry-dap-debugger-stack-frame-private.h"
#include "foundry-json-node.h"

struct _FoundryDapDebuggerStackFrame
{
  FoundryDebuggerStackFrame parent_instance;
  GWeakRef debugger_wr;
  JsonNode *node;
};

G_DEFINE_FINAL_TYPE (FoundryDapDebuggerStackFrame, foundry_dap_debugger_stack_frame, FOUNDRY_TYPE_DEBUGGER_STACK_FRAME)

static guint64
foundry_dap_debugger_stack_frame_get_instruction_pointer (FoundryDebuggerStackFrame *stack_frame)
{
  FoundryDapDebuggerStackFrame *self = FOUNDRY_DAP_DEBUGGER_STACK_FRAME (stack_frame);
  const char *pc = NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "instructionPointerReference", FOUNDRY_JSON_NODE_GET_STRING (&pc)))
    {
      guint64 addr;

      if (sscanf (pc, "0x%"G_GINT64_MODIFIER"x", &addr) == 1)
        return addr;
    }

  return 0;
}

static char *
foundry_dap_debugger_stack_frame_dup_id (FoundryDebuggerStackFrame *stack_frame)
{
  FoundryDapDebuggerStackFrame *self = FOUNDRY_DAP_DEBUGGER_STACK_FRAME (stack_frame);
  gint64 id = 0;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "id", FOUNDRY_JSON_NODE_GET_INT (&id)))
    return g_strdup_printf ("%"G_GINT64_FORMAT, id);

  return NULL;
}

static char *
foundry_dap_debugger_stack_frame_dup_name (FoundryDebuggerStackFrame *stack_frame)
{
  FoundryDapDebuggerStackFrame *self = FOUNDRY_DAP_DEBUGGER_STACK_FRAME (stack_frame);
  const char *name = NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "name", FOUNDRY_JSON_NODE_GET_STRING (&name)))
    return g_strdup (name);

  return g_strdup ("??");
}

static char *
foundry_dap_debugger_stack_frame_dup_module_id (FoundryDebuggerStackFrame *stack_frame)
{
  FoundryDapDebuggerStackFrame *self = FOUNDRY_DAP_DEBUGGER_STACK_FRAME (stack_frame);
  const char *module_id = NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "moduleId", FOUNDRY_JSON_NODE_GET_STRING (&module_id)))
    return g_strdup (module_id);

  return NULL;
}

static gboolean
foundry_dap_debugger_stack_frame_can_restart (FoundryDebuggerStackFrame *stack_frame)
{
  FoundryDapDebuggerStackFrame *self = FOUNDRY_DAP_DEBUGGER_STACK_FRAME (stack_frame);
  gboolean can_restart = FALSE;

  FOUNDRY_JSON_OBJECT_PARSE (self->node, "canRestart", FOUNDRY_JSON_NODE_GET_BOOLEAN (&can_restart));

  return can_restart;
}

static void
foundry_dap_debugger_stack_frame_get_source_range (FoundryDebuggerStackFrame *stack_frame,
                                                   guint                     *begin_line,
                                                   guint                     *begin_line_offset,
                                                   guint                     *end_line,
                                                   guint                     *end_line_offset)
{
  FoundryDapDebuggerStackFrame *self = FOUNDRY_DAP_DEBUGGER_STACK_FRAME (stack_frame);
  gint64 value = 0;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "line", FOUNDRY_JSON_NODE_GET_INT (&value)))
    *begin_line = value;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "endLine", FOUNDRY_JSON_NODE_GET_INT (&value)))
    *end_line = value;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "column", FOUNDRY_JSON_NODE_GET_INT (&value)))
    *begin_line_offset = value;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "endColumn", FOUNDRY_JSON_NODE_GET_INT (&value)))
    *end_line_offset = value;
}

static FoundryDebuggerSource *
foundry_dap_debugger_stack_frame_dup_source (FoundryDebuggerStackFrame *stack_frame)
{
  FoundryDapDebuggerStackFrame *self = FOUNDRY_DAP_DEBUGGER_STACK_FRAME (stack_frame);
  g_autoptr(FoundryDapDebugger) debugger = NULL;
  JsonNode *source = NULL;

  if (!(debugger = g_weak_ref_get (&self->debugger_wr)))
    return NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "source", FOUNDRY_JSON_NODE_GET_NODE (&source)))
    return foundry_dap_debugger_source_new (debugger, source);

  return NULL;
}

static void
foundry_dap_debugger_stack_frame_finalize (GObject *object)
{
  FoundryDapDebuggerStackFrame *self = (FoundryDapDebuggerStackFrame *)object;

  g_weak_ref_clear (&self->debugger_wr);
  g_clear_pointer (&self->node, json_node_unref);

  G_OBJECT_CLASS (foundry_dap_debugger_stack_frame_parent_class)->finalize (object);
}

static void
foundry_dap_debugger_stack_frame_class_init (FoundryDapDebuggerStackFrameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryDebuggerStackFrameClass *stack_frame_class = FOUNDRY_DEBUGGER_STACK_FRAME_CLASS (klass);

  object_class->finalize = foundry_dap_debugger_stack_frame_finalize;

  stack_frame_class->can_restart = foundry_dap_debugger_stack_frame_can_restart;
  stack_frame_class->dup_id = foundry_dap_debugger_stack_frame_dup_id;
  stack_frame_class->dup_module_id = foundry_dap_debugger_stack_frame_dup_module_id;
  stack_frame_class->dup_name = foundry_dap_debugger_stack_frame_dup_name;
  stack_frame_class->get_instruction_pointer = foundry_dap_debugger_stack_frame_get_instruction_pointer;
  stack_frame_class->get_source_range = foundry_dap_debugger_stack_frame_get_source_range;
  stack_frame_class->dup_source = foundry_dap_debugger_stack_frame_dup_source;
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
  self->node = json_node_ref (node);

  return FOUNDRY_DEBUGGER_STACK_FRAME (g_steal_pointer (&self));
}
