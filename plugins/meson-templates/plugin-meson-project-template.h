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

typedef struct _PluginMesonProjectTemplateExpansion
{
  const char         *input;
  const char         *output_pattern;
  const char * const *languages;
  gboolean            executable;
} PluginMesonProjectTemplateExpansion;

typedef struct _PluginMesonProjectTemplateLanguageScope
{
  const char *language;
  const char * const *extra_scope;
} PluginMesonProjectTemplateLanguageScope;

#define PLUGIN_TYPE_MESON_PROJECT_TEMPLATE (plugin_meson_project_template_get_type())

G_DECLARE_FINAL_TYPE (PluginMesonProjectTemplate, plugin_meson_project_template, PLUGIN, MESON_PROJECT_TEMPLATE, FoundryProjectTemplate)

void plugin_meson_project_template_set_expansions     (PluginMesonProjectTemplate                    *self,
                                                       const PluginMesonProjectTemplateExpansion     *expansions,
                                                       guint                                          n_expansions);
void plugin_meson_project_template_set_extra_scope    (PluginMesonProjectTemplate                    *self,
                                                       const char * const                            *extra_scope);
void plugin_meson_project_template_set_language_scope (PluginMesonProjectTemplate                    *self,
                                                       const PluginMesonProjectTemplateLanguageScope *language_scope,
                                                       guint                                          n_language_scope);

G_END_DECLS
