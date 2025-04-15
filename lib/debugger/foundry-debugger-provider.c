/* foundry-debugger-provider.c
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

#include "foundry-debugger-provider-private.h"

G_DEFINE_ABSTRACT_TYPE (FoundryDebuggerProvider, foundry_debugger_provider, FOUNDRY_TYPE_CONTEXTUAL)

static DexFuture *
foundry_debugger_provider_real_load (FoundryDebuggerProvider *self)
{
  return dex_future_new_true ();
}

static DexFuture *
foundry_debugger_provider_real_unload (FoundryDebuggerProvider *self)
{
  return dex_future_new_true ();
}

static void
foundry_debugger_provider_class_init (FoundryDebuggerProviderClass *klass)
{
  klass->load = foundry_debugger_provider_real_load;
  klass->unload = foundry_debugger_provider_real_unload;
}

static void
foundry_debugger_provider_init (FoundryDebuggerProvider *self)
{
}

DexFuture *
foundry_debugger_provider_load (FoundryDebuggerProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DEBUGGER_PROVIDER (self), NULL);

  return FOUNDRY_DEBUGGER_PROVIDER_GET_CLASS (self)->load (self);
}

DexFuture *
foundry_debugger_provider_unload (FoundryDebuggerProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DEBUGGER_PROVIDER (self), NULL);

  return FOUNDRY_DEBUGGER_PROVIDER_GET_CLASS (self)->unload (self);
}
