/* plugin-gitlab-ci-pipeline-private.h
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

#include "foundry-ci-pipeline.h"

#include "plugin-gitlab-ci-context-private.h"
#include "plugin-gitlab-ci-job-private.h"

G_BEGIN_DECLS

#define PLUGIN_TYPE_GITLAB_CI_PIPELINE (plugin_gitlab_ci_pipeline_get_type())

G_DECLARE_FINAL_TYPE (PluginGitlabCiPipeline, plugin_gitlab_ci_pipeline, PLUGIN, GITLAB_CI_PIPELINE, FoundryCiPipeline)

struct _PluginGitlabCiPipeline
{
  FoundryCiPipeline      parent_instance;
  FoundryCiProvider     *provider;
  PluginGitlabCiContext *context;
  GPtrArray             *stages;
  GPtrArray             *jobs;
  GHashTable            *jobs_by_name;
  char                  *digest;
  gboolean               workflow_selected;
  char                  *workflow_reason;
};

PluginGitlabCiPipeline *plugin_gitlab_ci_pipeline_new         (FoundryCiProvider       *provider,
                                                               PluginGitlabCiContext   *context);
PluginGitlabCiContext  *plugin_gitlab_ci_pipeline_dup_context (PluginGitlabCiPipeline  *self);
void                    plugin_gitlab_ci_pipeline_add_job     (PluginGitlabCiPipeline  *self,
                                                               PluginGitlabCiJob       *job);
PluginGitlabCiJob      *plugin_gitlab_ci_pipeline_lookup_job  (PluginGitlabCiPipeline  *self,
                                                               const char              *name);
gboolean                plugin_gitlab_ci_pipeline_validate    (PluginGitlabCiPipeline  *self,
                                                               GError                 **error);

G_END_DECLS
