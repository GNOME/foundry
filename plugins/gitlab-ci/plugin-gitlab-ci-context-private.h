/* plugin-gitlab-ci-context-private.h
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

#include "foundry-types.h"

G_BEGIN_DECLS

typedef struct _PluginGitlabCiContext PluginGitlabCiContext;

#define PLUGIN_TYPE_GITLAB_CI_CONTEXT (plugin_gitlab_ci_context_get_type())

struct _PluginGitlabCiContext
{
  gatomicrefcount  ref_count;
  char            *repository_root;
  char            *configuration_path;
  char            *server_host;
  GHashTable      *variables;
  GHashTable      *files;
  GHashTable      *changed_files;
};

GType                  plugin_gitlab_ci_context_get_type     (void);
DexFuture             *plugin_gitlab_ci_context_new          (FoundryContext        *context) G_GNUC_WARN_UNUSED_RESULT;
PluginGitlabCiContext *plugin_gitlab_ci_context_ref          (PluginGitlabCiContext *self);
void                   plugin_gitlab_ci_context_unref        (PluginGitlabCiContext *self);
const char            *plugin_gitlab_ci_context_get_variable (PluginGitlabCiContext *self,
                                                              const char            *name);
gboolean               plugin_gitlab_ci_context_file_exists  (PluginGitlabCiContext *self,
                                                              const char            *pattern);
gboolean               plugin_gitlab_ci_context_file_changed (PluginGitlabCiContext *self,
                                                              const char            *pattern);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (PluginGitlabCiContext, plugin_gitlab_ci_context_unref)

G_END_DECLS
