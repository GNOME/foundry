/* foundry-text-settings.c
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

#include "config.h"

#include "foundry-text-settings.h"

typedef struct
{
  FoundryTextSettings *next;

  guint8 auto_indent;
  guint8 enable_snippets;
  guint8 highlight_current_line;
  guint8 highlight_diagnostics;
  guint8 implicit_trailing_newline;
  guint8 indent_on_tab;
  guint8 insert_spaces_instead_of_tabs;
  guint8 insert_matching_brace;
  guint8 overwrite_matching_brace;
  guint8 show_line_numbers;
  guint8 show_right_margin;
  guint8 smart_backspace;
  guint8 smart_home_end;
  guint right_margin_position;
  guint tab_width;
  int indent_width;

  guint8 auto_indent_set;
  guint8 enable_snippets_set;
  guint8 highlight_current_line_set;
  guint8 highlight_diagnostics_set;
  guint8 implicit_trailing_newline_set;
  guint8 indent_on_tab_set;
  guint8 insert_spaces_instead_of_tabs_set;
  guint8 insert_matching_brace_set;
  guint8 overwrite_matching_brace_set;
  guint8 show_line_numbers_set;
  guint8 show_right_margin_set;
  guint8 smart_backspace_set;
  guint8 smart_home_end_set;
  guint8 right_margin_position_set;
  guint8 tab_width_set;
  guint8 indent_width_set;
} FoundryTextSettingsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (FoundryTextSettings, foundry_text_settings, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_AUTO_INDENT,
  PROP_ENABLE_SNIPPETS,
  PROP_HIGHLIGHT_CURRENT_LINE,
  PROP_HIGHLIGHT_DIAGNOSTICS,
  PROP_IMPLICIT_TRAILING_NEWLINE,
  PROP_INDENT_ON_TAB,
  PROP_INSERT_MATCHING_BRACE,
  PROP_INSERT_SPACES_INSTEAD_OF_TABS,
  PROP_NEXT,
  PROP_OVERWRITE_MATCHING_BRACE,
  PROP_RIGHT_MARGIN_POSITION,
  PROP_SHOW_LINE_NUMBERS,
  PROP_SHOW_RIGHT_MARGIN,
  PROP_SMART_BACKSPACE,
  PROP_SMART_HOME_END,
  PROP_TAB_WIDTH,
  PROP_INDENT_WIDTH,
  N_PROPS
};

enum {
  CHANGED,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static inline gboolean
is_set (FoundryTextSettings *self,
        goffset              set_offset)
{
  FoundryTextSettingsPrivate *priv = foundry_text_settings_get_instance_private (self);

  return *(guint8 *)((guint8 *)priv + set_offset);
}

static gboolean
get_boolean (FoundryTextSettings *self,
             goffset              value_offset,
             goffset              set_offset,
             gboolean             default_value)
{
  FoundryTextSettingsPrivate *priv = foundry_text_settings_get_instance_private (self);

  g_assert (FOUNDRY_IS_TEXT_SETTINGS (self));

  if (is_set (self, set_offset))
    return *(guint8 *)((guint8 *)priv + value_offset);

  if (priv->next)
    return get_boolean (priv->next, value_offset, set_offset, default_value);

  return default_value;
}

static guint
get_uint (FoundryTextSettings *self,
          goffset              value_offset,
          goffset              set_offset,
          guint                default_value)
{
  FoundryTextSettingsPrivate *priv = foundry_text_settings_get_instance_private (self);

  g_assert (FOUNDRY_IS_TEXT_SETTINGS (self));

  if (is_set (self, set_offset))
    return *(guint *)((guint8 *)priv + value_offset);

  if (priv->next)
    return get_uint (priv->next, value_offset, set_offset, default_value);

  return default_value;
}

static int
get_int (FoundryTextSettings *self,
         goffset              value_offset,
         goffset              set_offset,
         int                  default_value)
{
  FoundryTextSettingsPrivate *priv = foundry_text_settings_get_instance_private (self);

  g_assert (FOUNDRY_IS_TEXT_SETTINGS (self));

  if (is_set (self, set_offset))
    return *(int *)((guint8 *)priv + value_offset);

  if (priv->next)
    return get_int (priv->next, value_offset, set_offset, default_value);

  return default_value;
}

static void
foundry_text_settings_finalize (GObject *object)
{
  FoundryTextSettings *self = (FoundryTextSettings *)object;
  FoundryTextSettingsPrivate *priv = foundry_text_settings_get_instance_private (self);

  g_clear_object (&priv->next);

  G_OBJECT_CLASS (foundry_text_settings_parent_class)->finalize (object);
}

static void
foundry_text_settings_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundryTextSettings *self = FOUNDRY_TEXT_SETTINGS (object);
  FoundryTextSettingsPrivate *priv = foundry_text_settings_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_AUTO_INDENT:
      g_value_set_boolean (value,
                           get_boolean (self,
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, auto_indent),
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, auto_indent_set),
                                        g_value_get_boolean (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_ENABLE_SNIPPETS:
      g_value_set_boolean (value,
                           get_boolean (self,
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, enable_snippets),
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, enable_snippets_set),
                                        g_value_get_boolean (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_HIGHLIGHT_CURRENT_LINE:
      g_value_set_boolean (value,
                           get_boolean (self,
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, highlight_current_line),
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, highlight_current_line_set),
                                        g_value_get_boolean (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_HIGHLIGHT_DIAGNOSTICS:
      g_value_set_boolean (value,
                           get_boolean (self,
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, highlight_diagnostics),
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, highlight_diagnostics_set),
                                        g_value_get_boolean (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_IMPLICIT_TRAILING_NEWLINE:
      g_value_set_boolean (value,
                           get_boolean (self,
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, implicit_trailing_newline),
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, implicit_trailing_newline_set),
                                        g_value_get_boolean (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_INDENT_ON_TAB:
      g_value_set_boolean (value,
                           get_boolean (self,
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, indent_on_tab),
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, indent_on_tab_set),
                                        g_value_get_boolean (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_INSERT_SPACES_INSTEAD_OF_TABS:
      g_value_set_boolean (value,
                           get_boolean (self,
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, insert_spaces_instead_of_tabs),
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, insert_spaces_instead_of_tabs_set),
                                        g_value_get_boolean (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_INSERT_MATCHING_BRACE:
      g_value_set_boolean (value,
                           get_boolean (self,
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, insert_matching_brace),
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, insert_matching_brace_set),
                                        g_value_get_boolean (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_OVERWRITE_MATCHING_BRACE:
      g_value_set_boolean (value,
                           get_boolean (self,
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, overwrite_matching_brace),
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, overwrite_matching_brace_set),
                                        g_value_get_boolean (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_SHOW_LINE_NUMBERS:
      g_value_set_boolean (value,
                           get_boolean (self,
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, show_line_numbers),
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, show_line_numbers_set),
                                        g_value_get_boolean (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_SHOW_RIGHT_MARGIN:
      g_value_set_boolean (value,
                           get_boolean (self,
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, show_right_margin),
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, show_right_margin_set),
                                        g_value_get_boolean (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_SMART_BACKSPACE:
      g_value_set_boolean (value,
                           get_boolean (self,
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, smart_backspace),
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, smart_backspace_set),
                                        g_value_get_boolean (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_SMART_HOME_END:
      g_value_set_boolean (value,
                           get_boolean (self,
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, smart_home_end),
                                        G_STRUCT_OFFSET (FoundryTextSettingsPrivate, smart_home_end_set),
                                        g_value_get_boolean (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_RIGHT_MARGIN_POSITION:
      g_value_set_uint (value,
                        get_uint (self,
                                  G_STRUCT_OFFSET (FoundryTextSettingsPrivate, right_margin_position),
                                  G_STRUCT_OFFSET (FoundryTextSettingsPrivate, right_margin_position_set),
                                  g_value_get_uint (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_TAB_WIDTH:
      g_value_set_uint (value,
                        get_uint (self,
                                  G_STRUCT_OFFSET (FoundryTextSettingsPrivate, tab_width),
                                  G_STRUCT_OFFSET (FoundryTextSettingsPrivate, tab_width_set),
                                  g_value_get_uint (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_INDENT_WIDTH:
      g_value_set_int (value,
                       get_int (self,
                                G_STRUCT_OFFSET (FoundryTextSettingsPrivate, indent_width),
                                G_STRUCT_OFFSET (FoundryTextSettingsPrivate, indent_width_set),
                                g_value_get_int (g_param_spec_get_default_value (pspec))));
      break;

    case PROP_NEXT:
      g_value_set_object (value, priv->next);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_text_settings_class_init (FoundryTextSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_text_settings_finalize;
  object_class->get_property = foundry_text_settings_get_property;

  properties[PROP_AUTO_INDENT] =
    g_param_spec_boolean ("auto-indent", NULL, NULL,
                          TRUE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_ENABLE_SNIPPETS] =
    g_param_spec_boolean ("enable-snippets", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_HIGHLIGHT_CURRENT_LINE] =
    g_param_spec_boolean ("highlight-current-line", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_HIGHLIGHT_DIAGNOSTICS] =
    g_param_spec_boolean ("highlight-diagnostics", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_IMPLICIT_TRAILING_NEWLINE] =
    g_param_spec_boolean ("implicit-trailing-newline", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_INDENT_ON_TAB] =
    g_param_spec_boolean ("indent-on-tab", NULL, NULL,
                          TRUE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_INSERT_SPACES_INSTEAD_OF_TABS] =
    g_param_spec_boolean ("insert-spaces-instead-of-tabs", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_INSERT_MATCHING_BRACE] =
    g_param_spec_boolean ("insert-matching-brace", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_OVERWRITE_MATCHING_BRACE] =
    g_param_spec_boolean ("overwrite-matching-brace", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SHOW_LINE_NUMBERS] =
    g_param_spec_boolean ("show-line-numbers", NULL, NULL,
                          TRUE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SHOW_RIGHT_MARGIN] =
    g_param_spec_boolean ("show-right-margin", NULL, NULL,
                          TRUE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SMART_BACKSPACE] =
    g_param_spec_boolean ("smart-backspace", NULL, NULL,
                          TRUE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SMART_HOME_END] =
    g_param_spec_boolean ("smart-home-end", NULL, NULL,
                          TRUE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_RIGHT_MARGIN_POSITION] =
    g_param_spec_uint ("right-margin-position", NULL, NULL,
                       1, 1000, 80,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_TAB_WIDTH] =
    g_param_spec_uint ("tab-width", NULL, NULL,
                       1, 32, 8,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_INDENT_WIDTH] =
    g_param_spec_int ("indent-width", NULL, NULL,
                       -1, 32, -1,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_NEXT] =
    g_param_spec_object ("next", NULL, NULL,
                         FOUNDRY_TYPE_TEXT_SETTINGS,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (FoundryTextSettingsClass, changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void
foundry_text_settings_init (FoundryTextSettings *self)
{
}
