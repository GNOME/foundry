/* plugin-gitlab-ci-run-private.h
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

#include "foundry-ci-artifact.h"
#include "foundry-ci-run.h"

G_BEGIN_DECLS

#define PLUGIN_TYPE_GITLAB_CI_RUN (plugin_gitlab_ci_run_get_type())

G_DECLARE_FINAL_TYPE (PluginGitlabCiRun, plugin_gitlab_ci_run, PLUGIN, GITLAB_CI_RUN, FoundryCiRun)

PluginGitlabCiRun *plugin_gitlab_ci_run_new             (FoundryContext    *context);
DexCancellable    *plugin_gitlab_ci_run_dup_cancellable (PluginGitlabCiRun *self);
void               plugin_gitlab_ci_run_set_state       (PluginGitlabCiRun *self,
                                                         FoundryCiRunState  state);
void               plugin_gitlab_ci_run_set_progress    (PluginGitlabCiRun *self,
                                                         double             progress);
void               plugin_gitlab_ci_run_set_output_dir  (PluginGitlabCiRun *self,
                                                         const char        *output_dir);
void               plugin_gitlab_ci_run_add_artifact    (PluginGitlabCiRun *self,
                                                         FoundryCiArtifact *artifact);
void               plugin_gitlab_ci_run_complete        (PluginGitlabCiRun *self,
                                                         int                exit_status);
void               plugin_gitlab_ci_run_fail            (PluginGitlabCiRun *self,
                                                         GError            *error);

G_END_DECLS
