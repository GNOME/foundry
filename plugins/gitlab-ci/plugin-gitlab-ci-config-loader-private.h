/* plugin-gitlab-ci-config-loader-private.h
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
#include <json-glib/json-glib.h>

#include "plugin-gitlab-ci-context-private.h"

G_BEGIN_DECLS

typedef struct _PluginGitlabCiConfigLoader PluginGitlabCiConfigLoader;

PluginGitlabCiConfigLoader *plugin_gitlab_ci_config_loader_new   (FoundryContext             *foundry_context,
                                                                  PluginGitlabCiContext      *context,
                                                                  gboolean                    offline);
PluginGitlabCiConfigLoader *plugin_gitlab_ci_config_loader_ref   (PluginGitlabCiConfigLoader *self);
void                        plugin_gitlab_ci_config_loader_unref (PluginGitlabCiConfigLoader *self);
DexFuture                  *plugin_gitlab_ci_config_loader_load  (PluginGitlabCiConfigLoader *self) G_GNUC_WARN_UNUSED_RESULT;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (PluginGitlabCiConfigLoader, plugin_gitlab_ci_config_loader_unref)

G_END_DECLS
