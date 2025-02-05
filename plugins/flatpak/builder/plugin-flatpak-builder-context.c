/* plugin-flatpak-builder-context.c
 *
 * Copyright 2015 Red Hat, Inc
 * Copyright 2025 Christian Hergert
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
 * Authors:
 *       Alexander Larsson <alexl@redhat.com>
 *       Christian Hergert <chergert@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <flatpak/flatpak.h>

#include "plugin-flatpak-builder-context.h"

struct _PluginFlatpakBuilderContext
{
  GObject parent_instance;

  GFile   *app_dir;
  GFile   *run_dir;
  char    *state_subdir;

  char    *arch;
};

enum {
  PROP_0,
  PROP_APP_DIR,
  PROP_RUN_DIR,
  PROP_STATE_SUBDIR,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PluginFlatpakBuilderContext, plugin_flatpak_builder_context, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
plugin_flatpak_builder_context_finalize (GObject *object)
{
  PluginFlatpakBuilderContext *self = (PluginFlatpakBuilderContext *)object;

  g_clear_object (&self->app_dir);
  g_clear_object (&self->run_dir);
  g_clear_pointer (&self->state_subdir, g_free);

  g_clear_pointer (&self->arch, g_free);

  G_OBJECT_CLASS (plugin_flatpak_builder_context_parent_class)->finalize (object);
}

static void
plugin_flatpak_builder_context_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  PluginFlatpakBuilderContext *self = PLUGIN_FLATPAK_BUILDER_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_APP_DIR:
      g_value_take_object (value, plugin_flatpak_builder_context_dup_app_dir (self));
      break;

    case PROP_RUN_DIR:
      g_value_take_object (value, plugin_flatpak_builder_context_dup_run_dir (self));
      break;

    case PROP_STATE_SUBDIR:
      g_value_take_string (value, plugin_flatpak_builder_context_dup_state_subdir (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_builder_context_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  PluginFlatpakBuilderContext *self = PLUGIN_FLATPAK_BUILDER_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_APP_DIR:
      plugin_flatpak_builder_context_set_app_dir (self, g_value_get_object (value));
      break;

    case PROP_RUN_DIR:
      plugin_flatpak_builder_context_set_run_dir (self, g_value_get_object (value));
      break;

    case PROP_STATE_SUBDIR:
      plugin_flatpak_builder_context_set_state_subdir (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_builder_context_class_init (PluginFlatpakBuilderContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_flatpak_builder_context_finalize;
  object_class->get_property = plugin_flatpak_builder_context_get_property;
  object_class->set_property = plugin_flatpak_builder_context_set_property;

  properties[PROP_APP_DIR] =
    g_param_spec_object ("app-dir", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_RUN_DIR] =
    g_param_spec_object ("run-dir", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_STATE_SUBDIR] =
    g_param_spec_string ("state-subdir", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_flatpak_builder_context_init (PluginFlatpakBuilderContext *self)
{
}

GFile *
plugin_flatpak_builder_context_dup_app_dir (PluginFlatpakBuilderContext *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_BUILDER_CONTEXT (self), NULL);

  return self->app_dir ? g_object_ref (self->app_dir) : NULL;
}

void
plugin_flatpak_builder_context_set_app_dir (PluginFlatpakBuilderContext *self,
                                            GFile                       *app_dir)
{
  g_return_if_fail (PLUGIN_IS_FLATPAK_BUILDER_CONTEXT (self));
  g_return_if_fail (!app_dir || G_IS_FILE (app_dir));

  if (g_set_object (&self->app_dir, app_dir))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_APP_DIR]);
}

GFile *
plugin_flatpak_builder_context_dup_run_dir (PluginFlatpakBuilderContext *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_BUILDER_CONTEXT (self), NULL);

  return self->run_dir ? g_object_ref (self->run_dir) : NULL;
}

void
plugin_flatpak_builder_context_set_run_dir (PluginFlatpakBuilderContext *self,
                                            GFile                       *run_dir)
{
  g_return_if_fail (PLUGIN_IS_FLATPAK_BUILDER_CONTEXT (self));
  g_return_if_fail (!run_dir || G_IS_FILE (run_dir));

  if (g_set_object (&self->run_dir, run_dir))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RUN_DIR]);
}

char *
plugin_flatpak_builder_context_dup_state_subdir (PluginFlatpakBuilderContext *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_BUILDER_CONTEXT (self), NULL);

  return g_strdup (self->state_subdir);
}

void
plugin_flatpak_builder_context_set_state_subdir (PluginFlatpakBuilderContext *self,
                                                 const char                  *state_subdir)
{
  g_return_if_fail (PLUGIN_IS_FLATPAK_BUILDER_CONTEXT (self));

  if (g_set_str (&self->state_subdir, state_subdir))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE_SUBDIR]);
}

char *
plugin_flatpak_builder_context_dup_arch (PluginFlatpakBuilderContext *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_BUILDER_CONTEXT (self), NULL);

  if (self->arch == NULL)
    self->arch = g_strdup (flatpak_get_default_arch ());

  return g_strdup (self->arch);
}
