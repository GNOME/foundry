/* plugin-flatpak-dependency.c
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

#include "plugin-flatpak-dependency.h"

struct _PluginFlatpakDependency
{
  FoundryDependency    parent_instance;
  PluginFlatpakModule *module;
};

G_DEFINE_FINAL_TYPE (PluginFlatpakDependency, plugin_flatpak_dependency, FOUNDRY_TYPE_DEPENDENCY)

enum {
  PROP_0,
  PROP_MODULE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static char *
plugin_flatpak_dependency_dup_name (FoundryDependency *dependency)
{
  return plugin_flatpak_module_dup_name (PLUGIN_FLATPAK_DEPENDENCY (dependency)->module);
}

static void
plugin_flatpak_dependency_finalize (GObject *object)
{
  PluginFlatpakDependency *self = (PluginFlatpakDependency *)object;

  g_clear_object (&self->module);

  G_OBJECT_CLASS (plugin_flatpak_dependency_parent_class)->finalize (object);
}

static void
plugin_flatpak_dependency_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  PluginFlatpakDependency *self = PLUGIN_FLATPAK_DEPENDENCY (object);

  switch (prop_id)
    {
    case PROP_MODULE:
      g_value_set_object (value, self->module);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_dependency_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  PluginFlatpakDependency *self = PLUGIN_FLATPAK_DEPENDENCY (object);

  switch (prop_id)
    {
    case PROP_MODULE:
      self->module = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_dependency_class_init (PluginFlatpakDependencyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryDependencyClass *dependency_class = FOUNDRY_DEPENDENCY_CLASS (klass);

  object_class->finalize = plugin_flatpak_dependency_finalize;
  object_class->get_property = plugin_flatpak_dependency_get_property;
  object_class->set_property = plugin_flatpak_dependency_set_property;

  dependency_class->dup_name = plugin_flatpak_dependency_dup_name;

  properties[PROP_MODULE] =
    g_param_spec_object ("module", NULL, NULL,
                         PLUGIN_TYPE_FLATPAK_MODULE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_flatpak_dependency_init (PluginFlatpakDependency *self)
{
}

PluginFlatpakDependency *
plugin_flatpak_dependency_new (PluginFlatpakModule *module)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_MODULE (module), NULL);

  return g_object_new (PLUGIN_TYPE_FLATPAK_DEPENDENCY,
                       "module", module,
                       NULL);
}
