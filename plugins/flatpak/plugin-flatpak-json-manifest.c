/* plugin-flatpak-json-manifest.c
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

#include "plugin-flatpak-json-manifest.h"
#include "plugin-flatpak-manifest-private.h"

struct _PluginFlatpakJsonManifest
{
  PluginFlatpakManifest parent_instance;
};

struct _PluginFlatpakJsonManifestClass
{
  PluginFlatpakManifestClass parent_class;
};

G_DEFINE_FINAL_TYPE (PluginFlatpakJsonManifest, plugin_flatpak_json_manifest, PLUGIN_TYPE_FLATPAK_MANIFEST)

static void
plugin_flatpak_json_manifest_finalize (GObject *object)
{
  PluginFlatpakJsonManifest *self = (PluginFlatpakJsonManifest *)object;

  G_OBJECT_CLASS (plugin_flatpak_json_manifest_parent_class)->finalize (object);
}

static void
plugin_flatpak_json_manifest_class_init (PluginFlatpakJsonManifestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_flatpak_json_manifest_finalize;
}

static void
plugin_flatpak_json_manifest_init (PluginFlatpakJsonManifest *self)
{
}

static DexFuture *
plugin_flatpak_json_manifest_load_fiber (gpointer user_data)
{
  PluginFlatpakJsonManifest *self = user_data;
  g_autoptr(GFile) file = NULL;

  g_assert (PLUGIN_IS_FLATPAK_JSON_MANIFEST (self));

  file = plugin_flatpak_manifest_dup_file (PLUGIN_FLATPAK_MANIFEST (self));

  g_print ("Loading %s\n", g_file_peek_path (file));

  return dex_future_new_take_object (g_object_ref (self));
}

DexFuture *
plugin_flatpak_json_manifest_new (FoundryContext *context,
                                  GFile          *file)
{
  PluginFlatpakJsonManifest *self;

  dex_return_error_if_fail (FOUNDRY_IS_CONTEXT (context));
  dex_return_error_if_fail (G_IS_FILE (file));

  self = g_object_new (PLUGIN_TYPE_FLATPAK_JSON_MANIFEST,
                       "context", context,
                       "file", file,
                       NULL);

  return dex_scheduler_spawn (NULL, 0,
                              plugin_flatpak_json_manifest_load_fiber,
                              g_steal_pointer (&self),
                              g_object_unref);
}
