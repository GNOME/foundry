/* foundry-debugger-breakpoint.c
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

#include "foundry-debugger-breakpoint.h"
#include "foundry-util.h"

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryDebuggerBreakpoint, foundry_debugger_breakpoint, FOUNDRY_TYPE_DEBUGGER_TRAP)

static GParamSpec *properties[N_PROPS];

static void
foundry_debugger_breakpoint_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_debugger_breakpoint_class_init (FoundryDebuggerBreakpointClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_debugger_breakpoint_get_property;
}

static void
foundry_debugger_breakpoint_init (FoundryDebuggerBreakpoint *self)
{
}

/**
 * foundry_debugger_breakpoint_remove:
 * @self: a [class@Foundry.DebuggerBreakpoint]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves
 *   to any value or rejects with error.
 */
DexFuture *
foundry_debugger_breakpoint_remove (FoundryDebuggerBreakpoint *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_DEBUGGER_BREAKPOINT (self));

  if (FOUNDRY_DEBUGGER_BREAKPOINT_GET_CLASS (self)->remove)
    return FOUNDRY_DEBUGGER_BREAKPOINT_GET_CLASS (self)->remove (self);

  return foundry_future_new_not_supported ();
}
