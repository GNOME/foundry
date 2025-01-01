/* plugin-flatpak-download-stage.c
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

#include "plugin-flatpak-download-stage.h"

struct _PluginFlatpakDownloadStage
{
  FoundryBuildStage parent_instance;
  char *state_dir;
};

G_DEFINE_FINAL_TYPE (PluginFlatpakDownloadStage, plugin_flatpak_download_stage, FOUNDRY_TYPE_BUILD_STAGE)

enum {
  PROP_0,
  PROP_STATE_DIR,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
plugin_flatpak_download_stage_finalize (GObject *object)
{
  PluginFlatpakDownloadStage *self = (PluginFlatpakDownloadStage *)object;

  g_clear_pointer (&self->state_dir, g_free);

  G_OBJECT_CLASS (plugin_flatpak_download_stage_parent_class)->finalize (object);
}

static void
plugin_flatpak_download_stage_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  PluginFlatpakDownloadStage *self = PLUGIN_FLATPAK_DOWNLOAD_STAGE (object);

  switch (prop_id)
    {
    case PROP_STATE_DIR:
      g_value_set_string (value, self->state_dir);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_download_stage_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  PluginFlatpakDownloadStage *self = PLUGIN_FLATPAK_DOWNLOAD_STAGE (object);

  switch (prop_id)
    {
    case PROP_STATE_DIR:
      self->state_dir = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_download_stage_class_init (PluginFlatpakDownloadStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_flatpak_download_stage_finalize;
  object_class->get_property = plugin_flatpak_download_stage_get_property;
  object_class->set_property = plugin_flatpak_download_stage_set_property;

  properties[PROP_STATE_DIR] =
    g_param_spec_string ("state-dir", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_flatpak_download_stage_init (PluginFlatpakDownloadStage *self)
{
}

FoundryBuildStage *
plugin_flatpak_download_stage_new (FoundryContext *context,
                                   const char     *state_dir)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (state_dir != NULL, NULL);

  return g_object_new (PLUGIN_TYPE_FLATPAK_DOWNLOAD_STAGE,
                       "context", context,
                       "state-dir", state_dir,
                       NULL);
}
