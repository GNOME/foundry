/* plugin-doc-search-provider.c
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

#include "plugin-doc-search-provider.h"
#include "plugin-doc-search-result.h"

struct _PluginDocSearchProvider
{
  FoundrySearchProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginDocSearchProvider, plugin_doc_search_provider, FOUNDRY_TYPE_SEARCH_PROVIDER)

static gpointer
plugin_doc_search_provider_map_func (gpointer item,
                                     gpointer user_data)
{
  return plugin_doc_search_result_new (FOUNDRY_DOCUMENTATION (item));
}

static DexFuture *
plugin_doc_search_provider_map_results (DexFuture *completed,
                                        gpointer   user_data)
{
  g_autoptr(GListModel) mapped = NULL;
  g_autoptr(GListModel) model = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (completed));

  if (!(model = dex_await_object (dex_ref (completed), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  mapped = foundry_map_list_model_new (g_object_ref (model),
                                       plugin_doc_search_provider_map_func,
                                       NULL, NULL);

  return dex_future_new_take_object (g_object_ref (G_LIST_MODEL (mapped)));
}

static DexFuture *
plugin_doc_search_provider_search (FoundrySearchProvider *provider,
                                   FoundrySearchRequest  *request)
{
  g_autoptr(FoundryDocumentationManager) doc_manager = NULL;
  g_autoptr(FoundryDocumentationQuery) doc_query = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *search_text = NULL;

  g_assert (PLUGIN_IS_DOC_SEARCH_PROVIDER (provider));
  g_assert (FOUNDRY_IS_SEARCH_REQUEST (request));

  if (!foundry_search_request_has_category (request, FOUNDRY_SEARCH_CATEGORY_DOCUMENTATION))
    return foundry_future_new_not_supported ();

  if (!(search_text = foundry_search_request_dup_search_text (request)))
    return foundry_future_new_not_supported ();

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (provider), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  doc_manager = foundry_context_dup_documentation_manager (context);
  doc_query = foundry_documentation_query_new ();

  foundry_documentation_query_set_keyword (doc_query, search_text);

  return dex_future_then (foundry_documentation_manager_query (doc_manager, doc_query),
                          plugin_doc_search_provider_map_results,
                          NULL, NULL);
}

static void
plugin_doc_search_provider_class_init (PluginDocSearchProviderClass *klass)
{
  FoundrySearchProviderClass *search_provider_class = FOUNDRY_SEARCH_PROVIDER_CLASS (klass);

  search_provider_class->search = plugin_doc_search_provider_search;
}

static void
plugin_doc_search_provider_init (PluginDocSearchProvider *self)
{
}
