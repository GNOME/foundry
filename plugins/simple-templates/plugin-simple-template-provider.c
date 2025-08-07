/* plugin-simple-template-provider.c
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

#include "plugin-simple-template-provider.h"

struct _PluginSimpleTemplateProvider
{
  FoundryTemplateProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginSimpleTemplateProvider, plugin_simple_template_provider, FOUNDRY_TYPE_TEMPLATE_PROVIDER)

static DexFuture *
plugin_simple_template_provider_list_code_templates_fiber (GFile          *templates_dir,
                                                           FoundryContext *context)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) children = NULL;

  g_assert (!templates_dir || G_IS_FILE (templates_dir));
  g_assert (!context || FOUNDRY_IS_CONTEXT (context));

  store = g_list_store_new (FOUNDRY_TYPE_CODE_TEMPLATE);

  if ((children = g_resources_enumerate_children ("/app/devsuite/foundry/templates/", 0, NULL)))
    {
      for (guint i = 0; children[i]; i++)
        {
          g_autoptr(FoundryCodeTemplate) template = NULL;
          g_autofree char *uri = g_strconcat ("resource:///app/devsuite/foundry/templates/", children[i], NULL);
          g_autoptr(GFile) file = g_file_new_for_uri (uri);
          g_autoptr(GError) parse_error = NULL;

          if ((template = dex_await_object (foundry_simple_code_template_new (context, file), &parse_error)))
            g_list_store_append (store, template);
          else
            g_debug ("Failed to parse template `%s`: %s", uri, parse_error->message);
        }
    }

  if (templates_dir != NULL &&
      (enumerator = dex_await_object (dex_file_enumerate_children (templates_dir,
                                                                   G_FILE_ATTRIBUTE_STANDARD_NAME",",
                                                                   G_FILE_QUERY_INFO_NONE,
                                                                   G_PRIORITY_DEFAULT),
                                      &error)))
    {
      gpointer infosptr;

      while ((infosptr = dex_await_boxed (dex_file_enumerator_next_files (enumerator, 100, G_PRIORITY_DEFAULT), &error)))
        {
          g_autolist(GFileInfo) infos = infosptr;

          for (const GList *iter = infos; iter; iter = iter->next)
            {
              GFileInfo *info = iter->data;
              const char *name = g_file_info_get_name (info);
              g_autoptr(FoundryCodeTemplate) template = NULL;
              g_autoptr(GFile) file = NULL;
              g_autoptr(GError) parse_error = NULL;

              /* Skip dot files */
              if (name == NULL || name[0] == '.')
                continue;

              file = g_file_enumerator_get_child (enumerator, info);

              if ((template = dex_await_object (foundry_simple_code_template_new (context, file), &parse_error)))
                g_list_store_append (store, template);
              else
                g_debug ("Failed to parse template: %s", parse_error->message);
            }
        }
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static DexFuture *
plugin_simple_template_provider_list_code_templates (FoundryTemplateProvider *provider,
                                                     FoundryContext          *context)
{
  g_autoptr(GFile) state_dir = NULL;
  g_autoptr(GFile) templates_dir = NULL;

  g_assert (PLUGIN_IS_SIMPLE_TEMPLATE_PROVIDER (provider));
  g_assert (!context || FOUNDRY_IS_CONTEXT (context));

  if (context != NULL)
    {
      state_dir = foundry_context_dup_state_directory (context);
      templates_dir = g_file_get_child (state_dir, "templates");
    }

  return foundry_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                                  G_CALLBACK (plugin_simple_template_provider_list_code_templates_fiber),
                                  2,
                                  G_TYPE_FILE, templates_dir,
                                  FOUNDRY_TYPE_CONTEXT, context);
}

static void
plugin_simple_template_provider_class_init (PluginSimpleTemplateProviderClass *klass)
{
  FoundryTemplateProviderClass *template_provider_class = FOUNDRY_TEMPLATE_PROVIDER_CLASS (klass);

  template_provider_class->list_code_templates = plugin_simple_template_provider_list_code_templates;
}

static void
plugin_simple_template_provider_init (PluginSimpleTemplateProvider *self)
{
}
