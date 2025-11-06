/* plugin-gitlab-issue.c
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

#include "plugin-gitlab-issue.h"
#include "plugin-gitlab-user.h"

struct _PluginGitlabIssue
{
  GObject parent_instance;
  GWeakRef forge_wr;
  JsonNode *node;
  FoundryForgeUser *author;
};

G_DEFINE_FINAL_TYPE (PluginGitlabIssue, plugin_gitlab_issue, FOUNDRY_TYPE_FORGE_ISSUE)

static char *
plugin_gitlab_issue_dup_id (FoundryForgeIssue *issue)
{
  PluginGitlabIssue *self = PLUGIN_GITLAB_ISSUE (issue);
  gint64 id = 0;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "iid", FOUNDRY_JSON_NODE_GET_INT (&id)) && id > 0)
    return g_strdup_printf ("%"G_GINT64_FORMAT, id);

  return NULL;
}

static char *
plugin_gitlab_issue_dup_online_url (FoundryForgeIssue *issue)
{
  PluginGitlabIssue *self = PLUGIN_GITLAB_ISSUE (issue);
  const char *online_url = NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "web_url", FOUNDRY_JSON_NODE_GET_STRING (&online_url)))
    return g_strdup (online_url);

  return NULL;
}

static char *
plugin_gitlab_issue_dup_state (FoundryForgeIssue *issue)
{
  PluginGitlabIssue *self = PLUGIN_GITLAB_ISSUE (issue);
  const char *state = NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "state", FOUNDRY_JSON_NODE_GET_STRING (&state)))
    return g_strdup (state);

  return NULL;
}

static char *
plugin_gitlab_issue_dup_title (FoundryForgeIssue *issue)
{
  PluginGitlabIssue *self = PLUGIN_GITLAB_ISSUE (issue);
  const char *title = NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "title", FOUNDRY_JSON_NODE_GET_STRING (&title)))
    return g_strdup (title);

  return NULL;
}

static GDateTime *
plugin_gitlab_issue_dup_created_at (FoundryForgeIssue *issue)
{
  PluginGitlabIssue *self = PLUGIN_GITLAB_ISSUE (issue);
  const char *created_at_str = NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "created_at", FOUNDRY_JSON_NODE_GET_STRING (&created_at_str)))
    return g_date_time_new_from_iso8601 (created_at_str, NULL);

  return NULL;
}

static FoundryForgeUser *
plugin_gitlab_issue_dup_user (FoundryForgeIssue *issue)
{
  PluginGitlabIssue *self = PLUGIN_GITLAB_ISSUE (issue);
  g_autoptr(PluginGitlabForge) forge = NULL;
  JsonNode *author_node = NULL;

  if (self->author != NULL)
    return g_object_ref (self->author);

  if (!(forge = g_weak_ref_get (&self->forge_wr)))
    return NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "author", FOUNDRY_JSON_NODE_GET_NODE (&author_node)))
    {
      self->author = plugin_gitlab_user_new (forge, json_node_ref (author_node));
      return g_object_ref (self->author);
    }

  return NULL;
}

static void
plugin_gitlab_issue_finalize (GObject *object)
{
  PluginGitlabIssue *self = (PluginGitlabIssue *)object;

  g_weak_ref_clear (&self->forge_wr);
  g_clear_pointer (&self->node, json_node_unref);
  g_clear_object (&self->author);

  G_OBJECT_CLASS (plugin_gitlab_issue_parent_class)->finalize (object);
}

static void
plugin_gitlab_issue_class_init (PluginGitlabIssueClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryForgeIssueClass *forge_issue_class = FOUNDRY_FORGE_ISSUE_CLASS (klass);

  object_class->finalize = plugin_gitlab_issue_finalize;

  forge_issue_class->dup_id = plugin_gitlab_issue_dup_id;
  forge_issue_class->dup_online_url = plugin_gitlab_issue_dup_online_url;
  forge_issue_class->dup_state = plugin_gitlab_issue_dup_state;
  forge_issue_class->dup_title = plugin_gitlab_issue_dup_title;
  forge_issue_class->dup_created_at = plugin_gitlab_issue_dup_created_at;
  forge_issue_class->dup_user = plugin_gitlab_issue_dup_user;
}

static void
plugin_gitlab_issue_init (PluginGitlabIssue *self)
{
  g_weak_ref_init (&self->forge_wr, NULL);
}

FoundryForgeIssue *
plugin_gitlab_issue_new (PluginGitlabForge *forge,
                         JsonNode          *node)
{
  PluginGitlabIssue *self;

  g_return_val_if_fail (PLUGIN_IS_GITLAB_FORGE (forge), NULL);
  g_return_val_if_fail (node != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_GITLAB_ISSUE, NULL);
  g_weak_ref_set (&self->forge_wr, forge);
  self->node = node;

  return FOUNDRY_FORGE_ISSUE (self);
}
