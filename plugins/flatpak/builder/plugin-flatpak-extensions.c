/* plugin-flatpak-extensions.c
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

#include "plugin-flatpak-extension.h"
#include "plugin-flatpak-extensions.h"

struct _PluginFlatpakExtensions
{
  PluginFlatpakList parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginFlatpakExtensions, plugin_flatpak_extensions, PLUGIN_TYPE_FLATPAK_LIST)

static void
plugin_flatpak_extensions_class_init (PluginFlatpakExtensionsClass *klass)
{
  PLUGIN_FLATPAK_LIST_CLASS (klass)->item_type = PLUGIN_TYPE_FLATPAK_EXTENSION;
}

static void
plugin_flatpak_extensions_init (PluginFlatpakExtensions *self)
{
}
