/* plugin-gitlab-ci-job-private.h
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

#include "foundry-ci-job.h"

G_BEGIN_DECLS

typedef enum
{
  PLUGIN_GITLAB_CI_JOB_STATUS_SELECTED,
  PLUGIN_GITLAB_CI_JOB_STATUS_SKIPPED,
  PLUGIN_GITLAB_CI_JOB_STATUS_MANUAL,
  PLUGIN_GITLAB_CI_JOB_STATUS_UNSUPPORTED,
} PluginGitlabCiJobStatus;

typedef struct _PluginGitlabCiVariable PluginGitlabCiVariable;
typedef struct _PluginGitlabCiNeed     PluginGitlabCiNeed;

struct _PluginGitlabCiVariable
{
  char     *name;
  char     *value;
  gboolean  secret;
  gboolean  file;
};

struct _PluginGitlabCiNeed
{
  char     *job;
  gboolean  artifacts;
};

#define PLUGIN_TYPE_GITLAB_CI_JOB (plugin_gitlab_ci_job_get_type())

G_DECLARE_FINAL_TYPE (PluginGitlabCiJob, plugin_gitlab_ci_job, PLUGIN, GITLAB_CI_JOB, FoundryCiJob)

struct _PluginGitlabCiJob
{
  FoundryCiJob             parent_instance;
  FoundryCiPipeline       *pipeline;
  FoundryCiProvider       *provider;
  char                    *name;
  char                    *stage;
  char                    *image;
  char                    *image_user;
  GPtrArray               *before_script;
  GPtrArray               *script;
  GPtrArray               *after_script;
  GPtrArray               *needs;
  GPtrArray               *dependencies;
  GPtrArray               *artifact_paths;
  GHashTable              *variables;
  GPtrArray               *unsupported;
  char                    *artifacts_when;
  char                    *when;
  char                    *timeout;
  gboolean                 allow_failure;
  gboolean                 has_explicit_needs;
  int                      retry;
  PluginGitlabCiJobStatus  status;
  char                    *reason;
};

PluginGitlabCiVariable *plugin_gitlab_ci_variable_new         (const char              *name,
                                                               const char              *value,
                                                               gboolean                 secret,
                                                               gboolean                 file);
void                    plugin_gitlab_ci_variable_free        (PluginGitlabCiVariable  *self);
PluginGitlabCiNeed     *plugin_gitlab_ci_need_new             (const char              *job,
                                                               gboolean                 artifacts);
void                    plugin_gitlab_ci_need_free            (PluginGitlabCiNeed      *self);
PluginGitlabCiJob      *plugin_gitlab_ci_job_new              (FoundryCiProvider       *provider,
                                                               FoundryCiPipeline       *pipeline);
const char             *plugin_gitlab_ci_job_status_to_string (PluginGitlabCiJobStatus  status);

G_END_DECLS
