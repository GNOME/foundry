/* foundry-git-repository-paths.c
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

#include "foundry-git-error.h"
#include "foundry-git-repository-paths-private.h"

struct _FoundryGitRepositoryPaths
{
  char *git_dir;
  char *workdir;
};

G_DEFINE_BOXED_TYPE (FoundryGitRepositoryPaths,
                     foundry_git_repository_paths,
                     foundry_git_repository_paths_ref,
                     foundry_git_repository_paths_unref)

static void
foundry_git_repository_paths_finalize (gpointer data)
{
  FoundryGitRepositoryPaths *self = data;

  g_clear_pointer (&self->git_dir, g_free);
  g_clear_pointer (&self->workdir, g_free);
}

/**
 * foundry_git_repository_paths_new:
 * @git_dir: the git directory path
 * @workdir: the working directory path
 *
 * Creates a new #FoundryGitRepositoryPaths instance.
 *
 * Returns: (transfer full): a new #FoundryGitRepositoryPaths
 */
FoundryGitRepositoryPaths *
foundry_git_repository_paths_new (const char *git_dir,
                                  const char *workdir)
{
  FoundryGitRepositoryPaths *self;

  g_return_val_if_fail (git_dir != NULL, NULL);
  g_return_val_if_fail (workdir != NULL, NULL);

  self = g_atomic_rc_box_new0 (FoundryGitRepositoryPaths);
  self->git_dir = g_strdup (git_dir);
  self->workdir = g_strdup (workdir);

  return self;
}

/**
 * foundry_git_repository_paths_ref: (skip)
 * @self: a #FoundryGitRepositoryPaths
 *
 * Increments the reference count of @self.
 *
 * Returns: (transfer full): @self
 */
FoundryGitRepositoryPaths *
foundry_git_repository_paths_ref (FoundryGitRepositoryPaths *self)
{
  return g_atomic_rc_box_acquire (self);
}

/**
 * foundry_git_repository_paths_unref:
 * @self: a #FoundryGitRepositoryPaths
 *
 * Decrements the reference count of @self. When the reference count
 * reaches zero, the resources are freed.
 */
void
foundry_git_repository_paths_unref (FoundryGitRepositoryPaths *self)
{
  g_atomic_rc_box_release_full (self, foundry_git_repository_paths_finalize);
}

/**
 * foundry_git_repository_paths_open:
 * @self: a #FoundryGitRepositoryPaths
 * @repository_out: (out) (transfer full): location to store the opened repository
 * @error: (nullable): location to store an error, or %NULL
 *
 * Opens the git repository at the git directory path.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
foundry_git_repository_paths_open (FoundryGitRepositoryPaths  *self,
                                   git_repository            **repository_out,
                                   GError                    **error)
{
  const git_error *git_err;
  git_repository *repository = NULL;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (repository_out != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (git_repository_open (&repository, self->git_dir) != 0)
    {
      git_err = git_error_last ();
      g_set_error (error,
                   FOUNDRY_GIT_ERROR,
                   git_err->klass,
                   "%s", git_err->message);
      return FALSE;
    }

  *repository_out = repository;

  return TRUE;
}

/**
 * foundry_repository_paths_get_workdir_file:
 * @self: a #FoundryGitRepositoryPaths
 * @path: the relative path within the working directory
 *
 * Gets a #GFile for a child path within the working directory.
 *
 * Returns: (transfer full): a new #GFile
 */
GFile *
foundry_repository_paths_get_workdir_file (FoundryGitRepositoryPaths *self,
                                           const char                *path)
{
  g_autoptr(GFile) workdir_file = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  workdir_file = g_file_new_for_path (self->workdir);

  return g_file_get_child (workdir_file, path);
}

/**
 * foundry_repository_paths_get_workdir_path:
 * @self: a #FoundryGitRepositoryPaths
 * @path: the relative path within the working directory
 *
 * Gets a new path starting from the workdir.
 *
 * Returns: (transfer full): a new path
 */
char *
foundry_repository_paths_get_workdir_path (FoundryGitRepositoryPaths *self,
                                           const char                *path)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  return g_build_filename (self->workdir, path, NULL);
}

/**
 * foundry_git_repository_paths_dup_workdir:
 * @self: a #FoundryGitRepositoryPaths
 *
 * Duplicates the working directory path.
 *
 * Returns: (transfer full): a newly allocated copy of the working directory path
 */
char *
foundry_git_repository_paths_dup_workdir (FoundryGitRepositoryPaths *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return g_strdup (self->workdir);
}

/**
 * foundry_git_repository_paths_dup_git_dir:
 * @self: a #FoundryGitRepositoryPaths
 *
 * Duplicates the git directory path.
 *
 * Returns: (transfer full): a newly allocated copy of the git directory path
 */
char *
foundry_git_repository_paths_dup_git_dir (FoundryGitRepositoryPaths *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return g_strdup (self->git_dir);
}
