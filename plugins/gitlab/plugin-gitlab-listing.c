/* plugin-gitlab-listing.c
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

#include "plugin-gitlab-listing.h"

struct _PluginGitlabListing
{
  FoundryForgeListing parent_instance;
  PluginGitlabInflate inflate;
  GPtrArray *pages;
  GWeakRef forge_wr;
  char *method;
  char *path;
  char **params;
  char *next_page;
  guint n_pages;
};

G_DEFINE_FINAL_TYPE (PluginGitlabListing, plugin_gitlab_listing, FOUNDRY_TYPE_FORGE_LISTING)

static DexFuture *
plugin_gitlab_listing_load_fiber (PluginGitlabListing *self,
                                  PluginGitlabInflate  inflate,
                                  const char          *method,
                                  const char          *path,
                                  const char * const  *params,
                                  const char          *next_page)
{
  g_autoptr(PluginGitlabForge) forge = NULL;
  g_autoptr(SoupMessage) message = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GError) error = NULL;
  SoupMessageHeaders *headers;
  const char *x_next_page;
  JsonArray *array;
  guint length;

  g_assert (PLUGIN_IS_GITLAB_LISTING (self));
  g_assert (next_page != NULL);

  if (!(forge = g_weak_ref_get (&self->forge_wr)))
    return foundry_future_new_disposed ();

  g_assert (PLUGIN_IS_GITLAB_FORGE (forge));

  if (!(message = dex_await_object (plugin_gitlab_forge_create_message (forge, method, path, params,
                                                                        "page", next_page,
                                                                        NULL),
                                    &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(node = dex_await_boxed (plugin_gitlab_forge_send_message_and_read_json (forge, message), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!JSON_NODE_HOLDS_ARRAY (node))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "Unexpected JSON reply");

  headers = soup_message_get_response_headers (message);
  x_next_page = soup_message_headers_get_one (headers, "x-next-page");

  g_debug ("Gitlab listing x-next-page: %s", x_next_page);

  store = g_list_store_new (G_TYPE_OBJECT);
  array = json_node_get_array (node);
  length = json_array_get_length (array);

  for (guint i = 0; i < length; i++)
    {
      JsonNode *element = json_array_get_element (array, i);
      g_autoptr(GObject) object = inflate (forge, json_node_ref (element));

      if (object != NULL)
        g_list_store_append (store, object);
    }

  if (!foundry_str_empty0 (x_next_page) &&
      g_set_str (&self->next_page, x_next_page))
    g_object_notify (G_OBJECT (self), "n-pages");

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static guint
plugin_gitlab_listing_get_n_pages (FoundryForgeListing *listing)
{
  PluginGitlabListing *self = PLUGIN_GITLAB_LISTING (listing);

  if (self->next_page != NULL)
    return self->pages->len + 1;

  return self->pages->len;
}

static DexFuture *
plugin_gitlab_listing_load_page (FoundryForgeListing *listing,
                                 guint                page)
{
  PluginGitlabListing *self = PLUGIN_GITLAB_LISTING (listing);

  if (page < self->pages->len)
    return dex_ref (g_ptr_array_index (self->pages, page));

  if (page == self->pages->len && self->next_page != NULL)
    {
      g_autofree char *next_page = g_steal_pointer (&self->next_page);
      DexFuture *future;

      future = foundry_scheduler_spawn (NULL, 0,
                                        G_CALLBACK (plugin_gitlab_listing_load_fiber),
                                        6,
                                        PLUGIN_TYPE_GITLAB_LISTING, self,
                                        G_TYPE_POINTER, self->inflate,
                                        G_TYPE_STRING, self->method,
                                        G_TYPE_STRING, self->path,
                                        G_TYPE_STRV, self->params,
                                        G_TYPE_STRING, next_page);

      g_ptr_array_add (self->pages, dex_ref (future));

      return g_steal_pointer (&future);
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_FOUND,
                                "Failed to locate page `%u`", page);
}

static void
plugin_gitlab_listing_finalize (GObject *object)
{
  PluginGitlabListing *self = (PluginGitlabListing *)object;

  g_weak_ref_clear (&self->forge_wr);
  g_clear_pointer (&self->method, g_free);
  g_clear_pointer (&self->path, g_free);
  g_clear_pointer (&self->next_page, g_free);
  g_clear_pointer (&self->params, g_strfreev);
  g_clear_pointer (&self->pages, g_ptr_array_unref);

  G_OBJECT_CLASS (plugin_gitlab_listing_parent_class)->finalize (object);
}

static void
plugin_gitlab_listing_class_init (PluginGitlabListingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryForgeListingClass *forge_listing_class = FOUNDRY_FORGE_LISTING_CLASS (klass);

  object_class->finalize = plugin_gitlab_listing_finalize;

  forge_listing_class->get_n_pages = plugin_gitlab_listing_get_n_pages;
  forge_listing_class->load_page = plugin_gitlab_listing_load_page;
}

static void
plugin_gitlab_listing_init (PluginGitlabListing *self)
{
  g_weak_ref_init (&self->forge_wr, NULL);
  self->pages = g_ptr_array_new_with_free_func (dex_unref);
}

static DexFuture *
plugin_gitlab_listing_new_fiber (PluginGitlabForge   *forge,
                                 PluginGitlabInflate  inflate,
                                 const char          *method,
                                 const char          *path,
                                 const char * const  *params)
{
  g_autoptr(PluginGitlabListing) self = NULL;
  g_autoptr(SoupMessage) message = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GError) error = NULL;
  SoupMessageHeaders *headers;
  const char *x_total;
  const char *x_next_page;
  JsonArray *array;
  guint length;

  g_assert (PLUGIN_IS_GITLAB_FORGE (forge));
  g_assert (method != NULL);
  g_assert (path != NULL);

  if (!(message = dex_await_object (plugin_gitlab_forge_create_message (forge, method, path, params, NULL), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(node = dex_await_boxed (plugin_gitlab_forge_send_message_and_read_json (forge, message), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!JSON_NODE_HOLDS_ARRAY (node))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "Unexpected JSON reply");

  headers = soup_message_get_response_headers (message);
  x_total = soup_message_headers_get_one (headers, "x-total");
  x_next_page = soup_message_headers_get_one (headers, "x-next-page");

  g_debug ("Gitlab listing x-total: %s", x_total);
  g_debug ("Gitlab listing x-next-page: %s", x_next_page);

  store = g_list_store_new (G_TYPE_OBJECT);
  array = json_node_get_array (node);
  length = json_array_get_length (array);

  for (guint i = 0; i < length; i++)
    {
      JsonNode *element = json_array_get_element (array, i);
      g_autoptr(GObject) object = inflate (forge, json_node_ref (element));

      if (object != NULL)
        g_list_store_append (store, object);
    }

  self = g_object_new (PLUGIN_TYPE_GITLAB_LISTING, NULL);
  self->inflate = inflate;
  self->method = g_strdup (method);
  self->path = g_strdup (path);
  self->params = g_strdupv ((char **)params);
  g_weak_ref_set (&self->forge_wr, forge);

  if (!foundry_str_empty0 (x_next_page))
    self->next_page = g_strdup (x_next_page);

  g_ptr_array_add (self->pages, dex_future_new_take_object (g_object_ref (store)));

  return dex_future_new_take_object (g_steal_pointer (&self));
}

DexFuture *
plugin_gitlab_listing_new (PluginGitlabForge   *forge,
                           PluginGitlabInflate  inflate,
                           const char          *method,
                           const char          *path,
                           const char * const  *params)
{
  dex_return_error_if_fail (PLUGIN_IS_GITLAB_FORGE (forge));
  dex_return_error_if_fail (inflate != NULL);
  dex_return_error_if_fail (path != NULL);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_gitlab_listing_new_fiber),
                                  5,
                                  PLUGIN_TYPE_GITLAB_FORGE, forge,
                                  G_TYPE_POINTER, inflate,
                                  G_TYPE_STRING, method,
                                  G_TYPE_STRING, path,
                                  G_TYPE_STRV, params);
}
