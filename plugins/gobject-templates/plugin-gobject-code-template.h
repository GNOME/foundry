/* plugin-gobject-code-template.h
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
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

#define PLUGIN_TYPE_GOBJECT_CODE_TEMPLATE (plugin_gobject_code_template_get_type())

G_DECLARE_FINAL_TYPE (PluginGobjectCodeTemplate, plugin_gobject_code_template, PLUGIN, GOBJECT_CODE_TEMPLATE, FoundryCodeTemplate)

enum {
  INPUT_KIND_TEXT,
  INPUT_KIND_SWITCH,
};

typedef struct _PluginGobjectCodeTemplateInput
{
  const char *id;
  const char *title;
  const char *subtitle;
  guint       input_kind;
  const char *regex;
  const char *value;
} PluginGobjectCodeTemplateInput;

typedef struct _PluginGobjectCodeTemplateFile
{
  const char *resource;
  const char *suffix;
} PluginGobjectCodeTemplateFile;

typedef struct _PluginGobjectCodeTemplateInfo
{
  const char                     *id;
  const char                     *description;
  PluginGobjectCodeTemplateInput *inputs;
  guint                           n_inputs;
  PluginGobjectCodeTemplateFile  *files;
  guint                           n_files;
} PluginGobjectCodeTemplateInfo;

FoundryCodeTemplate *plugin_gobject_code_template_new (const PluginGobjectCodeTemplateInfo *info,
                                                       FoundryContext                      *context);

G_END_DECLS
