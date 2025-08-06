/* plugin-gobject-template-provider.c
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

#include "plugin-gobject-code-template.h"
#include "plugin-gobject-template-provider.h"

struct _PluginGobjectTemplateProvider
{
  FoundryTemplateProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginGobjectTemplateProvider, plugin_gobject_template_provider, FOUNDRY_TYPE_TEMPLATE_PROVIDER)

static const PluginGobjectCodeTemplateInfo templates[] = {
  {
    "gobject",
    N_("Create a new GObject class"),
    (PluginGobjectCodeTemplateInput[]) {
      { "filename",
        N_("File Name"),
        N_("The base for the filename such as “my-object”"),
        INPUT_KIND_TEXT,
        "^[\\w\\-_]+$",
        "my-object",
      },
      { "namespace",
        N_("Namespace"),
        N_("The namespace in title case such as “My”"),
        INPUT_KIND_TEXT,
        "^\\w+$",
        "My",
      },
      { "classname",
        N_("Class Name"),
        N_("The class name in title case such as “Object”"),
        INPUT_KIND_TEXT,
        "^\\w+$",
        "Object",
      },
      { "parentclass",
        N_("Parent Class"),
        N_("The parent class in title case such as “GObject”"),
        INPUT_KIND_TEXT,
        "^\\w+$",
        "GObject",
      },
      { "final",
        N_("Final Class"),
        N_("Set final if you do not intend to subclass"),
        INPUT_KIND_SWITCH,
        NULL,
        "true",
      },
    },
    5,
    (PluginGobjectCodeTemplateFile[]) {
      { "gobject.tmpl.c", ".c" },
      { "gobject.tmpl.h", ".h" },
    },
    2,
  },
};

static DexFuture *
plugin_gobject_template_provider_list_code_templates (FoundryTemplateProvider *provider,
                                                      FoundryContext          *context)
{
  PluginGobjectTemplateProvider *self = (PluginGobjectTemplateProvider *)provider;
  g_autoptr(GListStore) store = NULL;

  g_assert (PLUGIN_IS_GOBJECT_TEMPLATE_PROVIDER (self));
  g_assert (!context || FOUNDRY_IS_CONTEXT (context));

  store = g_list_store_new (FOUNDRY_TYPE_CODE_TEMPLATE);

  for (guint i = 0; i < G_N_ELEMENTS (templates); i++)
    {
      g_autoptr(FoundryCodeTemplate) template = plugin_gobject_code_template_new (&templates[i], context);

      g_list_store_append (store, template);
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static void
plugin_gobject_template_provider_class_init (PluginGobjectTemplateProviderClass *klass)
{
  FoundryTemplateProviderClass *provider_class = FOUNDRY_TEMPLATE_PROVIDER_CLASS (klass);

  provider_class->list_code_templates = plugin_gobject_template_provider_list_code_templates;
}

static void
plugin_gobject_template_provider_init (PluginGobjectTemplateProvider *self)
{
}
