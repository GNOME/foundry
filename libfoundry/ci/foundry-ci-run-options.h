/* foundry-ci-run-options.h
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

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_CI_RUN_OPTIONS (foundry_ci_run_options_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_FINAL_TYPE (FoundryCiRunOptions, foundry_ci_run_options, FOUNDRY, CI_RUN_OPTIONS, GObject)

FOUNDRY_AVAILABLE_IN_1_2
FoundryCiRunOptions *foundry_ci_run_options_new                (void);
FOUNDRY_AVAILABLE_IN_1_2
gboolean             foundry_ci_run_options_get_fail_fast      (FoundryCiRunOptions *self);
FOUNDRY_AVAILABLE_IN_1_2
void                 foundry_ci_run_options_set_fail_fast      (FoundryCiRunOptions *self,
                                                                gboolean             fail_fast);
FOUNDRY_AVAILABLE_IN_1_2
guint                foundry_ci_run_options_get_max_jobs       (FoundryCiRunOptions *self);
FOUNDRY_AVAILABLE_IN_1_2
void                 foundry_ci_run_options_set_max_jobs       (FoundryCiRunOptions *self,
                                                                guint                max_jobs);
FOUNDRY_AVAILABLE_IN_1_2
gboolean             foundry_ci_run_options_get_offline        (FoundryCiRunOptions *self);
FOUNDRY_AVAILABLE_IN_1_2
void                 foundry_ci_run_options_set_offline        (FoundryCiRunOptions *self,
                                                                gboolean             offline);
FOUNDRY_AVAILABLE_IN_1_2
char                *foundry_ci_run_options_dup_output_dir     (FoundryCiRunOptions *self);
FOUNDRY_AVAILABLE_IN_1_2
void                 foundry_ci_run_options_set_output_dir     (FoundryCiRunOptions *self,
                                                                const char          *output_dir);
FOUNDRY_AVAILABLE_IN_1_2
gboolean             foundry_ci_run_options_get_save_state     (FoundryCiRunOptions *self);
FOUNDRY_AVAILABLE_IN_1_2
void                 foundry_ci_run_options_set_save_state     (FoundryCiRunOptions *self,
                                                                gboolean             save_state);
FOUNDRY_AVAILABLE_IN_1_2
gboolean             foundry_ci_run_options_get_save_workspace (FoundryCiRunOptions *self);
FOUNDRY_AVAILABLE_IN_1_2
void                 foundry_ci_run_options_set_save_workspace (FoundryCiRunOptions *self,
                                                                gboolean             save_workspace);
FOUNDRY_AVAILABLE_IN_1_2
void                 foundry_ci_run_options_set_fds            (FoundryCiRunOptions *self,
                                                                int                  stdin_fd,
                                                                int                  stdout_fd,
                                                                int                  stderr_fd);
FOUNDRY_AVAILABLE_IN_1_2
int                  foundry_ci_run_options_get_stdin_fd       (FoundryCiRunOptions *self);
FOUNDRY_AVAILABLE_IN_1_2
int                  foundry_ci_run_options_get_stdout_fd      (FoundryCiRunOptions *self);
FOUNDRY_AVAILABLE_IN_1_2
int                  foundry_ci_run_options_get_stderr_fd      (FoundryCiRunOptions *self);

G_END_DECLS
