/* foundry-git-repository.c
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

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "foundry-auth-prompt.h"
#include "foundry-auth-provider.h"
#include "foundry-git-autocleanups.h"
#include "foundry-git-commit-private.h"
#include "foundry-git-error.h"
#include "foundry-git-blame-private.h"
#include "foundry-git-branch-private.h"
#include "foundry-git-callbacks-private.h"
#include "foundry-git-file-list-private.h"
#include "foundry-git-file-private.h"
#include "foundry-git-remote-private.h"
#include "foundry-git-repository-private.h"
#include "foundry-git-tag-private.h"

struct _FoundryGitRepository
{
  GObject         parent_instance;
  GMutex          mutex;
  git_repository *repository;
  GFile          *workdir;
};

G_DEFINE_FINAL_TYPE (FoundryGitRepository, foundry_git_repository, G_TYPE_OBJECT)

static void
foundry_git_repository_finalize (GObject *object)
{
  FoundryGitRepository *self = (FoundryGitRepository *)object;

  g_clear_pointer (&self->repository, git_repository_free);
  g_clear_object (&self->workdir);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (foundry_git_repository_parent_class)->finalize (object);
}

static void
foundry_git_repository_class_init (FoundryGitRepositoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_git_repository_finalize;
}

static void
foundry_git_repository_init (FoundryGitRepository *self)
{
  g_mutex_init (&self->mutex);
}

/**
 * _foundry_git_repository_new:
 * @repository: (transfer full): the git_repository to wrap
 *
 * Creates a new [class@Foundry.GitRepository] taking ownership of @repository.
 *
 * Returns: (transfer full):
 */
FoundryGitRepository *
_foundry_git_repository_new (git_repository *repository)
{
  FoundryGitRepository *self;
  const char *path;

  g_return_val_if_fail (repository != NULL, NULL);

  path = git_repository_workdir (repository);

  self = g_object_new (FOUNDRY_TYPE_GIT_REPOSITORY, NULL);
  self->repository = g_steal_pointer (&repository);
  self->workdir = g_file_new_for_path (path);

  return self;
}

static DexFuture *
foundry_git_repository_list_remotes_thread (gpointer data)
{
  FoundryGitRepository *self = data;
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(GListStore) store = NULL;
  g_auto(git_strarray) remotes = {0};

  g_assert (FOUNDRY_IS_GIT_REPOSITORY (self));

  locker = g_mutex_locker_new (&self->mutex);

  if (git_remote_list (&remotes, self->repository) != 0)
    return foundry_git_reject_last_error ();

  store = g_list_store_new (FOUNDRY_TYPE_VCS_REMOTE);

  for (gsize i = 0; i < remotes.count; i++)
    {
      g_autoptr(FoundryGitRemote) vcs_remote = NULL;
      g_autoptr(git_remote) remote = NULL;

      if (git_remote_lookup (&remote, self->repository, remotes.strings[i]) != 0)
        continue;

      if ((vcs_remote = _foundry_git_remote_new (g_steal_pointer (&remote), NULL)))
        g_list_store_append (store, vcs_remote);
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

DexFuture *
_foundry_git_repository_list_remotes (FoundryGitRepository *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));

  return dex_thread_spawn ("[git-list-remotes]",
                           foundry_git_repository_list_remotes_thread,
                           g_object_ref (self),
                           g_object_unref);
}

gboolean
_foundry_git_repository_is_ignored (FoundryGitRepository *self,
                                    const char           *relative_path)
{
  g_autoptr(GMutexLocker) locker = NULL;
  gboolean ignored = FALSE;

  g_return_val_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self), FALSE);
  g_return_val_if_fail (relative_path != NULL, FALSE);

  locker = g_mutex_locker_new (&self->mutex);

  if (git_ignore_path_is_ignored (&ignored, self->repository, relative_path) == GIT_OK)
    return ignored;

  return FALSE;
}

DexFuture *
_foundry_git_repository_list_files (FoundryGitRepository *self)
{
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(git_index) index = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));

  locker = g_mutex_locker_new (&self->mutex);

  if (git_repository_index (&index, self->repository) != 0)
    return foundry_git_reject_last_error ();

  return dex_future_new_take_object (_foundry_git_file_list_new (self->workdir, g_steal_pointer (&index)));
}

typedef struct _Blame
{
  FoundryGitRepository *self;
  char                 *relative_path;
  GBytes               *bytes;
} Blame;

static void
blame_free (Blame *state)
{
  g_clear_object (&state->self);
  g_clear_pointer (&state->relative_path, g_free);
  g_clear_pointer (&state->bytes, g_bytes_unref);
  g_free (state);
}

static DexFuture *
foundry_git_repository_blame_thread (gpointer user_data)
{
  Blame *state = user_data;
  g_autoptr(git_blame) blame = NULL;
  g_autoptr(git_blame) bytes_blame = NULL;
  g_autoptr(GMutexLocker) locker = NULL;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_GIT_REPOSITORY (state->self));
  g_assert (state->relative_path != NULL);

  locker = g_mutex_locker_new (&state->self->mutex);

  if (git_blame_file (&blame, state->self->repository, state->relative_path, NULL) != 0)
    return foundry_git_reject_last_error ();

  if (state->bytes != NULL)
    {
      gconstpointer data = g_bytes_get_data (state->bytes, NULL);
      gsize size = g_bytes_get_size (state->bytes);

      if (git_blame_buffer (&bytes_blame, blame, data, size) != 0)
        return foundry_git_reject_last_error ();
    }

  return dex_future_new_take_object (_foundry_git_blame_new (g_steal_pointer (&blame),
                                                             g_steal_pointer (&bytes_blame)));
}

DexFuture *
_foundry_git_repository_blame (FoundryGitRepository *self,
                               const char           *relative_path,
                               GBytes               *bytes)
{
  Blame *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));
  dex_return_error_if_fail (relative_path != NULL);

  state = g_new0 (Blame, 1);
  state->self = g_object_ref (self);
  state->relative_path = g_strdup (relative_path);
  state->bytes = bytes ? g_bytes_ref (bytes) : NULL;

  return dex_thread_spawn ("[git-blame]",
                           foundry_git_repository_blame_thread,
                           state,
                           (GDestroyNotify) blame_free);
}

static DexFuture *
foundry_git_repository_list_branches_thread (gpointer data)
{
  FoundryGitRepository *self = data;
  g_autoptr(git_branch_iterator) iter = NULL;
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(GListStore) store = NULL;

  g_assert (FOUNDRY_IS_GIT_REPOSITORY (self));

  locker = g_mutex_locker_new (&self->mutex);

  if (git_branch_iterator_new (&iter, self->repository, GIT_BRANCH_ALL) < 0)
    return foundry_git_reject_last_error ();

  store = g_list_store_new (FOUNDRY_TYPE_VCS_BRANCH);

  for (;;)
    {
      g_autoptr(FoundryGitBranch) branch = NULL;
      g_autoptr(git_reference) ref = NULL;
      git_branch_t branch_type;

      if (git_branch_next (&ref, &branch_type, iter) != 0)
        break;

      if ((branch = _foundry_git_branch_new (self, g_steal_pointer (&ref), branch_type)))
        g_list_store_append (store, branch);
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

DexFuture *
_foundry_git_repository_list_branches (FoundryGitRepository *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));

  return dex_thread_spawn ("[git-list-branches]",
                           foundry_git_repository_list_branches_thread,
                           g_object_ref (self),
                           g_object_unref);
}

static DexFuture *
foundry_git_repository_list_tags_thread (gpointer data)
{
  FoundryGitRepository *self = data;
  g_autoptr(git_reference_iterator) iter = NULL;
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(GListStore) store = NULL;

  g_assert (FOUNDRY_IS_GIT_REPOSITORY (self));

  locker = g_mutex_locker_new (&self->mutex);

  if (git_reference_iterator_new (&iter, self->repository) < 0)
    return foundry_git_reject_last_error ();

  store = g_list_store_new (FOUNDRY_TYPE_VCS_TAG);

  for (;;)
    {
      g_autoptr(git_reference) ref = NULL;
      const char *name;

      if (git_reference_next (&ref, iter) != 0)
        break;

      if ((name = git_reference_name (ref)))
        {
          if (g_str_has_prefix (name, "refs/tags/") || !!strstr (name, "/tags/"))
            {
              g_autoptr(FoundryGitTag) tag = NULL;

              if ((tag = _foundry_git_tag_new (self, g_steal_pointer (&ref))))
                g_list_store_append (store, tag);
            }
        }
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

DexFuture *
_foundry_git_repository_list_tags (FoundryGitRepository *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));

  return dex_thread_spawn ("[git-list-tags]",
                           foundry_git_repository_list_tags_thread,
                           g_object_ref (self),
                           g_object_unref);
}

typedef struct _FindRemote
{
  FoundryGitRepository *self;
  char *name;
} FindRemote;

static void
find_remote_free (FindRemote *state)
{
  g_clear_pointer (&state->name, g_free);
  g_clear_object (&state->self);
  g_free (state);
}

static DexFuture *
foundry_git_repository_find_remote_thread (gpointer data)
{
  FindRemote *state = data;
  g_autoptr(git_remote) remote = NULL;
  g_autoptr(GMutexLocker) locker = NULL;
  FoundryGitRepository *self;
  const char *name;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_GIT_REPOSITORY (state->self));
  g_assert (state->name != NULL);

  name = state->name;
  self = state->self;

  locker = g_mutex_locker_new (&self->mutex);

  if (git_remote_lookup (&remote, self->repository, name) == 0)
    {
      const char *alt_name = git_remote_name (remote);

      if (alt_name != NULL)
        name = alt_name;

      return dex_future_new_take_object (_foundry_git_remote_new (g_steal_pointer (&remote), name));
    }

  if (git_remote_create_anonymous (&remote, self->repository, name) == 0)
    {
      const char *alt_name = git_remote_name (remote);

      if (alt_name != NULL)
        name = alt_name;

      return dex_future_new_take_object (_foundry_git_remote_new (g_steal_pointer (&remote), name));
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_FOUND,
                                "Not found");
}

DexFuture *
_foundry_git_repository_find_remote (FoundryGitRepository *self,
                                     const char           *name)
{
  FindRemote *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));
  dex_return_error_if_fail (name != NULL);

  state = g_new0 (FindRemote, 1);
  state->self = g_object_ref (self);
  state->name = g_strdup (name);

  return dex_thread_spawn ("[git-find-remote]",
                           foundry_git_repository_find_remote_thread,
                           state,
                           (GDestroyNotify) find_remote_free);
}

DexFuture *
_foundry_git_repository_find_file (FoundryGitRepository *self,
                                   GFile                *file)
{
  g_autofree char *relative_path = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));
  dex_return_error_if_fail (G_IS_FILE (file));

  if (!g_file_has_prefix (file, self->workdir))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "File does not exist in working tree");

  relative_path = g_file_get_relative_path (self->workdir, file);

  g_assert (relative_path != NULL);

  return dex_future_new_take_object (_foundry_git_file_new (self->workdir, relative_path));
}

char *
_foundry_git_repository_dup_branch_name (FoundryGitRepository *self)
{
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(git_reference) head = NULL;

  g_return_val_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self), NULL);

  locker = g_mutex_locker_new (&self->mutex);

  if (git_repository_head (&head, self->repository) == 0)
    {
      const char *branch_name = NULL;

      if (git_branch_name (&branch_name, head) == 0)
        return g_strdup (branch_name);
    }

  return NULL;
}

typedef struct _Fetch
{
  char                *git_dir;
  char                *remote_name;
  FoundryOperation    *operation;
  FoundryAuthProvider *auth_provider;
  int                  pty_fd;
} Fetch;

static void
fetch_free (Fetch *state)
{
  g_clear_pointer (&state->git_dir, g_free);
  g_clear_pointer (&state->remote_name, g_free);
  g_clear_object (&state->operation);
  g_clear_object (&state->auth_provider);
  g_clear_fd (&state->pty_fd, NULL);
  g_free (state);
}

static DexFuture *
foundry_git_repository_fetch_thread (gpointer user_data)
{
  Fetch *state = user_data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_remote) remote = NULL;
  git_fetch_options fetch_opts;
  int rval;

  g_assert (state != NULL);
  g_assert (state->git_dir != NULL);
  g_assert (state->remote_name != NULL);
  g_assert (FOUNDRY_IS_OPERATION (state->operation));

  if (git_repository_open (&repository, state->git_dir) != 0)
    return foundry_git_reject_last_error ();

  if (git_remote_lookup (&remote, repository, state->remote_name) != 0 &&
      git_remote_create_anonymous (&remote, repository, state->remote_name) != 0)
    return foundry_git_reject_last_error ();

  git_fetch_options_init (&fetch_opts, GIT_FETCH_OPTIONS_VERSION);

  fetch_opts.download_tags = GIT_REMOTE_DOWNLOAD_TAGS_ALL;
  fetch_opts.update_fetchhead = 1;

  _foundry_git_callbacks_init (&fetch_opts.callbacks, state->operation, state->auth_provider, state->pty_fd);
  rval = git_remote_fetch (remote, NULL, &fetch_opts, NULL);
  _foundry_git_callbacks_clear (&fetch_opts.callbacks);

  if (rval != 0)
    return foundry_git_reject_last_error ();

  return dex_future_new_true ();
}

DexFuture *
_foundry_git_repository_fetch (FoundryGitRepository *self,
                               FoundryAuthProvider  *auth_provider,
                               FoundryVcsRemote     *remote,
                               FoundryOperation     *operation)
{
  Fetch *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));
  dex_return_error_if_fail (FOUNDRY_IS_AUTH_PROVIDER (auth_provider));
  dex_return_error_if_fail (FOUNDRY_IS_GIT_REMOTE (remote));
  dex_return_error_if_fail (FOUNDRY_IS_OPERATION (operation));

  state = g_new0 (Fetch, 1);
  state->remote_name = foundry_vcs_remote_dup_name (remote);
  state->git_dir = g_strdup (git_repository_path (self->repository));
  state->operation = g_object_ref (operation);
  state->auth_provider = g_object_ref (auth_provider);
  state->pty_fd = -1;

  return dex_thread_spawn ("[git-fetch]",
                           foundry_git_repository_fetch_thread,
                           state,
                           (GDestroyNotify) fetch_free);
}

typedef struct _FindCommit
{
  FoundryGitRepository *self;
  git_oid oid;
} FindCommit;

static void
find_commit_free (FindCommit *state)
{
  g_clear_object (&state->self);
  g_free (state);
}

static DexFuture *
foundry_git_repository_find_commit_thread (gpointer data)
{
  FindCommit *state = data;
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(git_commit) commit = NULL;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_GIT_REPOSITORY (state->self));

  locker = g_mutex_locker_new (&state->self->mutex);

  if (git_commit_lookup (&commit, state->self->repository, &state->oid) != 0)
    return foundry_git_reject_last_error ();

  return dex_future_new_take_object (_foundry_git_commit_new (g_steal_pointer (&commit),
                                                              (GDestroyNotify) git_commit_free));
}

DexFuture *
_foundry_git_repository_find_commit (FoundryGitRepository *self,
                                     const char           *id)
{
  FindCommit *state;
  git_oid oid;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));
  dex_return_error_if_fail (id != NULL);

  if (git_oid_fromstr (&oid, id) != 0)
    return foundry_git_reject_last_error ();

  state = g_new0 (FindCommit, 1);
  state->self = g_object_ref (self);
  state->oid = oid;

  return dex_thread_spawn ("[git-find-commit]",
                           foundry_git_repository_find_commit_thread,
                           state,
                           (GDestroyNotify) find_commit_free);

}
