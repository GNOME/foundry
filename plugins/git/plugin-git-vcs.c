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

#include "plugin-git-autocleanups.h"
#include "plugin-git-file-list.h"
#include "plugin-git-error.h"
#include "plugin-git-vcs.h"
#include "plugin-git-vcs-blame.h"

struct _PluginGitVcs
{
  FoundryVcs      parent_instance;
  git_repository *repository;
  char           *branch_name;
  GFile          *workdir;
};

G_DEFINE_FINAL_TYPE (PluginGitVcs, plugin_git_vcs, FOUNDRY_TYPE_VCS)

static guint
plugin_git_vcs_get_priority (FoundryVcs *vcs)
{
  return 100;
}

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

static char *
plugin_git_vcs_dup_branch_name (FoundryVcs *vcs)
{
  PluginGitVcs *self = PLUGIN_GIT_VCS (vcs);

  return g_strdup (self->branch_name);
}

static gboolean
plugin_git_vcs_is_ignored (FoundryVcs *vcs,
                           const char *relative_path)
{
  PluginGitVcs *self = (PluginGitVcs *)vcs;
  gboolean ignored = FALSE;

  g_assert (PLUGIN_IS_GIT_VCS (vcs));
  g_assert (relative_path != NULL);

  if (git_ignore_path_is_ignored (&ignored, self->repository, relative_path) == GIT_OK)
    return ignored;

  return FALSE;
}

static gboolean
plugin_git_vcs_is_file_ignored (FoundryVcs *vcs,
                                GFile      *file)
{
  PluginGitVcs *self = (PluginGitVcs *)vcs;
  g_autofree char *relative_path = NULL;
  gboolean ignored = FALSE;

  g_assert (PLUGIN_IS_GIT_VCS (vcs));
  g_assert (G_IS_FILE (file));

  if (self->workdir == NULL)
    return FALSE;

  if (!g_file_has_prefix (file, self->workdir))
    return FALSE;

  relative_path = g_file_get_relative_path (self->workdir, file);

  if (git_ignore_path_is_ignored (&ignored, self->repository, relative_path) == GIT_OK)
    return ignored;

  return FALSE;
}

static DexFuture *
plugin_git_vcs_list_files (FoundryVcs *vcs)
{
  PluginGitVcs *self = (PluginGitVcs *)vcs;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(git_index) index = NULL;

  g_assert (PLUGIN_IS_GIT_VCS (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  if (git_repository_index (&index, self->repository) == 0)
    return dex_future_new_take_object (plugin_git_file_list_new (context,
                                                                 self->workdir,
                                                                 g_steal_pointer (&index)));

  return foundry_future_new_disposed ();
}

static DexFuture *
plugin_git_vcs_blame (FoundryVcs     *vcs,
                      FoundryVcsFile *file,
                      GBytes         *bytes)
{
  PluginGitVcs *self = (PluginGitVcs *)vcs;
  g_autofree char *relative_path = NULL;
  g_autoptr(git_blame) blame = NULL;
  g_autoptr(git_blame) bytes_blame = NULL;

  dex_return_error_if_fail (PLUGIN_IS_GIT_VCS (self));
  dex_return_error_if_fail (FOUNDRY_IS_VCS_FILE (file));

  relative_path = foundry_vcs_file_dup_relative_path (file);

  if (git_blame_file (&blame, self->repository, relative_path, NULL) != 0)
    goto reject;

  if (bytes != NULL)
    {
      gconstpointer data = g_bytes_get_data (bytes, NULL);
      gsize size = g_bytes_get_size (bytes);

      if (git_blame_buffer (&bytes_blame, blame, data, size) != 0)
        goto reject;
    }

  return dex_future_new_take_object (plugin_git_vcs_blame_new (file,
                                                               g_steal_pointer (&blame),
                                                               g_steal_pointer (&bytes_blame)));

reject:
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not supported");
}

static void
plugin_git_vcs_finalize (GObject *object)
{
  PluginGitVcs *self = (PluginGitVcs *)object;

  g_clear_pointer (&self->repository, git_repository_free);
  g_clear_pointer (&self->branch_name, g_free);
  g_clear_object (&self->workdir);

  G_OBJECT_CLASS (plugin_git_vcs_parent_class)->finalize (object);
}

static void
plugin_git_vcs_class_init (PluginGitVcsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsClass *vcs_class = FOUNDRY_VCS_CLASS (klass);

  object_class->finalize = plugin_git_vcs_finalize;

  vcs_class->dup_branch_name = plugin_git_vcs_dup_branch_name;
  vcs_class->dup_id = plugin_git_vcs_dup_id;
  vcs_class->dup_name = plugin_git_vcs_dup_name;
  vcs_class->get_priority = plugin_git_vcs_get_priority;
  vcs_class->is_ignored = plugin_git_vcs_is_ignored;
  vcs_class->is_file_ignored = plugin_git_vcs_is_file_ignored;
  vcs_class->list_files = plugin_git_vcs_list_files;
  vcs_class->blame = plugin_git_vcs_blame;
}

static void
plugin_git_vcs_init (PluginGitVcs *self)
{
}

static DexFuture *
_plugin_git_vcs_load_fiber (gpointer data)
{
  PluginGitVcs *self = data;
  g_autoptr(git_reference) head = NULL;

  g_assert (PLUGIN_IS_GIT_VCS (self));

  if (git_repository_head (&head, self->repository) == GIT_OK)
    {
      const char *branch_name = NULL;

      if (git_branch_name (&branch_name, head) == GIT_OK)
        g_set_str (&self->branch_name, branch_name);
    }

  return dex_future_new_take_object (g_object_ref (self));
}

static DexFuture *
_plugin_git_vcs_load (PluginGitVcs *self)
{
  g_assert (PLUGIN_IS_GIT_VCS (self));

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              _plugin_git_vcs_load_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

DexFuture *
plugin_git_vcs_new (FoundryContext *context,
                    git_repository *repository)
{
  PluginGitVcs *self;
  const char *workdir;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (repository != NULL, NULL);

  workdir = git_repository_workdir (repository);

  self = g_object_new (PLUGIN_TYPE_GIT_VCS,
                       "context", context,
                       NULL);

  self->repository = g_steal_pointer (&repository);
  self->workdir = g_file_new_for_path (workdir);

  return _plugin_git_vcs_load (self);
}
