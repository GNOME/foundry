/* plugin-git-vcs-file.c
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

#include "plugin-git-vcs-file.h"

struct _PluginGitVcsFile
{
  FoundryVcsFile parent_instance;
  GFile *workdir;
  char *relative_path;
};

G_DEFINE_FINAL_TYPE (PluginGitVcsFile, plugin_git_vcs_file, FOUNDRY_TYPE_VCS_FILE)

static GFile *
plugin_git_vcs_file_dup_file (FoundryVcsFile *file)
{
  PluginGitVcsFile *self = PLUGIN_GIT_VCS_FILE (file);

  return g_file_get_child (self->workdir, self->relative_path);
}

static char *
plugin_git_vcs_file_dup_relative_path (FoundryVcsFile *file)
{
  PluginGitVcsFile *self = PLUGIN_GIT_VCS_FILE (file);

  return g_strdup (self->relative_path);
}

static void
plugin_git_vcs_file_finalize (GObject *object)
{
  PluginGitVcsFile *self = (PluginGitVcsFile *)object;

  g_clear_object (&self->workdir);
  g_clear_pointer (&self->relative_path, g_free);

  G_OBJECT_CLASS (plugin_git_vcs_file_parent_class)->finalize (object);
}

static void
plugin_git_vcs_file_class_init (PluginGitVcsFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsFileClass *vcs_file_class = FOUNDRY_VCS_FILE_CLASS (klass);

  object_class->finalize = plugin_git_vcs_file_finalize;

  vcs_file_class->dup_file = plugin_git_vcs_file_dup_file;
  vcs_file_class->dup_relative_path = plugin_git_vcs_file_dup_relative_path;
}

static void
plugin_git_vcs_file_init (PluginGitVcsFile *self)
{
}

FoundryVcsFile *
plugin_git_vcs_file_new (GFile      *workdir,
                         const char *relative_path)
{
  PluginGitVcsFile *self;

  g_assert (G_IS_FILE (workdir));
  g_assert (relative_path != NULL);

  self = g_object_new (PLUGIN_TYPE_GIT_VCS_FILE, NULL);
  self->workdir = g_object_ref (workdir);
  self->relative_path = g_strdup (relative_path);

  return FOUNDRY_VCS_FILE (self);
}
