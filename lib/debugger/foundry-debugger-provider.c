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

#include "foundry-build-pipeline.h"
#include "foundry-command.h"
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

static DexFuture *
foundry_debugger_provider_real_load_debugger (FoundryDebuggerProvider *self,
                                              FoundryBuildPipeline    *pipeline)
{
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not supported");
}

static DexFuture *
foundry_debugger_provider_real_supports (FoundryDebuggerProvider *self,
                                         FoundryBuildPipeline    *pipeline,
                                         FoundryCommand          *command)
{
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not supported");
}

static void
foundry_debugger_provider_class_init (FoundryDebuggerProviderClass *klass)
{
  klass->load = foundry_debugger_provider_real_load;
  klass->unload = foundry_debugger_provider_real_unload;
  klass->load_debugger = foundry_debugger_provider_real_load_debugger;
  klass->supports = foundry_debugger_provider_real_supports;
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

/**
 * foundry_debugger_provider_supports:
 * @self: a [class@Foundry.DebuggerProvider]
 * @pipeline: (nullable): a [class@Foundry.BuildPipeline]
 * @command: a [class@Foundry.Command]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   an integer of the priority of the debugger (larger value is
 *   higher priority) or rejects with error.
 */
DexFuture *
foundry_debugger_provider_supports (FoundryDebuggerProvider *self,
                                    FoundryBuildPipeline    *pipeline,
                                    FoundryCommand          *command)
{
  dex_return_error_if_fail (FOUNDRY_IS_DEBUGGER_PROVIDER (self));
  dex_return_error_if_fail (!pipeline || FOUNDRY_IS_BUILD_PIPELINE (pipeline));
  dex_return_error_if_fail (FOUNDRY_IS_COMMAND (command));

  return FOUNDRY_DEBUGGER_PROVIDER_GET_CLASS (self)->supports (self, pipeline, command);
}

/**
 * foundry_debugger_provider_load_debugger:
 * @self: a [class@Foundry.DebuggerProvider]
 * @pipeline: a [class@Foundry.BuildPipeline]
 *
 * Returns: (transfer full):
 */
DexFuture *
foundry_debugger_provider_load_debugger (FoundryDebuggerProvider *self,
                                         FoundryBuildPipeline    *pipeline)
{
  dex_return_error_if_fail (FOUNDRY_IS_DEBUGGER_PROVIDER (self));
  dex_return_error_if_fail (!pipeline || FOUNDRY_IS_BUILD_PIPELINE (pipeline));

  return FOUNDRY_DEBUGGER_PROVIDER_GET_CLASS (self)->load_debugger (self, pipeline);
}
