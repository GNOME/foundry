/* plugin-git-vcs.c
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

#include "plugin-git-vcs.h"

struct _PluginGitVcs
{
  FoundryVcs      parent_instance;
  git_repository *repository;
};

G_DEFINE_FINAL_TYPE (PluginGitVcs, plugin_git_vcs, FOUNDRY_TYPE_VCS)

static char *
plugin_git_vcs_dup_id (FoundryVcs *vcs)
{
  return g_strdup ("git");
}

static char *
plugin_git_vcs_dup_name (FoundryVcs *vcs)
{
  return g_strdup ("Git");
}

static void
plugin_git_vcs_finalize (GObject *object)
{
  PluginGitVcs *self = (PluginGitVcs *)object;

  g_clear_pointer (&self->repository, git_repository_free);

  G_OBJECT_CLASS (plugin_git_vcs_parent_class)->finalize (object);
}

static void
plugin_git_vcs_class_init (PluginGitVcsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsClass *vcs_class = FOUNDRY_VCS_CLASS (klass);

  object_class->finalize = plugin_git_vcs_finalize;

  vcs_class->dup_id = plugin_git_vcs_dup_id;
  vcs_class->dup_name = plugin_git_vcs_dup_name;
}

static void
plugin_git_vcs_init (PluginGitVcs *self)
{
}

PluginGitVcs *
plugin_git_vcs_new (FoundryContext *context,
                    git_repository *repository)
{
  PluginGitVcs *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (repository != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_GIT_VCS,
                       "context", context,
                       NULL);
  self->repository = g_steal_pointer (&repository);

  return self;
}
