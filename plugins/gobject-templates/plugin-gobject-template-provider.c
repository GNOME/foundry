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

#include "plugin-gobject-template-provider.h"

struct _PluginGobjectTemplateProvider
{
  FoundryTemplateProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginGobjectTemplateProvider, plugin_gobject_template_provider, FOUNDRY_TYPE_TEMPLATE_PROVIDER)

static DexFuture *
plugin_gobject_template_provider_list_code_templates (FoundryTemplateProvider *provider,
                                                      FoundryContext          *context)
{
  PluginGobjectTemplateProvider *self = (PluginGobjectTemplateProvider *)provider;
  g_autoptr(GListStore) store = NULL;

  g_assert (PLUGIN_IS_GOBJECT_TEMPLATE_PROVIDER (self));
  g_assert (!context || FOUNDRY_IS_CONTEXT (context));

  store = g_list_store_new (FOUNDRY_TYPE_CODE_TEMPLATE);

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static void
plugin_gobject_template_provider_finalize (GObject *object)
{
  G_OBJECT_CLASS (plugin_gobject_template_provider_parent_class)->finalize (object);
}

static void
plugin_gobject_template_provider_class_init (PluginGobjectTemplateProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryTemplateProviderClass *provider_class = FOUNDRY_TEMPLATE_PROVIDER_CLASS (klass);

  object_class->finalize = plugin_gobject_template_provider_finalize;

  provider_class->list_code_templates = plugin_gobject_template_provider_list_code_templates;
}

static void
plugin_gobject_template_provider_init (PluginGobjectTemplateProvider *self)
{
}
