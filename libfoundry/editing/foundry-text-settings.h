/* foundry-text-settings.h
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

#include <glib-object.h>

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_TEXT_SETTINGS (foundry_text_settings_get_type())

typedef enum _FoundryTextSetting
{
  FOUNDRY_TEXT_SETTING_NONE = 0,
  FOUNDRY_TEXT_SETTING_AUTO_INDENT,
  FOUNDRY_TEXT_SETTING_ENABLE_SNIPPETS,
  FOUNDRY_TEXT_SETTING_HIGHLIGHT_CURRENT_LINE,
  FOUNDRY_TEXT_SETTING_HIGHLIGHT_DIAGNOSTICS,
  FOUNDRY_TEXT_SETTING_IMPLICIT_TRAILING_NEWLINE,
  FOUNDRY_TEXT_SETTING_INDENT_ON_TAB,
  FOUNDRY_TEXT_SETTING_INSERT_SPACES_INSTEAD_OF_TABS,
  FOUNDRY_TEXT_SETTING_INSERT_MATCHING_BRACE,
  FOUNDRY_TEXT_SETTING_OVERWRITE_MATCHING_BRACE,
  FOUNDRY_TEXT_SETTING_SHOW_LINE_NUMBERS,
  FOUNDRY_TEXT_SETTING_SHOW_RIGHT_MARGIN,
  FOUNDRY_TEXT_SETTING_SMART_BACKSPACE,
  FOUNDRY_TEXT_SETTING_SMART_HOME_END,
  FOUNDRY_TEXT_SETTING_RIGHT_MARGIN_POSITION,
  FOUNDRY_TEXT_SETTING_TAB_WIDTH,
  FOUNDRY_TEXT_SETTING_INDENT_WIDTH,
} FoundryTextSetting;

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (FoundryTextSettings, foundry_text_settings, FOUNDRY, TEXT_SETTINGS, GObject)

struct _FoundryTextSettingsClass
{
  GObjectClass parent_class;

  void     (*changed)     (FoundryTextSettings *self);
  gboolean (*get_setting) (FoundryTextSettings *self,
                           FoundryTextSetting   setting,
                           GValue              *value);

  /*< private >*/
  gpointer _reserved[8];
};

FOUNDRY_AVAILABLE_IN_ALL
gboolean foundry_text_settings_get_setting  (FoundryTextSettings *self,
                                             FoundryTextSetting   setting,
                                             GValue              *value);
FOUNDRY_AVAILABLE_IN_ALL
void     foundry_text_settings_emit_changed (FoundryTextSettings *self);

G_END_DECLS
