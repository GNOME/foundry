/* plugin-flatpak-sources.c
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

#include "plugin-flatpak-list-private.h"
#include "plugin-flatpak-source.h"
#include "plugin-flatpak-source-archive.h"
#include "plugin-flatpak-source-bzr.h"
#include "plugin-flatpak-source-file.h"
#include "plugin-flatpak-source-git.h"
#include "plugin-flatpak-source-patch.h"
#include "plugin-flatpak-sources.h"

struct _PluginFlatpakSources
{
  PluginFlatpakList parent_instance;
};

struct _PluginFlatpakSourcesClass
{
  PluginFlatpakListClass parent_class;
};

G_DEFINE_FINAL_TYPE (PluginFlatpakSources, plugin_flatpak_sources, PLUGIN_TYPE_FLATPAK_LIST)

static GType
plugin_flatpak_sources_get_item_type (PluginFlatpakList *self,
                                      const char        *type)
{
  g_autofree GType *children = NULL;
  guint n_children = 0;

  if ((children = g_type_children (PLUGIN_TYPE_FLATPAK_SOURCE, &n_children)))
    {
      for (guint i = 0; i < n_children; i++)
        {
          g_autoptr(PluginFlatpakSourceClass) klass = g_type_class_ref (children[i]);

          if (g_strcmp0 (type, klass->type) == 0)
            return children[i];
        }
    }

  g_print ("Cannot find %s\n", type);

  return PLUGIN_FLATPAK_LIST_GET_CLASS (self)->item_type;
}

static void
plugin_flatpak_sources_class_init (PluginFlatpakSourcesClass *klass)
{
  PluginFlatpakListClass *list_class = PLUGIN_FLATPAK_LIST_CLASS (klass);

  list_class->item_type = PLUGIN_TYPE_FLATPAK_SOURCE;
  list_class->get_item_type = plugin_flatpak_sources_get_item_type;

  g_type_ensure (PLUGIN_TYPE_FLATPAK_SOURCE_ARCHIVE);
  g_type_ensure (PLUGIN_TYPE_FLATPAK_SOURCE_BZR);
  g_type_ensure (PLUGIN_TYPE_FLATPAK_SOURCE_FILE);
  g_type_ensure (PLUGIN_TYPE_FLATPAK_SOURCE_GIT);
  g_type_ensure (PLUGIN_TYPE_FLATPAK_SOURCE_PATCH);
}

static void
plugin_flatpak_sources_init (PluginFlatpakSources *self)
{
}
