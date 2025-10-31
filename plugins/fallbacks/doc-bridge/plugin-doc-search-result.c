/* plugin-doc-search-result.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "plugin-doc-search-result.h"

struct _PluginDocSearchResult
{
  FoundrySearchResult   parent_instance;
  FoundryDocumentation *documentation;
};

G_DEFINE_FINAL_TYPE (PluginDocSearchResult, plugin_doc_search_result, FOUNDRY_TYPE_SEARCH_RESULT)

static void
plugin_doc_search_result_finalize (GObject *object)
{
  PluginDocSearchResult *self = (PluginDocSearchResult *)object;

  g_clear_object (&self->documentation);

  G_OBJECT_CLASS (plugin_doc_search_result_parent_class)->finalize (object);
}

static void
plugin_doc_search_result_class_init (PluginDocSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_doc_search_result_finalize;
}

static void
plugin_doc_search_result_init (PluginDocSearchResult *self)
{
}

/**
 * plugin_doc_search_result_new:
 * @documentation: (transfer full):
 *
 * Returns: (transfer full):
 */
FoundrySearchResult *
plugin_doc_search_result_new (FoundryDocumentation *documentation)
{
  PluginDocSearchResult *self;

  g_return_val_if_fail (FOUNDRY_IS_DOCUMENTATION (documentation), NULL);


  self = g_object_new (PLUGIN_TYPE_DOC_SEARCH_RESULT, NULL);
  self->documentation = documentation;

  return FOUNDRY_SEARCH_RESULT (self);
}
