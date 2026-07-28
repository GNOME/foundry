/* plugin-gitlab-ci-types-private.h
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

#pragma once

#include <libdex.h>

G_BEGIN_DECLS

typedef enum
{
  PLUGIN_GITLAB_CI_EXIT_SUCCESS       = 0,
  PLUGIN_GITLAB_CI_EXIT_JOB_FAILURE   = 1,
  PLUGIN_GITLAB_CI_EXIT_CONFIGURATION = 2,
  PLUGIN_GITLAB_CI_EXIT_INTERRUPTED   = 130,
} PluginGitlabCiExitStatus;

typedef void (*PluginGitlabCiProgressFunc) (double   progress,
                                            gpointer user_data);

typedef struct
{
  int                          jobs;
  char                        *output_dir;
  char                        *result_output_dir;
  int                          stdin_fd;
  int                          stdout_fd;
  int                          stderr_fd;
  gboolean                     offline;
  gboolean                     save_state;
  gboolean                     save_workspace;
  gboolean                     fail_fast;
  gboolean                     shell;
  char                       **job_names;
  PluginGitlabCiProgressFunc   progress_func;
  gpointer                     progress_data;
} PluginGitlabCiRunOptions;

G_END_DECLS
