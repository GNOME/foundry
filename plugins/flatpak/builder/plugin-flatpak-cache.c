/* plugin-flatpak-cache.c
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
 *    Alexander Larsson <alexl@redhat.com>
 *    Christian Hergert <chergert@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "plugin-flatpak-cache.h"

struct _PluginFlatpakCache
{
  GObject parent_instance;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PluginFlatpakCache, plugin_flatpak_cache, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
plugin_flatpak_cache_finalize (GObject *object)
{
  PluginFlatpakCache *self = (PluginFlatpakCache *)object;

  G_OBJECT_CLASS (plugin_flatpak_cache_parent_class)->finalize (object);
}

static void
plugin_flatpak_cache_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  PluginFlatpakCache *self = PLUGIN_FLATPAK_CACHE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_cache_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  PluginFlatpakCache *self = PLUGIN_FLATPAK_CACHE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_cache_class_init (PluginFlatpakCacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_flatpak_cache_finalize;
  object_class->get_property = plugin_flatpak_cache_get_property;
  object_class->set_property = plugin_flatpak_cache_set_property;
}

static void
plugin_flatpak_cache_init (PluginFlatpakCache *self)
{
}

void
plugin_flatpak_cache_checksum_str (PluginFlatpakCache *self,
                                           const char                *str)
{
  g_return_if_fail (PLUGIN_IS_FLATPAK_CACHE (self));
}

void
plugin_flatpak_cache_checksum_strv (PluginFlatpakCache *self,
                                            const char * const        *str)
{
  g_return_if_fail (PLUGIN_IS_FLATPAK_CACHE (self));
}
