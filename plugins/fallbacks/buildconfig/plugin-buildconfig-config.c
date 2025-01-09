/* plugin-buildconfig-config.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "plugin-buildconfig-config.h"

struct _PluginBuildconfigConfig
{
  FoundryConfig parent_instance;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PluginBuildconfigConfig, plugin_buildconfig_config, FOUNDRY_TYPE_CONFIG)

static GParamSpec *properties[N_PROPS];

static gboolean
plugin_buildconfig_config_can_default (FoundryConfig *config,
                                       guint         *priority)
{
  *priority = 0;
  return TRUE;
}

static void
plugin_buildconfig_config_finalize (GObject *object)
{
  PluginBuildconfigConfig *self = (PluginBuildconfigConfig *)object;

  G_OBJECT_CLASS (plugin_buildconfig_config_parent_class)->finalize (object);
}

static void
plugin_buildconfig_config_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  PluginBuildconfigConfig *self = PLUGIN_BUILDCONFIG_CONFIG (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_buildconfig_config_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  PluginBuildconfigConfig *self = PLUGIN_BUILDCONFIG_CONFIG (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_buildconfig_config_class_init (PluginBuildconfigConfigClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryConfigClass *config_class = FOUNDRY_CONFIG_CLASS (klass);

  object_class->finalize = plugin_buildconfig_config_finalize;
  object_class->get_property = plugin_buildconfig_config_get_property;
  object_class->set_property = plugin_buildconfig_config_set_property;

  config_class->can_default = plugin_buildconfig_config_can_default;
}

static void
plugin_buildconfig_config_init (PluginBuildconfigConfig *self)
{
}

static char **
group_to_strv (GKeyFile   *key_file,
               const char *group)
{
  g_auto(GStrv) env = NULL;
  g_auto(GStrv) keys = NULL;
  gsize len;

  g_assert (key_file != NULL);
  g_assert (group != NULL);

  keys = g_key_file_get_keys (key_file, group, &len, NULL);

  for (gsize i = 0; i < len; i++)
    {
      g_autofree char *value = g_key_file_get_string (key_file, group, keys[i], NULL);

      if (value != NULL)
        env = g_environ_setenv (env, keys[i], value, TRUE);
    }

  return g_steal_pointer (&env);
}

static void
plugin_buildconfig_config_load (PluginBuildconfigConfig *self,
                                GKeyFile                *key_file,
                                const char              *group)
{
  g_autofree char *build_env_key = NULL;
  g_autofree char *runtime_env_key = NULL;
  g_auto(GStrv) build_env = NULL;
  g_auto(GStrv) runtime_env = NULL;

  g_assert (PLUGIN_IS_BUILDCONFIG_CONFIG (self));
  g_assert (key_file != NULL);
  g_assert (group != NULL);

  build_env_key = g_strdup_printf ("%s.environment", group);
  runtime_env_key = g_strdup_printf ("%s.runtime_environment", group);

  build_env = group_to_strv (key_file, build_env_key);
  runtime_env = group_to_strv (key_file, runtime_env_key);
}

PluginBuildconfigConfig *
plugin_buildconfig_config_new (FoundryContext *context,
                               GKeyFile       *key_file,
                               const char     *group)
{
  PluginBuildconfigConfig *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (key_file != NULL, NULL);
  g_return_val_if_fail (group != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_BUILDCONFIG_CONFIG,
                       "context", context,
                       NULL);

  plugin_buildconfig_config_load (self, key_file, group);

  return g_steal_pointer (&self);
}
