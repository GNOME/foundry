/* plugin-forgejo-project.c
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

#include "plugin-forgejo-project.h"

struct _PluginForgejoProject
{
  FoundryForgeProject parent_instance;
  JsonNode *node;
  GWeakRef forge_wr;
};

G_DEFINE_FINAL_TYPE (PluginForgejoProject, plugin_forgejo_project, FOUNDRY_TYPE_FORGE_PROJECT)

static char *
get_string (PluginForgejoProject *self,
            const char          *key)
{
  const char *value = NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, key, FOUNDRY_JSON_NODE_GET_STRING (&value)))
    return g_strdup (value);

  return NULL;
}

static char *
plugin_forgejo_project_dup_description (FoundryForgeProject *project)
{
  return get_string (PLUGIN_FORGEJO_PROJECT (project), "description");
}

static char *
plugin_forgejo_project_dup_title (FoundryForgeProject *project)
{
  return get_string (PLUGIN_FORGEJO_PROJECT (project), "name");
}

static char *
plugin_forgejo_project_dup_avatar_url (FoundryForgeProject *project)
{
  JsonNode *owner_node = NULL;
  const char *value = NULL;
  PluginForgejoProject *self = PLUGIN_FORGEJO_PROJECT (project);

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "owner", FOUNDRY_JSON_NODE_GET_NODE (&owner_node)) &&
      JSON_NODE_HOLDS_OBJECT (owner_node) &&
      FOUNDRY_JSON_OBJECT_PARSE (owner_node, "avatar_url", FOUNDRY_JSON_NODE_GET_STRING (&value)))
    return g_strdup (value);

  return NULL;
}

static char *
plugin_forgejo_project_dup_online_url (FoundryForgeProject *project)
{
  return get_string (PLUGIN_FORGEJO_PROJECT (project), "html_url");
}

static void
plugin_forgejo_project_finalize (GObject *object)
{
  PluginForgejoProject *self = (PluginForgejoProject *)object;

  g_clear_pointer (&self->node, json_node_unref);
  g_weak_ref_clear (&self->forge_wr);

  G_OBJECT_CLASS (plugin_forgejo_project_parent_class)->finalize (object);
}

static void
plugin_forgejo_project_class_init (PluginForgejoProjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryForgeProjectClass *forge_project_class = FOUNDRY_FORGE_PROJECT_CLASS (klass);

  object_class->finalize = plugin_forgejo_project_finalize;

  forge_project_class->dup_avatar_url = plugin_forgejo_project_dup_avatar_url;
  forge_project_class->dup_title = plugin_forgejo_project_dup_title;
  forge_project_class->dup_description = plugin_forgejo_project_dup_description;
  forge_project_class->dup_online_url = plugin_forgejo_project_dup_online_url;
}

static void
plugin_forgejo_project_init (PluginForgejoProject *self)
{
  g_weak_ref_init (&self->forge_wr, NULL);
}

FoundryForgeProject *
plugin_forgejo_project_new (PluginForgejoForge *forge,
                             JsonNode          *node)
{
  PluginForgejoProject *self;

  g_return_val_if_fail (PLUGIN_IS_FORGEJO_FORGE (forge), NULL);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (node), NULL);

  self = g_object_new (PLUGIN_TYPE_FORGEJO_PROJECT, NULL);
  g_weak_ref_set (&self->forge_wr, forge);
  self->node = node;

  return FOUNDRY_FORGE_PROJECT (self);
}
