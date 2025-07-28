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

#include "plugin-meson-template-provider.h"

struct _PluginMesonTemplateProvider
{
  FoundryTemplateProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginMesonTemplateProvider, plugin_meson_template_provider, FOUNDRY_TYPE_TEMPLATE_PROVIDER)

static void
plugin_meson_template_provider_dispose (GObject *object)
{
  PluginMesonTemplateProvider *self = (PluginMesonTemplateProvider *)object;

  G_OBJECT_CLASS (plugin_meson_template_provider_parent_class)->dispose (object);
}

static void
plugin_meson_template_provider_class_init (PluginMesonTemplateProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = plugin_meson_template_provider_dispose;
}

static void
plugin_meson_template_provider_init (PluginMesonTemplateProvider *self)
{
}
