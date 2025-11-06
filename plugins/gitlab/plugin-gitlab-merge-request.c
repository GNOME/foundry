/* plugin-gitlab-merge-request.c
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

#include "plugin-gitlab-merge-request.h"

struct _PluginGitlabMergeRequest
{
  GObject parent_instance;
  GWeakRef forge_wr;
  JsonNode *node;
};

G_DEFINE_FINAL_TYPE (PluginGitlabMergeRequest, plugin_gitlab_merge_request, FOUNDRY_TYPE_FORGE_MERGE_REQUEST)

static char *
plugin_gitlab_merge_request_dup_id (FoundryForgeMergeRequest *merge_request)
{
  PluginGitlabMergeRequest *self = PLUGIN_GITLAB_MERGE_REQUEST (merge_request);
  gint64 id = 0;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "iid", FOUNDRY_JSON_NODE_GET_INT (&id)) && id > 0)
    return g_strdup_printf ("%"G_GINT64_FORMAT, id);

  return NULL;
}

static char *
plugin_gitlab_merge_request_dup_online_url (FoundryForgeMergeRequest *merge_request)
{
  PluginGitlabMergeRequest *self = PLUGIN_GITLAB_MERGE_REQUEST (merge_request);
  const char *online_url = NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "web_url", FOUNDRY_JSON_NODE_GET_STRING (&online_url)))
    return g_strdup (online_url);

  return NULL;
}

static char *
plugin_gitlab_merge_request_dup_state (FoundryForgeMergeRequest *merge_request)
{
  PluginGitlabMergeRequest *self = PLUGIN_GITLAB_MERGE_REQUEST (merge_request);
  const char *state = NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "state", FOUNDRY_JSON_NODE_GET_STRING (&state)))
    return g_strdup (state);

  return NULL;
}

static char *
plugin_gitlab_merge_request_dup_title (FoundryForgeMergeRequest *merge_request)
{
  PluginGitlabMergeRequest *self = PLUGIN_GITLAB_MERGE_REQUEST (merge_request);
  const char *title = NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "title", FOUNDRY_JSON_NODE_GET_STRING (&title)))
    return g_strdup (title);

  return NULL;
}

static GDateTime *
plugin_gitlab_merge_request_dup_created_at (FoundryForgeMergeRequest *merge_request)
{
  PluginGitlabMergeRequest *self = PLUGIN_GITLAB_MERGE_REQUEST (merge_request);
  const char *created_at_str = NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "created_at", FOUNDRY_JSON_NODE_GET_STRING (&created_at_str)))
    return g_date_time_new_from_iso8601 (created_at_str, NULL);

  return NULL;
}

static void
plugin_gitlab_merge_request_finalize (GObject *object)
{
  PluginGitlabMergeRequest *self = (PluginGitlabMergeRequest *)object;

  g_weak_ref_clear (&self->forge_wr);
  g_clear_pointer (&self->node, json_node_unref);

  G_OBJECT_CLASS (plugin_gitlab_merge_request_parent_class)->finalize (object);
}

static void
plugin_gitlab_merge_request_class_init (PluginGitlabMergeRequestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryForgeMergeRequestClass *forge_merge_request_class = FOUNDRY_FORGE_MERGE_REQUEST_CLASS (klass);

  object_class->finalize = plugin_gitlab_merge_request_finalize;

  forge_merge_request_class->dup_id = plugin_gitlab_merge_request_dup_id;
  forge_merge_request_class->dup_online_url = plugin_gitlab_merge_request_dup_online_url;
  forge_merge_request_class->dup_state = plugin_gitlab_merge_request_dup_state;
  forge_merge_request_class->dup_title = plugin_gitlab_merge_request_dup_title;
  forge_merge_request_class->dup_created_at = plugin_gitlab_merge_request_dup_created_at;
}

static void
plugin_gitlab_merge_request_init (PluginGitlabMergeRequest *self)
{
  g_weak_ref_init (&self->forge_wr, NULL);
}

FoundryForgeMergeRequest *
plugin_gitlab_merge_request_new (PluginGitlabForge *forge,
                                 JsonNode          *node)
{
  PluginGitlabMergeRequest *self;

  g_return_val_if_fail (PLUGIN_IS_GITLAB_FORGE (forge), NULL);
  g_return_val_if_fail (node != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_GITLAB_MERGE_REQUEST, NULL);
  g_weak_ref_set (&self->forge_wr, forge);
  self->node = node;

  return FOUNDRY_FORGE_MERGE_REQUEST (self);
}
