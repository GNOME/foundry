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

struct _PluginGitlabIssue
{
  GObject parent_instance;
  GWeakRef forge_wr;
  JsonNode *node;
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
plugin_gitlab_issue_dup_state  (FoundryForgeIssue *issue)
{
  PluginGitlabIssue *self = PLUGIN_GITLAB_ISSUE (issue);
  const char *state = NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "state", FOUNDRY_JSON_NODE_GET_STRING (&state)))
    return g_strdup (state);

  return NULL;
}

static char *
plugin_gitlab_issue_dup_title  (FoundryForgeIssue *issue)
{
  PluginGitlabIssue *self = PLUGIN_GITLAB_ISSUE (issue);
  const char *title = NULL;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "title", FOUNDRY_JSON_NODE_GET_STRING (&title)))
    return g_strdup (title);

  return NULL;
}

static void
plugin_gitlab_issue_finalize (GObject *object)
{
  PluginGitlabIssue *self = (PluginGitlabIssue *)object;

  g_weak_ref_clear (&self->forge_wr);
  g_clear_pointer (&self->node, json_node_unref);

  G_OBJECT_CLASS (plugin_gitlab_issue_parent_class)->finalize (object);
}

static void
plugin_gitlab_issue_class_init (PluginGitlabIssueClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryForgeIssueClass *forge_issue_class = FOUNDRY_FORGE_ISSUE_CLASS (klass);

  object_class->finalize = plugin_gitlab_issue_finalize;

  forge_issue_class->dup_id = plugin_gitlab_issue_dup_id;
  forge_issue_class->dup_state = plugin_gitlab_issue_dup_state;
  forge_issue_class->dup_title = plugin_gitlab_issue_dup_title;
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
