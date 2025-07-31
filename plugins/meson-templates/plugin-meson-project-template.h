/* plugin-meson-project-template.h
 *
 * Copyright 2022-2025 Christian Hergert <chergert@redhat.com>
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

#include <foundry.h>

G_BEGIN_DECLS

typedef struct _PluginMesonTemplateExpansion
{
  const char         *input;
  const char         *output_pattern;
  const char * const *languages;
  gboolean            executable;
} PluginMesonTemplateExpansion;

typedef struct _PluginMesonTemplateLanguageScope
{
  const char *language;
  const char * const *extra_scope;
} PluginMesonTemplateLanguageScope;

typedef struct _PluginMesonTemplateInfo
{
  int                                     priority;
  const char                             *id;
  const char                             *name;
  const char                             *description;
  const char * const                     *languages;
  const PluginMesonTemplateExpansion     *expansions;
  guint                                   n_expansions;
  const PluginMesonTemplateLanguageScope *language_scope;
  guint                                   n_language_scope;
  const char * const                     *extra_scope;
  const char * const                     *tags;
} PluginMesonTemplateInfo;

#define PLUGIN_TYPE_MESON_PROJECT_TEMPLATE (plugin_meson_project_template_get_type())

G_DECLARE_FINAL_TYPE (PluginMesonProjectTemplate, plugin_meson_project_template, PLUGIN, MESON_PROJECT_TEMPLATE, FoundryProjectTemplate)

FoundryProjectTemplate *plugin_meson_project_template_new (const PluginMesonTemplateInfo *info);

G_END_DECLS
