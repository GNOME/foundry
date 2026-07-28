/* plugin-gitlab-ci-error-private.h
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
  PLUGIN_GITLAB_CI_ERROR_INVALID_ARGUMENT,
  PLUGIN_GITLAB_CI_ERROR_NOT_FOUND,
  PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
  PLUGIN_GITLAB_CI_ERROR_LIMIT_EXCEEDED,
  PLUGIN_GITLAB_CI_ERROR_UNSUPPORTED,
  PLUGIN_GITLAB_CI_ERROR_POLICY,
  PLUGIN_GITLAB_CI_ERROR_NETWORK,
} PluginGitlabCiError;

#define PLUGIN_GITLAB_CI_ERROR (plugin_gitlab_ci_error_quark ())

GQuark plugin_gitlab_ci_error_quark (void);

G_END_DECLS
