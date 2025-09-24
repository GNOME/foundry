/* foundry-dap-debugger-module.c
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

#include "foundry-dap-debugger-module-private.h"

struct _FoundryDapDebuggerModule
{
  FoundryDebuggerModule parent_instance;
  char *id;
  char *name;
  char *path;
};

G_DEFINE_FINAL_TYPE (FoundryDapDebuggerModule, foundry_dap_debugger_module, FOUNDRY_TYPE_DEBUGGER_MODULE)

static char *
foundry_dap_debugger_module_dup_id (FoundryDebuggerModule *module)
{
  FoundryDapDebuggerModule *self = FOUNDRY_DAP_DEBUGGER_MODULE (module);

  return g_strdup (self->id);
}

static char *
foundry_dap_debugger_module_dup_name (FoundryDebuggerModule *module)
{
  FoundryDapDebuggerModule *self = FOUNDRY_DAP_DEBUGGER_MODULE (module);

  return g_strdup (self->name);
}

static char *
foundry_dap_debugger_module_dup_path (FoundryDebuggerModule *module)
{
  FoundryDapDebuggerModule *self = FOUNDRY_DAP_DEBUGGER_MODULE (module);

  return g_strdup (self->path);
}

static void
foundry_dap_debugger_module_finalize (GObject *object)
{
  FoundryDapDebuggerModule *self = (FoundryDapDebuggerModule *)object;

  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->path, g_free);

  G_OBJECT_CLASS (foundry_dap_debugger_module_parent_class)->finalize (object);
}

static void
foundry_dap_debugger_module_class_init (FoundryDapDebuggerModuleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryDebuggerModuleClass *module_class = FOUNDRY_DEBUGGER_MODULE_CLASS (klass);

  object_class->finalize = foundry_dap_debugger_module_finalize;

  module_class->dup_id = foundry_dap_debugger_module_dup_id;
  module_class->dup_name = foundry_dap_debugger_module_dup_name;
  module_class->dup_path = foundry_dap_debugger_module_dup_path;
}

static void
foundry_dap_debugger_module_init (FoundryDapDebuggerModule *self)
{
}

FoundryDebuggerModule *
foundry_dap_debugger_module_new (const char *id,
                                 const char *name,
                                 const char *path)
{
  FoundryDapDebuggerModule *self;

  self = g_object_new (FOUNDRY_TYPE_DAP_DEBUGGER_MODULE, NULL);
  self->id = g_strdup (id);
  self->name = g_strdup (name);
  self->path = g_strdup (path);

  return FOUNDRY_DEBUGGER_MODULE (self);
}
