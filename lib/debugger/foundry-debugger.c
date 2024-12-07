/* foundry-debugger.c
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

#include "config.h"

#include <glib/gi18n.h>

#include "foundry-debugger-private.h"

G_DEFINE_ABSTRACT_TYPE (FoundryDebugger, foundry_debugger, FOUNDRY_TYPE_CONTEXTUAL)

static DexFuture *
foundry_debugger_real_load (FoundryDebugger *self)
{
  return dex_future_new_true ();
}

static DexFuture *
foundry_debugger_real_unload (FoundryDebugger *self)
{
  return dex_future_new_true ();
}

static void
foundry_debugger_class_init (FoundryDebuggerClass *klass)
{
  klass->load = foundry_debugger_real_load;
  klass->unload = foundry_debugger_real_unload;
}

static void
foundry_debugger_init (FoundryDebugger *self)
{
}

DexFuture *
foundry_debugger_load (FoundryDebugger *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DEBUGGER (self), NULL);

  return FOUNDRY_DEBUGGER_GET_CLASS (self)->load (self);
}

DexFuture *
foundry_debugger_unload (FoundryDebugger *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DEBUGGER (self), NULL);

  return FOUNDRY_DEBUGGER_GET_CLASS (self)->unload (self);
}

/**
 * foundry_debugger_dup_name:
 * @self: a #FoundryDebugger
 *
 * Gets a name for the provider that is expected to be displayed to
 * users such as "Flatpak".
 *
 * Returns: (transfer full): the name of the provider
 */
char *
foundry_debugger_dup_name (FoundryDebugger *self)
{
  char *ret = NULL;

  g_return_val_if_fail (FOUNDRY_IS_DEBUGGER (self), NULL);

  if (FOUNDRY_DEBUGGER_GET_CLASS (self)->dup_name)
    ret = FOUNDRY_DEBUGGER_GET_CLASS (self)->dup_name (self);

  if (ret == NULL)
    ret = g_strdup (G_OBJECT_TYPE_NAME (self));

  return g_steal_pointer (&ret);
}
