/* plugin-flatpak-modules.c
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

#include "plugin-flatpak-module.h"
#include "plugin-flatpak-modules.h"

struct _PluginFlatpakModules
{
  PluginFlatpakList parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginFlatpakModules, plugin_flatpak_modules, PLUGIN_TYPE_FLATPAK_LIST)

static gboolean
plugin_flatpak_modules_deserialize (PluginFlatpakList *self,
                                    JsonNode          *node)
{
  if (JSON_NODE_HOLDS_VALUE (node))
    {
      const char *path = json_node_get_string (node);
      g_autoptr(PluginFlatpakModule) module = NULL;

      /* TODO: Parse path */

      return TRUE;
    }

  if (JSON_NODE_HOLDS_OBJECT (node))
    {
      g_autoptr(GObject) module = NULL;

      if (!(module = json_gobject_deserialize (PLUGIN_TYPE_FLATPAK_MODULE, node)))
        return FALSE;

      plugin_flatpak_list_add (PLUGIN_FLATPAK_LIST (self), module);

      return TRUE;
    }

  return FALSE;
}

static void
plugin_flatpak_modules_class_init (PluginFlatpakModulesClass *klass)
{
  PluginFlatpakListClass *list_class = PLUGIN_FLATPAK_LIST_CLASS (klass);

  list_class->item_type = PLUGIN_TYPE_FLATPAK_MODULE;
  list_class->deserialize = plugin_flatpak_modules_deserialize;
}

static void
plugin_flatpak_modules_init (PluginFlatpakModules *self)
{
}
