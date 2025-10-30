/* plugin-forgejo-user.c
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

#include "plugin-forgejo-user.h"

struct _PluginForgejoUser
{
  GObject parent_instance;
  GWeakRef forge_wr;
  JsonNode *node;
};

G_DEFINE_FINAL_TYPE (PluginForgejoUser, plugin_forgejo_user, FOUNDRY_TYPE_FORGE_USER)

static char *
get_string (PluginForgejoUser *self,
            const char       *key)
{
  const char *value = NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, key, FOUNDRY_JSON_NODE_GET_STRING (&value)))
    return g_strdup (value);

  return NULL;
}

static char *
plugin_forgejo_user_dup_handle (FoundryForgeUser *user)
{
  return get_string (PLUGIN_FORGEJO_USER (user), "username");
}

static char *
plugin_forgejo_user_dup_name (FoundryForgeUser *user)
{
  return get_string (PLUGIN_FORGEJO_USER (user), "full_name");
}

static char *
plugin_forgejo_user_dup_avatar_url (FoundryForgeUser *user)
{
  return get_string (PLUGIN_FORGEJO_USER (user), "avatar_url");
}

static char *
plugin_forgejo_user_dup_online_url (FoundryForgeUser *user)
{
  return get_string (PLUGIN_FORGEJO_USER (user), "html_url");
}

static char *
plugin_forgejo_user_dup_bio (FoundryForgeUser *user)
{
  return get_string (PLUGIN_FORGEJO_USER (user), "description");
}

static char *
plugin_forgejo_user_dup_location (FoundryForgeUser *user)
{
  return get_string (PLUGIN_FORGEJO_USER (user), "location");
}

static void
plugin_forgejo_user_finalize (GObject *object)
{
  PluginForgejoUser *self = (PluginForgejoUser *)object;

  g_weak_ref_clear (&self->forge_wr);
  g_clear_pointer (&self->node, json_node_unref);

  G_OBJECT_CLASS (plugin_forgejo_user_parent_class)->finalize (object);
}

static void
plugin_forgejo_user_class_init (PluginForgejoUserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryForgeUserClass *forge_user_class = FOUNDRY_FORGE_USER_CLASS (klass);

  object_class->finalize = plugin_forgejo_user_finalize;

  forge_user_class->dup_handle = plugin_forgejo_user_dup_handle;
  forge_user_class->dup_name = plugin_forgejo_user_dup_name;
  forge_user_class->dup_online_url = plugin_forgejo_user_dup_online_url;
  forge_user_class->dup_bio = plugin_forgejo_user_dup_bio;
  forge_user_class->dup_location = plugin_forgejo_user_dup_location;
  forge_user_class->dup_avatar_url = plugin_forgejo_user_dup_avatar_url;
}

static void
plugin_forgejo_user_init (PluginForgejoUser *self)
{
  g_weak_ref_init (&self->forge_wr, NULL);
}

FoundryForgeUser *
plugin_forgejo_user_new (PluginForgejoForge *forge,
                         JsonNode          *node)
{
  PluginForgejoUser *self;

  g_return_val_if_fail (PLUGIN_IS_FORGEJO_FORGE (forge), NULL);
  g_return_val_if_fail (node != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_FORGEJO_USER, NULL);
  g_weak_ref_set (&self->forge_wr, forge);
  self->node = node;

  return FOUNDRY_FORGE_USER (self);
}
