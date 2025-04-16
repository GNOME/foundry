/* foundry-dap-debugger.h
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

#include "foundry-debugger.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_DAP_DEBUGGER (foundry_dap_debugger_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (FoundryDapDebugger, foundry_dap_debugger, FOUNDRY, DAP_DEBUGGER, FoundryDebugger)

struct _FoundryDapDebuggerClass
{
  FoundryDebuggerClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

FOUNDRY_AVAILABLE_IN_ALL
FoundryDebugger *foundry_dap_debugger_new            (FoundryContext     *context,
                                                      GSubprocess        *subprocess,
                                                      GIOStream          *io_stream);
FOUNDRY_AVAILABLE_IN_ALL
GSubprocess     *foundry_dap_debugger_dup_subprocess (FoundryDapDebugger *self);
FOUNDRY_AVAILABLE_IN_ALL
GIOStream       *foundry_dap_debugger_dup_stream     (FoundryDapDebugger *self);

G_END_DECLS
