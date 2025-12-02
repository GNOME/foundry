/* foundry-menu-search-provider.c
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

#include <glib/gi18n-lib.h>

#include "foundry-menu-search-provider.h"
#include "foundry-menu-search-result.h"
#include "foundry-model-manager.h"
#include "foundry-search-request.h"
#include "foundry-util.h"

typedef struct
{
  GPtrArray *menu_models;
} FoundryMenuSearchProviderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (FoundryMenuSearchProvider, foundry_menu_search_provider, FOUNDRY_TYPE_SEARCH_PROVIDER)

static char *
foundry_menu_search_provider_dup_name (FoundrySearchProvider *provider)
{
  return g_strdup (_("Menu Actions"));
}

static gboolean
matches_search_text (const char *text,
                     const char *casefold_search_text)
{
  guint priority = 0;

  if (text == NULL || casefold_search_text == NULL)
    return FALSE;

  return foundry_fuzzy_match (text, casefold_search_text, &priority);
}

static DexFuture *
foundry_menu_search_provider_search_fiber (GPtrArray  *menu_models,
                                           const char *casefold_search_text,
                                           GListStore *store)
{
  guint n_menus;

  g_assert (menu_models != NULL);
  g_assert (G_IS_LIST_STORE (store));

  if (foundry_str_empty0 (casefold_search_text))
    return dex_future_new_true ();

  n_menus = menu_models->len;

  for (guint menu_idx = 0; menu_idx < n_menus; menu_idx++)
    {
      GMenuModel *menu_model = g_ptr_array_index (menu_models, menu_idx);
      guint n_items;

      if (menu_model == NULL)
        continue;

      n_items = g_menu_model_get_n_items (menu_model);

      for (guint item_idx = 0; item_idx < n_items; item_idx++)
        {
          g_autofree char *label = NULL;
          g_autofree char *description = NULL;
          gboolean matches = FALSE;

          if (g_menu_model_get_item_attribute (menu_model, item_idx, "label", "s", &label))
            matches = matches_search_text (label, casefold_search_text);

          if (!matches && g_menu_model_get_item_attribute (menu_model, item_idx, "description", "s", &description))
            matches = matches_search_text (description, casefold_search_text);

          if (matches)
            {
              g_autoptr(FoundryMenuSearchResult) result = NULL;

              result = foundry_menu_search_result_new (menu_model, item_idx);
              g_list_store_append (store, result);
            }
        }
    }

  return dex_future_new_take_object (foundry_flatten_list_model_new (g_object_ref (G_LIST_MODEL (store))));
}

static DexFuture *
foundry_menu_search_provider_search (FoundrySearchProvider *provider,
                                     FoundrySearchRequest  *request)
{
  FoundryMenuSearchProvider *self = FOUNDRY_MENU_SEARCH_PROVIDER (provider);
  FoundryMenuSearchProviderPrivate *priv = foundry_menu_search_provider_get_instance_private (self);
  g_autoptr(GPtrArray) menu_models_copy = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autofree char *casefold_search_text = NULL;
  g_autofree char *search_text = NULL;

  g_assert (FOUNDRY_IS_MENU_SEARCH_PROVIDER (provider));
  g_assert (FOUNDRY_IS_SEARCH_REQUEST (request));

  search_text = foundry_search_request_dup_search_text (request);

  if (priv->menu_models == NULL ||
      priv->menu_models->len == 0 ||
      foundry_str_empty0 (search_text))
    return foundry_future_new_not_supported ();

  casefold_search_text = g_utf8_casefold (search_text, -1);

  /* Create a copy of the menu models array to pass to the fiber */
  menu_models_copy = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < priv->menu_models->len; i++)
    {
      GMenuModel *menu_model = g_ptr_array_index (priv->menu_models, i);

      g_ptr_array_add (menu_models_copy, g_object_ref (menu_model));
    }

  store = g_list_store_new (FOUNDRY_TYPE_MENU_SEARCH_RESULT);

  return foundry_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                                  G_CALLBACK (foundry_menu_search_provider_search_fiber),
                                  3,
                                  G_TYPE_PTR_ARRAY, menu_models_copy,
                                  G_TYPE_STRING, casefold_search_text,
                                  G_TYPE_LIST_STORE, store);
}

static void
foundry_menu_search_provider_finalize (GObject *object)
{
  FoundryMenuSearchProvider *self = FOUNDRY_MENU_SEARCH_PROVIDER (object);
  FoundryMenuSearchProviderPrivate *priv = foundry_menu_search_provider_get_instance_private (self);

  g_clear_pointer (&priv->menu_models, g_ptr_array_unref);

  G_OBJECT_CLASS (foundry_menu_search_provider_parent_class)->finalize (object);
}

static void
foundry_menu_search_provider_class_init (FoundryMenuSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundrySearchProviderClass *search_provider_class = FOUNDRY_SEARCH_PROVIDER_CLASS (klass);

  object_class->finalize = foundry_menu_search_provider_finalize;

  search_provider_class->dup_name = foundry_menu_search_provider_dup_name;
  search_provider_class->search = foundry_menu_search_provider_search;
}

static void
foundry_menu_search_provider_init (FoundryMenuSearchProvider *self)
{
  FoundryMenuSearchProviderPrivate *priv = foundry_menu_search_provider_get_instance_private (self);

  priv->menu_models = g_ptr_array_new_with_free_func (g_object_unref);
}

FoundryMenuSearchProvider *
foundry_menu_search_provider_new (void)
{
  return g_object_new (FOUNDRY_TYPE_MENU_SEARCH_PROVIDER, NULL);
}

void
foundry_menu_search_provider_add_menu (FoundryMenuSearchProvider *self,
                                       GMenuModel                *menu_model)
{
  FoundryMenuSearchProviderPrivate *priv = foundry_menu_search_provider_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_MENU_SEARCH_PROVIDER (self));
  g_return_if_fail (G_IS_MENU_MODEL (menu_model));

  g_ptr_array_add (priv->menu_models, g_object_ref (menu_model));
}
