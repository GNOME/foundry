/* plugin-gitlab-project.c
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
#include "plugin-gitlab-project.h"

struct _PluginGitlabProject
{
  FoundryForgeProject parent_instance;
  JsonNode *node;
  GWeakRef forge_wr;
};

G_DEFINE_FINAL_TYPE (PluginGitlabProject, plugin_gitlab_project, FOUNDRY_TYPE_FORGE_PROJECT)

static gint64
plugin_gitlab_project_get_id (PluginGitlabProject  *self,
                              GError              **error)
{
  gint64 id = 0;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "id", FOUNDRY_JSON_NODE_GET_INT (&id)) && id)
    return id;

  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Failed to locate project-id");

  return 0;
}

static gpointer
inflate_issue (PluginGitlabForge *forge,
               JsonNode          *node)
{
  g_assert (PLUGIN_IS_GITLAB_FORGE (forge));
  g_assert (node != NULL);

  /* TODO */

  return NULL;
}

static DexFuture *
plugin_gitlab_project_list_issues (FoundryForgeProject *project,
                                   FoundryForgeQuery   *query)
{
  PluginGitlabProject *self = PLUGIN_GITLAB_PROJECT (project);
  g_autoptr(PluginGitlabForge) forge = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  gint64 project_id;

  g_assert (PLUGIN_IS_GITLAB_PROJECT (self));
  g_assert (!query || FOUNDRY_IS_FORGE_QUERY (query));

  if (!(forge = g_weak_ref_get (&self->forge_wr)))
    return foundry_future_new_disposed ();

  if (!(project_id = plugin_gitlab_project_get_id (self, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  path = g_strdup_printf ("/api/v4/projects/%"G_GINT64_FORMAT"/issues", project_id);

  return plugin_gitlab_listing_new (forge, inflate_issue, SOUP_METHOD_GET, path, NULL);
}


static void
plugin_gitlab_project_finalize (GObject *object)
{
  PluginGitlabProject *self = (PluginGitlabProject *)object;

  g_clear_pointer (&self->node, json_node_unref);
  g_weak_ref_clear (&self->forge_wr);

  G_OBJECT_CLASS (plugin_gitlab_project_parent_class)->finalize (object);
}

static void
plugin_gitlab_project_class_init (PluginGitlabProjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryForgeProjectClass *forge_project_class = FOUNDRY_FORGE_PROJECT_CLASS (klass);

  object_class->finalize = plugin_gitlab_project_finalize;

  forge_project_class->list_issues = plugin_gitlab_project_list_issues;
}

static void
plugin_gitlab_project_init (PluginGitlabProject *self)
{
  g_weak_ref_init (&self->forge_wr, NULL);
}

FoundryForgeProject *
plugin_gitlab_project_new (PluginGitlabForge *forge,
                           JsonNode          *node)
{
  PluginGitlabProject *self;

  g_return_val_if_fail (PLUGIN_IS_GITLAB_FORGE (forge), NULL);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (node), NULL);

  self = g_object_new (PLUGIN_TYPE_GITLAB_PROJECT, NULL);
  g_weak_ref_set (&self->forge_wr, forge);
  self->node = node;

  return FOUNDRY_FORGE_PROJECT (self);
}
