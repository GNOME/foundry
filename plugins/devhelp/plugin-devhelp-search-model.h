/* plugin-devhelp-search-model.h
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

#pragma once

#include <gio/gio.h>
#include <gom/gom.h>
#include <libdex.h>

G_BEGIN_DECLS

#define PLUGIN_TYPE_DEVHELP_SEARCH_MODEL (plugin_devhelp_search_model_get_type())

G_DECLARE_FINAL_TYPE (PluginDevhelpSearchModel, plugin_devhelp_search_model, PLUGIN, DEVHELP_SEARCH_MODEL, GObject)

PluginDevhelpSearchModel *plugin_devhelp_search_model_new      (GomResourceGroup         *group,
                                                                gboolean                  had_prefetch);
DexFuture                *plugin_devhelp_search_model_prefetch (PluginDevhelpSearchModel *self,
                                                                guint                     position);

G_END_DECLS
