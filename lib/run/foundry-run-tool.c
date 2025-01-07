/* foundry-run-tool.c
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

#include <glib/gi18n-lib.h>

#include "foundry-build-pipeline.h"
#include "foundry-command.h"
#include "foundry-process-launcher.h"
#include "foundry-run-tool-private.h"

typedef struct
{
  GSubprocess *subprocess;
} FoundryRunToolPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryRunTool, foundry_run_tool, FOUNDRY_TYPE_CONTEXTUAL)

static DexFuture *
foundry_run_tool_real_send_signal (FoundryRunTool *self,
                                   int             signum)
{
  FoundryRunToolPrivate *priv = foundry_run_tool_get_instance_private (self);

  g_assert (FOUNDRY_IS_RUN_TOOL (self));

  if (priv->subprocess != NULL)
    g_subprocess_send_signal (priv->subprocess, signum);

  return dex_future_new_true ();
}

static DexFuture *
foundry_run_tool_real_force_exit (FoundryRunTool *self)
{
  FoundryRunToolPrivate *priv = foundry_run_tool_get_instance_private (self);

  g_assert (FOUNDRY_IS_RUN_TOOL (self));

  if (priv->subprocess != NULL)
    g_subprocess_force_exit (priv->subprocess);

  return dex_future_new_true ();
}

static void
foundry_run_tool_class_init (FoundryRunToolClass *klass)
{
  klass->send_signal = foundry_run_tool_real_send_signal;
  klass->force_exit = foundry_run_tool_real_force_exit;
}

static void
foundry_run_tool_init (FoundryRunTool *self)
{
}

/**
 * foundry_run_tool_force_exit:
 * @self: a #FoundryRunTool
 *
 * Requests the application exit.
 *
 * The future resolves when the signal has been sent or equivalent
 * operation. That does not mean the process has stopped and depends
 * on where the tool is running (such as a remote device).
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when the
 *   request to force exit has been sent.
 */
DexFuture *
foundry_run_tool_force_exit (FoundryRunTool *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_RUN_TOOL (self));

  FOUNDRY_CONTEXTUAL_MESSAGE (self, "%s", _("Forcing exit of tool"));

  return FOUNDRY_RUN_TOOL_GET_CLASS (self)->force_exit (self);
}

/**
 * foundry_run_tool_send_signal:
 * @self: a #FoundryRunTool
 *
 * Sends a signal to the running application.
 *
 * The future resolves when the signal has been sent. There is no guarantee
 * of signal delivery.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when the
 *   signal has been sent.
 */
DexFuture *
foundry_run_tool_send_signal (FoundryRunTool *self,
                              int             signum)
{
  dex_return_error_if_fail (FOUNDRY_IS_RUN_TOOL (self));

  FOUNDRY_CONTEXTUAL_MESSAGE (self,
                              _("Sending signal %d to tool"),
                              signum);

  return FOUNDRY_RUN_TOOL_GET_CLASS (self)->send_signal (self, signum);
}

/**
 * foundry_run_tool_prepare:
 * @self: a #FoundryRunTool
 *
 * Prepares @launcher to run @command using the run tool.
 *
 * The resulting future resolves when preparation has completed.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   any value.
 */
DexFuture *
foundry_run_tool_prepare (FoundryRunTool         *self,
                          FoundryBuildPipeline   *pipeline,
                          FoundryCommand         *command,
                          FoundryProcessLauncher *launcher)
{
  dex_return_error_if_fail (FOUNDRY_IS_RUN_TOOL (self));
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_PIPELINE (self));
  dex_return_error_if_fail (FOUNDRY_IS_COMMAND (command));
  dex_return_error_if_fail (FOUNDRY_IS_PROCESS_LAUNCHER (launcher));

  return FOUNDRY_RUN_TOOL_GET_CLASS (self)->prepare (self, pipeline, command, launcher);
}
