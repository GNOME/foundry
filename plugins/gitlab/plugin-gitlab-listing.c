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
  GWeakRef forge_wr;
  char *path;
  char **params;
};

G_DEFINE_FINAL_TYPE (PluginGitlabListing, plugin_gitlab_listing, FOUNDRY_TYPE_FORGE_LISTING)

static void
plugin_gitlab_listing_finalize (GObject *object)
{
  PluginGitlabListing *self = (PluginGitlabListing *)object;

  g_weak_ref_clear (&self->forge_wr);
  g_clear_pointer (&self->path, g_free);
  g_clear_pointer (&self->params, g_strfreev);

  G_OBJECT_CLASS (plugin_gitlab_listing_parent_class)->finalize (object);
}

static void
plugin_gitlab_listing_class_init (PluginGitlabListingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_gitlab_listing_finalize;
}

static void
plugin_gitlab_listing_init (PluginGitlabListing *self)
{
  g_weak_ref_init (&self->forge_wr, NULL);
}

static DexFuture *
plugin_gitlab_listing_new_fiber (PluginGitlabForge  *forge,
                                 const char         *method,
                                 const char         *path,
                                 const char * const *params)
{
  g_autoptr(SoupMessage) message = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (PLUGIN_IS_GITLAB_FORGE (forge));
  g_assert (method != NULL);
  g_assert (path != NULL);

  if (!(message = dex_await_object (plugin_gitlab_forge_create_message (forge, method, path, params, NULL), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(node = dex_await_boxed (plugin_gitlab_forge_send_message_and_read_json (forge, message), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  {
    g_autoptr(JsonGenerator) g = json_generator_new ();
    json_generator_set_root (g, node);
    g_autofree char *str = json_generator_to_data (g, NULL);
    g_print ("JSON: %s\n", str);
  }

  return NULL;
}

DexFuture *
plugin_gitlab_listing_new (PluginGitlabForge  *forge,
                           const char         *method,
                           const char         *path,
                           const char * const *params)
{
  dex_return_error_if_fail (PLUGIN_IS_GITLAB_FORGE (forge));
  dex_return_error_if_fail (path != NULL);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_gitlab_listing_new_fiber),
                                  4,
                                  PLUGIN_TYPE_GITLAB_FORGE, forge,
                                  G_TYPE_STRING, method,
                                  G_TYPE_STRING, path,
                                  G_TYPE_STRV, params);
}
