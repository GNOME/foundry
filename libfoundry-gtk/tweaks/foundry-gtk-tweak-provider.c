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

#define APP_DEVSUITE_FOUNDRY_TEXT "app.devsuite.foundry.text"
#define LANGUAGE_SETTINGS_PATH    "/app/devsuite/foundry/text/@language@/"

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
    .section = "core",
    .sort_key = "010-010",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/editor/",
    .title = N_("Text Editor"),
    .icon_name = "document-edit-symbolic",
    .display_hint = "menu",
    .section = "core",
    .sort_key = "010-020",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/terminal/",
    .title = N_("Terminal"),
    .icon_name = "utilities-terminal-symbolic",
    .section = "core",
    .sort_key = "010-030",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/languages/",
    .title = N_("Programming Languages"),
    .icon_name = "text-x-javascript-symbolic",
    .display_hint = "menu",
    .section = "languages",
    .sort_key = "020-010",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/project/",
    .title = N_("Projects"),
    .icon_name = "folder-symbolic",
    .display_hint = "page",
    .section = "projects",
    .sort_key = "030-010",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/plugins/",
    .title = N_("Plugins"),
    .icon_name = "plugin-symbolic",
    .display_hint = "menu",
    .section = "plugins",
    .sort_key = "050-010",
  },
};

static const FoundryTweakInfo language_infos[] = {
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .subpath = "/",
    .title = "@Language@",
    .sort_key = "@section@-@Language@",
    .display_hint = "page",
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
    .subpath = "/formatting/show-right-margin",
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
    .subpath = "/formatting/insert-matching-brace",
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
    .subpath = "/formatting/overwrite-matching-brace",
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

static DexFuture *
foundry_gtk_tweak_provider_load (FoundryTweakProvider *provider)
{
  static const char *prefixes[] = {"/app", "/project", "/user"};
  GtkSourceLanguageManager *manager;
  const char * const *language_ids;

  g_assert (FOUNDRY_IS_GTK_TWEAK_PROVIDER (provider));

  manager = gtk_source_language_manager_get_default ();
  language_ids = gtk_source_language_manager_get_language_ids (manager);

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
          const char *section = gtk_source_language_get_section (language);
          g_autofree char *path = g_strdup_printf ("%s/languages/%s/", prefix, language_id);
          g_auto(GStrv) environ_ = NULL;

          if (gtk_source_language_get_hidden (language))
            continue;

          environ_ = g_environ_setenv (environ_, "language", language_id, TRUE);
          environ_ = g_environ_setenv (environ_, "Language", name, TRUE);
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
