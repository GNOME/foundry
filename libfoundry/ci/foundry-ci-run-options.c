/* foundry-ci-run-options.c
 *
 * Copyright 2026 Christian Hergert
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

#include "foundry-ci-run-options.h"

/**
 * FoundryCiRunOptions:
 *
 * Provider-independent options for local CI execution.
 *
 * A provider reads a stable snapshot of these values when
 * [method@Foundry.CiProvider.run] or [method@Foundry.CiProvider.run_shell]
 * is called. File descriptors are borrowed; the caller must keep them open
 * until the resulting [class@Foundry.CiRun] completes.
 *
 * Since: 1.2
 */

struct _FoundryCiRunOptions
{
  GObject  parent_instance;
  char    *output_dir;
  guint    max_jobs;
  int      stdin_fd;
  int      stdout_fd;
  int      stderr_fd;
  guint    fail_fast : 1;
  guint    offline : 1;
  guint    save_state : 1;
  guint    save_workspace : 1;
};

G_DEFINE_FINAL_TYPE (FoundryCiRunOptions, foundry_ci_run_options, G_TYPE_OBJECT)

static void
foundry_ci_run_options_finalize (GObject *object)
{
  FoundryCiRunOptions *self = FOUNDRY_CI_RUN_OPTIONS (object);

  g_clear_pointer (&self->output_dir, g_free);

  G_OBJECT_CLASS (foundry_ci_run_options_parent_class)->finalize (object);
}

static void
foundry_ci_run_options_class_init (FoundryCiRunOptionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_ci_run_options_finalize;
}

static void
foundry_ci_run_options_init (FoundryCiRunOptions *self)
{
  self->max_jobs = MAX (1, g_get_num_processors ());
  self->stdin_fd = -1;
  self->stdout_fd = -1;
  self->stderr_fd = -1;
}

/**
 * foundry_ci_run_options_new:
 *
 * Creates options initialized with sensible local execution defaults.
 *
 * Returns: (transfer full): a new [class@Foundry.CiRunOptions]
 *
 * Since: 1.2
 */
FoundryCiRunOptions *
foundry_ci_run_options_new (void)
{
  return g_object_new (FOUNDRY_TYPE_CI_RUN_OPTIONS, NULL);
}

/**
 * foundry_ci_run_options_get_fail_fast:
 * @self: a [class@Foundry.CiRunOptions]
 *
 * Gets whether execution stops after the first failed job.
 *
 * Returns: %TRUE if execution stops after the first failure
 *
 * Since: 1.2
 */
gboolean
foundry_ci_run_options_get_fail_fast (FoundryCiRunOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self), FALSE);

  return self->fail_fast;
}

/**
 * foundry_ci_run_options_set_fail_fast:
 * @self: a [class@Foundry.CiRunOptions]
 * @fail_fast: whether to stop after the first failed job
 *
 * Sets whether execution stops after the first failed job.
 *
 * Since: 1.2
 */
void
foundry_ci_run_options_set_fail_fast (FoundryCiRunOptions *self,
                                      gboolean             fail_fast)
{
  g_return_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self));

  self->fail_fast = !!fail_fast;
}

/**
 * foundry_ci_run_options_get_max_jobs:
 * @self: a [class@Foundry.CiRunOptions]
 *
 * Gets the maximum number of jobs which may run concurrently.
 *
 * Returns: the maximum concurrent job count
 *
 * Since: 1.2
 */
guint
foundry_ci_run_options_get_max_jobs (FoundryCiRunOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self), 0);

  return self->max_jobs;
}

/**
 * foundry_ci_run_options_set_max_jobs:
 * @self: a [class@Foundry.CiRunOptions]
 * @max_jobs: the maximum concurrent job count
 *
 * Sets the maximum number of jobs which may run concurrently.
 *
 * Since: 1.2
 */
void
foundry_ci_run_options_set_max_jobs (FoundryCiRunOptions *self,
                                     guint                max_jobs)
{
  g_return_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self));
  g_return_if_fail (max_jobs > 0);

  self->max_jobs = max_jobs;
}

/**
 * foundry_ci_run_options_get_offline:
 * @self: a [class@Foundry.CiRunOptions]
 *
 * Gets whether network access should be avoided.
 *
 * Returns: %TRUE if the run should remain offline
 *
 * Since: 1.2
 */
gboolean
foundry_ci_run_options_get_offline (FoundryCiRunOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self), FALSE);

  return self->offline;
}

/**
 * foundry_ci_run_options_set_offline:
 * @self: a [class@Foundry.CiRunOptions]
 * @offline: whether network access should be avoided
 *
 * Sets whether the provider should avoid network access.
 *
 * Since: 1.2
 */
void
foundry_ci_run_options_set_offline (FoundryCiRunOptions *self,
                                    gboolean             offline)
{
  g_return_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self));

  self->offline = !!offline;
}

/**
 * foundry_ci_run_options_dup_output_dir:
 * @self: a [class@Foundry.CiRunOptions]
 *
 * Gets the requested directory for durable run output.
 *
 * Returns: (transfer full) (nullable): the output directory
 *
 * Since: 1.2
 */
char *
foundry_ci_run_options_dup_output_dir (FoundryCiRunOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self), NULL);

  return g_strdup (self->output_dir);
}

/**
 * foundry_ci_run_options_set_output_dir:
 * @self: a [class@Foundry.CiRunOptions]
 * @output_dir: (nullable): a directory for durable run output
 *
 * Sets the requested directory for durable run output.
 *
 * Since: 1.2
 */
void
foundry_ci_run_options_set_output_dir (FoundryCiRunOptions *self,
                                       const char          *output_dir)
{
  g_return_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self));

  g_set_str (&self->output_dir, output_dir);
}

/**
 * foundry_ci_run_options_get_save_state:
 * @self: a [class@Foundry.CiRunOptions]
 *
 * Gets whether execution state should be preserved.
 *
 * Returns: %TRUE if execution state should be preserved
 *
 * Since: 1.2
 */
gboolean
foundry_ci_run_options_get_save_state (FoundryCiRunOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self), FALSE);

  return self->save_state;
}

/**
 * foundry_ci_run_options_set_save_state:
 * @self: a [class@Foundry.CiRunOptions]
 * @save_state: whether execution state should be preserved
 *
 * Sets whether execution state should be preserved after the run.
 *
 * Since: 1.2
 */
void
foundry_ci_run_options_set_save_state (FoundryCiRunOptions *self,
                                       gboolean             save_state)
{
  g_return_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self));

  self->save_state = !!save_state;
}

/**
 * foundry_ci_run_options_get_save_workspace:
 * @self: a [class@Foundry.CiRunOptions]
 *
 * Gets whether the job workspace should be preserved.
 *
 * Returns: %TRUE if the workspace should be preserved
 *
 * Since: 1.2
 */
gboolean
foundry_ci_run_options_get_save_workspace (FoundryCiRunOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self), FALSE);

  return self->save_workspace;
}

/**
 * foundry_ci_run_options_set_save_workspace:
 * @self: a [class@Foundry.CiRunOptions]
 * @save_workspace: whether the workspace should be preserved
 *
 * Sets whether the job workspace should be preserved after the run.
 *
 * Enabling this option also enables preservation of execution state.
 *
 * Since: 1.2
 */
void
foundry_ci_run_options_set_save_workspace (FoundryCiRunOptions *self,
                                           gboolean             save_workspace)
{
  g_return_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self));

  self->save_workspace = !!save_workspace;

  if (save_workspace)
    self->save_state = TRUE;
}

/**
 * foundry_ci_run_options_set_fds:
 * @self: a [class@Foundry.CiRunOptions]
 * @stdin_fd: standard input, or -1
 * @stdout_fd: standard output, or -1
 * @stderr_fd: standard error, or -1
 *
 * Sets the borrowed file descriptors used for the run. Passing -1 requests
 * the provider's default behavior for that stream.
 *
 * Since: 1.2
 */
void
foundry_ci_run_options_set_fds (FoundryCiRunOptions *self,
                                int                  stdin_fd,
                                int                  stdout_fd,
                                int                  stderr_fd)
{
  g_return_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self));
  g_return_if_fail (stdin_fd >= -1);
  g_return_if_fail (stdout_fd >= -1);
  g_return_if_fail (stderr_fd >= -1);

  self->stdin_fd = stdin_fd;
  self->stdout_fd = stdout_fd;
  self->stderr_fd = stderr_fd;
}

/**
 * foundry_ci_run_options_get_stdin_fd:
 * @self: a [class@Foundry.CiRunOptions]
 *
 * Gets the borrowed standard input file descriptor.
 *
 * Returns: the standard input file descriptor, or -1
 *
 * Since: 1.2
 */
int
foundry_ci_run_options_get_stdin_fd (FoundryCiRunOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self), -1);

  return self->stdin_fd;
}

/**
 * foundry_ci_run_options_get_stdout_fd:
 * @self: a [class@Foundry.CiRunOptions]
 *
 * Gets the borrowed standard output file descriptor.
 *
 * Returns: the standard output file descriptor, or -1
 *
 * Since: 1.2
 */
int
foundry_ci_run_options_get_stdout_fd (FoundryCiRunOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self), -1);

  return self->stdout_fd;
}

/**
 * foundry_ci_run_options_get_stderr_fd:
 * @self: a [class@Foundry.CiRunOptions]
 *
 * Gets the borrowed standard error file descriptor.
 *
 * Returns: the standard error file descriptor, or -1
 *
 * Since: 1.2
 */
int
foundry_ci_run_options_get_stderr_fd (FoundryCiRunOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_RUN_OPTIONS (self), -1);

  return self->stderr_fd;
}
