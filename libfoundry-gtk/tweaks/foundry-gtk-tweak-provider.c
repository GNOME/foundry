/* foundry-gtk-tweak-provider.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include <gtksourceview/gtksource.h>

#include "foundry-gtk-tweak-provider-private.h"

#define APP_DEVSUITE_FOUNDRY_EDITOR   "app.devsuite.foundry.editor"
#define APP_DEVSUITE_FOUNDRY_TERMINAL "app.devsuite.foundry.terminal"
#define APP_DEVSUITE_FOUNDRY_TEXT     "app.devsuite.foundry.text"
#define LANGUAGE_SETTINGS_PATH        "/app/devsuite/foundry/text/@language@/"

struct _FoundryGtkTweakProvider
{
  FoundryTweakProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (FoundryGtkTweakProvider, foundry_gtk_tweak_provider, FOUNDRY_TYPE_TWEAK_PROVIDER)

static const FoundryTweakInfo top_page_info[] = {
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/shortcuts/",
    .title = N_("Keyboard Shortcuts"),
    .icon_name = "preferences-desktop-keyboard-shortcuts-symbolic",
    .section = "-core",
    .sort_key = "010-010",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/editor/",
    .title = N_("Text Editor"),
    .icon_name = "document-edit-symbolic",
    .display_hint = "menu",
    .section = "-core",
    .sort_key = "010-020",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/terminal/",
    .title = N_("Terminal"),
    .icon_name = "utilities-terminal-symbolic",
    .section = "-core",
    .sort_key = "010-030",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/languages/",
    .title = N_("Programming Languages"),
    .icon_name = "text-x-javascript-symbolic",
    .display_hint = "menu",
    .section = "-languages",
    .sort_key = "020-010",
  },
};

static const FoundryTweakInfo language_infos[] = {
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/",
    .title = "@Language@",
    .sort_key = "@section@-@Language@",
    .display_hint = "page",
    .icon_name = "@icon@",
    .section = "@section@",
#ifdef HAVE_PLUGIN_EDITORCONFIG
    .subtitle = N_("Settings provided by .editorconfig and modelines take precedence over those below."),
#endif
  },

  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/formatting/",
    .title = N_("Indentation & Formatting"),
    .sort_key = "001",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/formatting/implicit-trailing-newline",
    .title = N_("Insert Trailing Newline"),
    .subtitle = N_("Ensure files end with a newline"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_TEXT,
      .setting.path = LANGUAGE_SETTINGS_PATH,
      .setting.key = "implicit-trailing-newline",
    },
  },

  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/indentation/",
    .sort_key = "010",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/indentation/auto-indent",
    .title = N_("Auto Indent"),
    .subtitle = N_("Automatically indent while you type"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_TEXT,
      .setting.path = LANGUAGE_SETTINGS_PATH,
      .setting.key = "auto-indent",
    },
  },

  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/margin/",
    .title = N_("Margin"),
    .sort_key = "030",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/margin/show-right-margin",
    .title = N_("Show Right Margin"),
    .subtitle = N_("Draw an indicator showing the right margin position"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_TEXT,
      .setting.path = LANGUAGE_SETTINGS_PATH,
      .setting.key = "show-right-margin",
    },
  },

  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/behavior/",
    .title = N_("Behavior"),
    .sort_key = "050",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/behavior/insert-matching-brace",
    .title = N_("Insert Matching Brace"),
    .subtitle = N_("Insert matching braces when typing an opening brace"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_TEXT,
      .setting.path = LANGUAGE_SETTINGS_PATH,
      .setting.key = "insert-matching-brace",
    },
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/behavior/overwrite-matching-brace",
    .title = N_("Overwrite Matching Brace"),
    .subtitle = N_("Overwrite matching braces when typing"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_TEXT,
      .setting.path = LANGUAGE_SETTINGS_PATH,
      .setting.key = "overwrite-matching-brace",
    },
  },
};

static const FoundryTweakInfo editor_infos[] = {
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/styling",
    .title = N_("Fonts & Styling"),
    .sort_key = "010",
    .icon_name = "font-select-symbolic",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/styling/font",
    .sort_key = "010",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/styling/font/custom-font",
    .title = N_("Use Custom Font"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_EDITOR,
      .setting.key = "use-custom-font",
    },
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_FONT,
    .subpath = "/styling/font/custom-font/font",
    .title = N_("Custom Font"),
    .flags = FOUNDRY_TWEAK_INFO_FONT_MONOSPACE,
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_EDITOR,
      .setting.key = "custom-font",
    },
  },

  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/styling/lines",
    .title = N_("Lines"),
    .sort_key = "020",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/styling/lines/numbers",
    .title = N_("Show Line Numbers"),
    .subtitle = N_("Show line numbers next to each line"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_EDITOR,
      .setting.key = "show-line-numbers",
    },
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/styling/lines/changes",
    .title = N_("Show Line Changes"),
    .subtitle = N_("Describe how a line was changed next to each line"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_EDITOR,
      .setting.key = "show-line-changes",
    },
  },

  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/styling/highlighting",
    .title = N_("Highlighting"),
    .sort_key = "030",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/styling/highlighting/current-line",
    .title = N_("Highlight Current Line"),
    .subtitle = N_("Make the current line stand out with highlights"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_EDITOR,
      .setting.key = "highlight-current-line",
    },
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/styling/highlighting/matching-brackets",
    .title = N_("Highlight Matching Brackets"),
    .subtitle = N_("Use cursor position to highlight matching brackets, braces, parenthesis, and more"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_EDITOR,
      .setting.key = "highlight-matching-brackets",
    },
  },

  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/styling/highlighting2",
    .sort_key = "031",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/styling/highlighting2/diagnostics",
    .title = N_("Highlight Diagnostics"),
    .subtitle = N_("Show diagnostics in the text editor"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_EDITOR,
      .setting.key = "highlight-diagnostics",
    },
  },
};

static const FoundryTweakInfo terminal_infos[] = {
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/fonts",
    .title = N_("Fonts & Styling"),
    .sort_key = "010",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/fonts/custom-font",
    .title = N_("Use Custom Font"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_TERMINAL,
      .setting.key = "use-custom-font",
    },
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_FONT,
    .subpath = "/fonts/custom-font/font",
    .title = N_("Custom Font"),
    .flags = FOUNDRY_TWEAK_INFO_FONT_MONOSPACE,
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_TERMINAL,
      .setting.key = "custom-font",
    },
  },

  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/styling",
    .sort_key = "020",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/styling/allow-bold",
    .title = N_("Allow Bold"),
    .subtitle = N_("Allow the use of bold escape sequences"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_TERMINAL,
      .setting.key = "allow-bold",
    },
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/styling/allow-hyperlinks",
    .title = N_("Allow Hyperlinks"),
    .subtitle = N_("Allow the use of hyperlinks escape sequences"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_TERMINAL,
      .setting.key = "allow-hyperlink",
    },
  },

  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/scrolling",
    .title = N_("Scrolling"),
    .sort_key = "030",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/scrolling/scroll-on-output",
    .title = N_("Scroll On Output"),
    .subtitle = N_("Automatically scroll when applications within the terminal output text"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_TERMINAL,
      .setting.key = "scroll-on-output",
    },
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/scrolling/scroll-on-keystroke",
    .title = N_("Scroll On Keyboard Input"),
    .subtitle = N_("Automatically scroll when typing to insert text"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_TERMINAL,
      .setting.key = "scroll-on-keystroke",
    },
  },

  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/history",
    .title = N_("History"),
    .sort_key = "040",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .subpath = "/history/limit-scrollback",
    .title = N_("Limit Scrollback"),
    .subtitle = N_("Limit the number of lines that are stored in memory for terminal scrollback"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_TERMINAL,
      .setting.key = "limit-scrollback",
    },
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SPIN,
    .subpath = "/history/max-scrollback-lines",
    .title = N_("Maximum Lines in Scrollback"),
    .subtitle = N_("The maximum number of lines stored in history when limiting scrollback"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = APP_DEVSUITE_FOUNDRY_TERMINAL,
      .setting.key = "max-scrollback-lines",
    },
  },
};

static char *
find_icon_name (GtkSourceLanguage *language)
{
  g_auto(GStrv) mime_types = gtk_source_language_get_mime_types (language);
  const char *suffix = gtk_source_language_get_metadata (language, "suggested-suffix");
  g_autofree char *filename = NULL;

  if (suffix != NULL)
    filename = g_strdup_printf ("file%s", suffix);

  if (mime_types != NULL)
    {
      for (guint i = 0; mime_types[i]; i++)
        {
          g_autofree char *content_type = g_content_type_from_mime_type (mime_types[i]);

          if (!foundry_str_empty0 (content_type))
            {
              g_autoptr(GIcon) icon = foundry_file_manager_find_symbolic_icon (NULL, content_type, filename);

              if (icon != NULL)
                return g_icon_to_string (icon);
            }
        }
    }

  return g_strdup ("text-x-generic-symbolic");
}

static DexFuture *
foundry_gtk_tweak_provider_load (FoundryTweakProvider *provider)
{
  static const char *prefixes[] = {"/app", "/project", "/user"};
  GtkSourceLanguageManager *manager;
  const char * const *language_ids;

  g_assert (FOUNDRY_IS_GTK_TWEAK_PROVIDER (provider));

  manager = gtk_source_language_manager_get_default ();
  language_ids = gtk_source_language_manager_get_language_ids (manager);

  foundry_tweak_provider_register (provider,
                                   GETTEXT_PACKAGE,
                                   "/app/terminal",
                                   terminal_infos,
                                   G_N_ELEMENTS (terminal_infos),
                                   NULL);

  foundry_tweak_provider_register (provider,
                                   GETTEXT_PACKAGE,
                                   "/app/editor",
                                   editor_infos,
                                   G_N_ELEMENTS (editor_infos),
                                   NULL);

  for (guint i = 0; i < G_N_ELEMENTS (prefixes); i++)
    {
      const char *prefix = prefixes[i];

      foundry_tweak_provider_register (provider,
                                       GETTEXT_PACKAGE,
                                       prefix,
                                       top_page_info,
                                       G_N_ELEMENTS (top_page_info),
                                       NULL);

      for (guint j = 0; language_ids[j]; j++)
        {
          const char *language_id = language_ids[j];
          GtkSourceLanguage *language = gtk_source_language_manager_get_language (manager, language_id);
          const char *name = gtk_source_language_get_name (language);
          g_autofree char *path = g_strdup_printf ("%s/languages/%s/", prefix, language_id);
          g_autofree char *icon_name = NULL;
          g_auto(GStrv) environ_ = NULL;
          const char *section;

          if (gtk_source_language_get_hidden (language))
            continue;

          if (!(section = gtk_source_language_get_section (language)))
            section = "";

          icon_name = find_icon_name (language);

          environ_ = g_environ_setenv (environ_, "language", language_id, TRUE);
          environ_ = g_environ_setenv (environ_, "Language", name, TRUE);
          environ_ = g_environ_setenv (environ_, "icon", icon_name, TRUE);
          environ_ = g_environ_setenv (environ_, "section", section, TRUE);

          foundry_tweak_provider_register (provider,
                                           GETTEXT_PACKAGE,
                                           path,
                                           language_infos,
                                           G_N_ELEMENTS (language_infos),
                                           (const char * const *)environ_);
        }
    }


  return dex_future_new_true ();
}

static void
foundry_gtk_tweak_provider_class_init (FoundryGtkTweakProviderClass *klass)
{
  FoundryTweakProviderClass *tweak_provider_class = FOUNDRY_TWEAK_PROVIDER_CLASS (klass);

  tweak_provider_class->load = foundry_gtk_tweak_provider_load;
}

static void
foundry_gtk_tweak_provider_init (FoundryGtkTweakProvider *self)
{
}
