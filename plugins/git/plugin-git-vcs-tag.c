/* plugin-git-vcs-tag.c
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

#include "plugin-git-vcs-tag.h"

struct _PluginGitVcsTag
{
  FoundryVcsTag parent_instance;
  git_reference *ref;
};

G_DEFINE_FINAL_TYPE (PluginGitVcsTag, plugin_git_vcs_tag, FOUNDRY_TYPE_VCS_TAG)

static char *
plugin_git_vcs_tag_dup_id (FoundryVcsObject *object)
{
  PluginGitVcsTag *self = (PluginGitVcsTag *)object;
  const git_oid *oid;

  g_assert (PLUGIN_IS_GIT_VCS_TAG (self));
  g_assert (self->ref != NULL);

  if ((oid = git_reference_target (self->ref)))
    {
      char oid_str[GIT_OID_HEXSZ + 1];

      git_oid_tostr (oid_str, sizeof oid_str, oid);
      oid_str[GIT_OID_HEXSZ] = 0;

      return g_strdup (oid_str);
    }

  return NULL;
}

static char *
plugin_git_vcs_tag_dup_name (FoundryVcsObject *object)
{
  PluginGitVcsTag *self = (PluginGitVcsTag *)object;
  const char *name;

  g_assert (PLUGIN_IS_GIT_VCS_TAG (self));
  g_assert (self->ref != NULL);

  if ((name = git_reference_name (self->ref)))
    {
      const char *suffix = strrchr (name, '/');

      if (suffix != NULL)
        {
          suffix++;
          name = suffix;
        }
    }

  return g_strdup (name);
}

static gboolean
plugin_git_vcs_tag_is_local (FoundryVcsObject *object)
{
  PluginGitVcsTag *self = (PluginGitVcsTag *)object;
  const char *name;

  g_assert (PLUGIN_IS_GIT_VCS_TAG (self));
  g_assert (self->ref != NULL);

  if ((name = git_reference_name (self->ref)))
    {
      if (g_str_has_prefix (name, "refs/tags/"))
        return TRUE;
    }

  return FALSE;
}

static void
plugin_git_vcs_tag_finalize (GObject *object)
{
  PluginGitVcsTag *self = (PluginGitVcsTag *)object;

  g_clear_pointer (&self->ref, git_reference_free);

  G_OBJECT_CLASS (plugin_git_vcs_tag_parent_class)->finalize (object);
}

static void
plugin_git_vcs_tag_class_init (PluginGitVcsTagClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsObjectClass *vcs_object_class = FOUNDRY_VCS_OBJECT_CLASS (klass);

  object_class->finalize = plugin_git_vcs_tag_finalize;

  vcs_object_class->dup_id = plugin_git_vcs_tag_dup_id;
  vcs_object_class->dup_name = plugin_git_vcs_tag_dup_name;
  vcs_object_class->is_local = plugin_git_vcs_tag_is_local;
}

static void
plugin_git_vcs_tag_init (PluginGitVcsTag *self)
{
}

/**
 * plugin_git_vcs_tag_new:
 * @ref: (transfer full): the underlying reference
 */
PluginGitVcsTag *
plugin_git_vcs_tag_new (git_reference *ref)
{
  PluginGitVcsTag *self;

  g_return_val_if_fail (ref != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_GIT_VCS_TAG, NULL);
  self->ref = g_steal_pointer (&ref);

  return self;
}

