/* plugin-meson-template-provider.c
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

#include <glib/gi18n-lib.h>

#include "plugin-meson-project-template.h"
#include "plugin-meson-template-provider.h"

struct _PluginMesonTemplateProvider
{
  FoundryTemplateProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginMesonTemplateProvider, plugin_meson_template_provider, FOUNDRY_TYPE_TEMPLATE_PROVIDER)

static PluginMesonTemplateExpansion gtk4_expansions[] = {
  { "meson.build",                                         "meson.build" },
  { "flatpak.json",                                        "{{appid}}.json" },
  { "README.md",                                           "README.md" },
  { "data/hello.desktop.in",                               "data/{{appid}}.desktop.in" },
  { "data/hello.metainfo.xml.in",                          "data/{{appid}}.metainfo.xml.in" },
  { "data/hello.service.in",                               "data/{{appid}}.service.in" },
  { "data/hello.gschema.xml",                              "data/{{appid}}.gschema.xml" },
  { "data/meson.build",                                    "data/meson.build" },
  { "data/icons/meson.build",                              "data/icons/meson.build" },
  { "data/icons/hicolor/scalable/apps/hello.svg",          "data/icons/hicolor/scalable/apps/{{appid}}.svg" },
  { "data/icons/hicolor/symbolic/apps/hello-symbolic.svg", "data/icons/hicolor/symbolic/apps/{{appid}}-symbolic.svg" },
  { "po/LINGUAS",                                          "po/LINGUAS" },
  { "po/meson.build",                                      "po/meson.build" },
  { "po/POTFILES.in",                                      "po/POTFILES.in" },
  { "src/shortcuts-file.ui",                               "src/{{shortcuts_path}}.ui" },

  /* C */
  { "src/application-gtk4.c", "src/{{prefix}}-application.c", FOUNDRY_STRV_INIT ("C") },
  { "src/application-gtk4.h", "src/{{prefix}}-application.h", FOUNDRY_STRV_INIT ("C") },
  { "src/hello.gresource.xml", "src/{{prefix}}.gresource.xml", FOUNDRY_STRV_INIT ("C") },
  { "src/main-gtk4.c", "src/main.c", FOUNDRY_STRV_INIT ("C") },
  { "src/meson-c-vala.build", "src/meson.build", FOUNDRY_STRV_INIT ("C") },
  { "src/window-gtk4.ui", "src/{{prefix}}-window.ui", FOUNDRY_STRV_INIT ("C") },
  { "src/window-gtk4.c", "src/{{prefix}}-window.c", FOUNDRY_STRV_INIT ("C") },
  { "src/window-gtk4.h", "src/{{prefix}}-window.h", FOUNDRY_STRV_INIT ("C") },

  /* JavaScript */
  { "src/hello.gresource.xml", "src/{{appid}}.data.gresource.xml", FOUNDRY_STRV_INIT ("JavaScript") },
  { "src/hello.js.in", "src/{{appid}}.in", FOUNDRY_STRV_INIT ("JavaScript"), TRUE },
  { "src/hello.src.gresource.xml", "src/{{appid}}.src.gresource.xml", FOUNDRY_STRV_INIT ("JavaScript") },
  { "src/main-gtk4.js.tmpl", "src/main.js", FOUNDRY_STRV_INIT ("JavaScript") },
  { "src/meson-js.build", "src/meson.build", FOUNDRY_STRV_INIT ("JavaScript") },
  { "src/window-gtk4.ui", "src/window.ui", FOUNDRY_STRV_INIT ("JavaScript") },
  { "src/window-gtk4.js", "src/window.js", FOUNDRY_STRV_INIT ("JavaScript") },

  /* Python */
  { "src/__init__.py", "src/__init__.py", FOUNDRY_STRV_INIT ("Python") },
  { "src/hello.gresource.xml", "src/{{prefix}}.gresource.xml", FOUNDRY_STRV_INIT ("Python") },
  { "src/hello.py.in", "src/{{name}}.in", FOUNDRY_STRV_INIT ("Python"), TRUE },
  { "src/main-gtk4.py", "src/main.py", FOUNDRY_STRV_INIT ("Python") },
  { "src/meson-py-gtk4.build", "src/meson.build", FOUNDRY_STRV_INIT ("Python") },
  { "src/window-gtk4.py", "src/window.py", FOUNDRY_STRV_INIT ("Python") },
  { "src/window-gtk4.ui", "src/window.ui", FOUNDRY_STRV_INIT ("Python") },

  /* Rust */
  { "src/Cargo-gtk4.toml", "Cargo.toml", FOUNDRY_STRV_INIT ("Rust") },
  { "src/application-gtk4.rs", "src/application.rs", FOUNDRY_STRV_INIT ("Rust") },
  { "src/config-gtk4.rs.in", "src/config.rs.in", FOUNDRY_STRV_INIT ("Rust") },
  { "src/hello.gresource.xml", "src/{{prefix}}.gresource.xml", FOUNDRY_STRV_INIT ("Rust") },
  { "src/main-gtk4.rs", "src/main.rs", FOUNDRY_STRV_INIT ("Rust") },
  { "src/meson-rs-gtk4.build", "src/meson.build", FOUNDRY_STRV_INIT ("Rust") },
  { "src/window-gtk4.rs", "src/window.rs", FOUNDRY_STRV_INIT ("Rust") },
  { "src/window-gtk4.ui", "src/window.ui", FOUNDRY_STRV_INIT ("Rust") },

  /* Vala */
  { "src/application-gtk4.vala", "src/application.vala", FOUNDRY_STRV_INIT ("Vala") },
  { "src/hello.gresource.xml", "src/{{prefix}}.gresource.xml", FOUNDRY_STRV_INIT ("Vala") },
  { "src/main-gtk4.vala", "src/main.vala", FOUNDRY_STRV_INIT ("Vala") },
  { "src/config.vapi", "src/config.vapi", FOUNDRY_STRV_INIT ("Vala") },
  { "src/meson-c-vala.build", "src/meson.build", FOUNDRY_STRV_INIT ("Vala") },
  { "src/window-gtk4.ui", "src/window.ui", FOUNDRY_STRV_INIT ("Vala") },
  { "src/window-gtk4.vala", "src/window.vala", FOUNDRY_STRV_INIT ("Vala") },
};

static const PluginMesonTemplateLanguageScope gtk4_language_scope[] = {
  { "C",          FOUNDRY_STRV_INIT ("ui_file={{prefix}}-window.ui") },
  { "JavaScript", FOUNDRY_STRV_INIT ("exec_name={{appid}}") },
};

static PluginMesonTemplateExpansion library_expansions[] = {
  { "meson.build", "meson.build" },
  { "README.md", "README.md" },
  { "src/meson-clib.build", "src/meson.build" },
  { "src/hello.c", "src/{{name}}.c" },
  { "src/hello.h", "src/{{name}}.h" },
  { "src/hello-version.h.in", "src/{{name}}-version.h.in" },
};

static PluginMesonTemplateExpansion cli_expansions[] = {
  /* Shared */
  { "meson.build", "meson.build" },
  { "flatpak.json", "{{appid}}.json" },
  { "README.md", "README.md" },

  /* C */
  { "src/meson-cli.build", "src/meson.build", FOUNDRY_STRV_INIT ("C") },
  { "src/main-cli.c", "src/main.c", FOUNDRY_STRV_INIT ("C") },

  /* C++ */
  { "src/meson-cli.build", "src/meson.build", FOUNDRY_STRV_INIT ("C++") },
  { "src/main-cli.cpp", "src/main.cpp", FOUNDRY_STRV_INIT ("C++") },

  /* Python */
  { "src/meson-py-cli.build", "src/meson.build", FOUNDRY_STRV_INIT ("Python") },
  { "src/hello-cli.py.in", "src/{{name}}.in", FOUNDRY_STRV_INIT ("Python") },
  { "src/__init__.py", "src/__init__.py", FOUNDRY_STRV_INIT ("Python") },
  { "src/main-cli.py", "src/main.py", FOUNDRY_STRV_INIT ("Python") },

  /* Rust */
  { "src/meson-cli.build", "src/meson.build", FOUNDRY_STRV_INIT ("Rust") },
  { "src/Cargo-cli.toml", "Cargo.toml", FOUNDRY_STRV_INIT ("Rust") },
  { "src/main-cli.rs", "src/main.rs", FOUNDRY_STRV_INIT ("Rust") },

  /* Vala */
  { "src/meson-cli.build", "src/meson.build", FOUNDRY_STRV_INIT ("Vala") },
  { "src/main-cli.vala", "src/main.vala", FOUNDRY_STRV_INIT ("Vala") },
};

static PluginMesonTemplateExpansion empty_expansions[] = {
  /* Shared */
  { "meson.build", "meson.build" },
  { "flatpak.json", "{{appid}}.json" },
  { "README.md", "README.md" },
  { "src/meson-empty.build", "src/meson.build" },

  /* Rust */
  { "src/Cargo-cli.toml", "Cargo.toml", FOUNDRY_STRV_INIT ("Rust") },
};

static const PluginMesonTemplateInfo templates[] = {
  {
    -1000,
    "adwaita",
    N_("GNOME Application"),
    N_("A Meson-based project for GNOME using GTK 4 and libadwaita"),
    FOUNDRY_STRV_INIT ("C", "JavaScript", "Python", "Rust", "Vala"),
    gtk4_expansions, G_N_ELEMENTS (gtk4_expansions),
    gtk4_language_scope, G_N_ELEMENTS (gtk4_language_scope),
    FOUNDRY_STRV_INIT ("is_adwaita=true",
                   "is_gtk4=true",
                   "enable_i18n=true",
                   "enable_gnome=true",
                   "ui_file=window.ui",
                   "exec_name={{name}}",
                   "shortcuts_path=shortcuts-dialog"),
  },
  {
    -900,
    "gtk4",
    N_("GTK 4 Application"),
    N_("A Meson-based project using GTK 4"),
    FOUNDRY_STRV_INIT ("C", "JavaScript", "Python", "Rust", "Vala"),
    gtk4_expansions, G_N_ELEMENTS (gtk4_expansions),
    gtk4_language_scope, G_N_ELEMENTS (gtk4_language_scope),
    FOUNDRY_STRV_INIT ("is_adwaita=false",
                   "is_gtk4=true",
                   "enable_i18n=true",
                   "enable_gnome=true",
                   "ui_file=window.ui",
                   "exec_name={{name}}",
                   "shortcuts_path=gtk/help-overlay"),
  },
  {
    -800,
    "library",
    N_("Shared Library"),
    N_("A Meson-based project for a shared library"),
    FOUNDRY_STRV_INIT ("C"),
    library_expansions, G_N_ELEMENTS (library_expansions),
  },
  {
    -700,
    "cli",
    N_("Command Line Tool"),
    N_("An Meson-based project for a command-line program"),
    FOUNDRY_STRV_INIT ("C", "C++", "Python", "Rust", "Vala"),
    cli_expansions, G_N_ELEMENTS (cli_expansions),
    NULL, 0,
    FOUNDRY_STRV_INIT ("is_cli=true", "exec_name={{name}}"),
  },
  {
    -600,
    "empty",
    N_("Empty Meson Project"),
    N_("An empty Meson project skeleton"),
    FOUNDRY_STRV_INIT ("C", "C++", "C♯", "JavaScript", "Python", "Rust", "Vala"),
    empty_expansions, G_N_ELEMENTS (empty_expansions),
    NULL, 0,
    FOUNDRY_STRV_INIT ("is_cli=true", "exec_name={{name}}"),
  },
};

static DexFuture *
plugin_meson_template_provider_list_project_templates (FoundryTemplateProvider *provider)
{
  g_autoptr(GListStore) store = NULL;

  g_assert (PLUGIN_IS_MESON_TEMPLATE_PROVIDER (provider));

  store = g_list_store_new (FOUNDRY_TYPE_PROJECT_TEMPLATE);

  for (guint i = 0; i < G_N_ELEMENTS (templates); i++)
    {
      const PluginMesonTemplateInfo *info = &templates[i];
      g_autoptr(FoundryProjectTemplate) template = plugin_meson_project_template_new (info);

      g_list_store_append (store, template);
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static void
plugin_meson_template_provider_class_init (PluginMesonTemplateProviderClass *klass)
{
  FoundryTemplateProviderClass *template_provider_class = FOUNDRY_TEMPLATE_PROVIDER_CLASS (klass);

  template_provider_class->list_project_templates = plugin_meson_template_provider_list_project_templates;
}

static void
plugin_meson_template_provider_init (PluginMesonTemplateProvider *self)
{
}
