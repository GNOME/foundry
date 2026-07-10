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

#include "foundry-auth-provider.h"
#include "foundry-git-autocleanups.h"
#include "foundry-git-blame-private.h"
#include "foundry-git-branch-private.h"
#include "foundry-git-callbacks-private.h"
#include "foundry-git-commit-private.h"
#include "foundry-git-error.h"
#include "foundry-git-file-list-private.h"
#include "foundry-git-file-private.h"
#include "foundry-git-graph-private.h"
#include "foundry-git-line-changes-private.h"
#include "foundry-git-monitor-private.h"
#include "foundry-git-private.h"
#include "foundry-git-remote-private.h"
#include "foundry-git-repository-private.h"
#include "foundry-git-repository-paths-private.h"
#include "foundry-git-status-list-private.h"
#include "foundry-git-tag-private.h"
#include "foundry-git-tree-private.h"
#include "foundry-util.h"
#include "foundry-vcs.h"

#include "line-cache.h"
#include "foundry-trace-private.h"

struct _FoundryGitRepository
{
  GObject                    parent_instance;
  GMutex                     mutex;
  git_repository            *repository;
  GFile                     *workdir;
  char                      *git_dir;
  FoundryGitRepositoryPaths *paths;
  DexFuture                 *monitor;
};

typedef struct _Stash
{
  FoundryGitRepositoryPaths *paths;
  guint                      include_untracked : 1;
} Stash;

G_DEFINE_FINAL_TYPE (FoundryGitRepository, foundry_git_repository, G_TYPE_OBJECT)

static void
foundry_git_repository_finalize (GObject *object)
{
  FoundryGitRepository *self = (FoundryGitRepository *)object;

  dex_clear (&self->monitor);
  g_clear_pointer (&self->repository, git_repository_free);
  g_clear_pointer (&self->git_dir, g_free);
  g_clear_object (&self->workdir);
  g_clear_pointer (&self->paths, foundry_git_repository_paths_unref);
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
  const char *workdir_path;

  g_return_val_if_fail (repository != NULL, NULL);

  path = git_repository_workdir (repository);

  self = g_object_new (FOUNDRY_TYPE_GIT_REPOSITORY, NULL);
  self->git_dir = g_strdup (git_repository_path (repository));
  self->repository = g_steal_pointer (&repository);
  self->workdir = g_file_new_for_path (path);
  workdir_path = path ? path : self->git_dir;
  self->paths = foundry_git_repository_paths_new (self->git_dir, workdir_path);

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

  FOUNDRY_TRACE_SCOPE_FUNC ();

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

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-list-remotes]",
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

  FOUNDRY_TRACE_SCOPE_FUNC ();

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

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-blame]",
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

  FOUNDRY_TRACE_SCOPE_FUNC ();

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

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-list-branches]",
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

  FOUNDRY_TRACE_SCOPE_FUNC ();

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

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-list-tags]",
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

  FOUNDRY_TRACE_SCOPE_FUNC ();

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

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-find-remote]",
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

char *
_foundry_git_repository_dup_git_dir (FoundryGitRepository *self)
{
  g_autoptr(GMutexLocker) locker = NULL;

  g_return_val_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self), NULL);

  locker = g_mutex_locker_new (&self->mutex);

  return g_strdup (self->git_dir);
}

FoundryGitRepositoryPaths *
_foundry_git_repository_dup_paths (FoundryGitRepository *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self), NULL);

  return foundry_git_repository_paths_ref (self->paths);
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

  FOUNDRY_TRACE_SCOPE ("git.fetch",
                       "%s",
                       state->remote_name);

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

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-fetch]",
                                 foundry_git_repository_fetch_thread,
                                 state,
                                 (GDestroyNotify) fetch_free);
}

typedef struct _FindCommitById
{
  FoundryGitRepository *self;
  char *id;
} FindCommitById;

static void
find_commit_by_id_free (FindCommitById *state)
{
  g_clear_object (&state->self);
  g_clear_pointer (&state->id, g_free);
  g_free (state);
}

typedef struct _FindByOid
{
  FoundryGitRepository *self;
  git_oid oid;
} FindByOid;

static void
find_by_oid_free (FindByOid *state)
{
  g_clear_object (&state->self);
  g_free (state);
}

static DexFuture *
foundry_git_repository_find_commit_thread (gpointer data)
{
  FindCommitById *state = data;
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(git_commit) commit = NULL;
  git_oid oid;
  int err;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_GIT_REPOSITORY (state->self));

  FOUNDRY_TRACE_SCOPE_FUNC ();

  locker = g_mutex_locker_new (&state->self->mutex);

  /* First try to parse as an OID */
  if (git_oid_fromstr (&oid, state->id) == 0)
    {
      if (git_commit_lookup (&commit, state->self->repository, &oid) != 0)
        return foundry_git_reject_last_error ();
    }
  else
    {
      /* If not an OID, try to resolve as a reference */
      if ((err = git_reference_name_to_id (&oid, state->self->repository, state->id)) != 0)
        {
          if (err == GIT_ENOTFOUND)
            return dex_future_new_reject (G_IO_ERROR,
                                          G_IO_ERROR_NOT_FOUND,
                                          "Reference '%s' not found", state->id);

          return foundry_git_reject_last_error ();
        }

      if (git_commit_lookup (&commit, state->self->repository, &oid) != 0)
        return foundry_git_reject_last_error ();
    }

  return dex_future_new_take_object (_foundry_git_commit_new (g_steal_pointer (&commit),
                                                              (GDestroyNotify) git_commit_free,
                                                              _foundry_git_repository_dup_paths (state->self)));
}

DexFuture *
_foundry_git_repository_find_commit (FoundryGitRepository *self,
                                     const char           *id)
{
  FindCommitById *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));
  dex_return_error_if_fail (id != NULL);

  state = g_new0 (FindCommitById, 1);
  state->self = g_object_ref (self);
  state->id = g_strdup (id);

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-find-commit]",
                                 foundry_git_repository_find_commit_thread,
                                 state,
                                 (GDestroyNotify) find_commit_by_id_free);
}

static DexFuture *
foundry_git_repository_find_tree_thread (gpointer data)
{
  FindByOid *state = data;
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(git_tree) tree = NULL;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_GIT_REPOSITORY (state->self));

  FOUNDRY_TRACE_SCOPE_FUNC ();

  locker = g_mutex_locker_new (&state->self->mutex);

  if (git_tree_lookup (&tree, state->self->repository, &state->oid) != 0)
    return foundry_git_reject_last_error ();

  return dex_future_new_take_object (_foundry_git_tree_new (g_steal_pointer (&tree)));
}

DexFuture *
_foundry_git_repository_find_tree (FoundryGitRepository *self,
                                   const char           *id)
{
  FindByOid *state;
  git_oid oid;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));
  dex_return_error_if_fail (id != NULL);

  if (git_oid_fromstr (&oid, id) != 0)
    return foundry_git_reject_last_error ();

  state = g_new0 (FindByOid, 1);
  state->self = g_object_ref (self);
  state->oid = oid;

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-find-tree]",
                                 foundry_git_repository_find_tree_thread,
                                 state,
                                 (GDestroyNotify) find_by_oid_free);
}

typedef struct _ListCommits
{
  FoundryGitRepositoryPaths *paths;
  char *relative_path;
} ListCommits;

static void
list_commits_free (ListCommits *state)
{
  g_clear_pointer (&state->paths, foundry_git_repository_paths_unref);
  g_clear_pointer (&state->relative_path, g_free);
  g_free (state);
}

static DexFuture *
foundry_git_repository_list_commits_thread (gpointer data)
{
  ListCommits *state = data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_revwalk) walker = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GError) error = NULL;
  git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;
  const char *paths[2] = {0};
  git_strarray pathspec = {(char**)paths, 1};
  git_oid oid;

  g_assert (state != NULL);
  g_assert (state->paths != NULL);
  g_assert (state->relative_path != NULL);

  FOUNDRY_TRACE_SCOPE_FUNC ();

  store = g_list_store_new (FOUNDRY_TYPE_VCS_COMMIT);

  if (!foundry_git_repository_paths_open (state->paths, &repository, &error))
    return dex_future_new_reject (error->domain, error->code, "%s", error->message);

  if (git_revwalk_new (&walker, repository) != 0)
    return foundry_git_reject_last_error ();

  paths[0] = state->relative_path;
  diff_opts.pathspec = pathspec;

  git_revwalk_sorting (walker, GIT_SORT_TIME | GIT_SORT_REVERSE);
  git_revwalk_push_head (walker);

  /* This could be made faster if libgit2 had support for the Git bitmap index.
   * Without it, we cannot filter the tree by file. So instead, we have to walk
   * commits and compare them against the parent commit.
   *
   * This is mostly fine on smaller repositories, but can be more problematic
   * on larger ones.
   *
   * What would be nice is if someone went and added bitmap support to libgit2.
   */

  while (git_revwalk_next (&oid, walker) == 0)
    {
      g_autoptr(git_commit) commit = NULL;
      g_autoptr(git_commit) parent = NULL;
      g_autoptr(git_tree) parent_tree = NULL;
      g_autoptr(git_tree) commit_tree = NULL;
      g_autoptr(git_diff) diff = NULL;
      gsize n_deltas;

      if (git_commit_lookup (&commit, repository, &oid) != 0 ||
          git_commit_parentcount (commit) == 0 ||
          git_commit_parent (&parent, commit, 0) != 0 ||
          git_commit_tree (&commit_tree, commit) != 0 ||
          git_commit_tree (&parent_tree, parent) != 0)
        continue;

      if (git_diff_tree_to_tree (&diff, repository, parent_tree, commit_tree, &diff_opts) != 0)
        continue;

      n_deltas = git_diff_num_deltas (diff);

      for (gsize i = 0; i < n_deltas; i++)
        {
          const git_diff_delta *delta = git_diff_get_delta (diff, i);

          if (strcmp (delta->new_file.path, state->relative_path) == 0 ||
              strcmp (delta->old_file.path, state->relative_path) == 0)
            {
              g_autoptr(FoundryGitCommit) item = NULL;

              if ((item = _foundry_git_commit_new (g_steal_pointer (&commit),
                                                   (GDestroyNotify) git_commit_free,
                                                   foundry_git_repository_paths_ref (state->paths))))
                g_list_store_append (store, item);

              break;
            }
        }
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

DexFuture *
_foundry_git_repository_list_commits_with_file (FoundryGitRepository *self,
                                                FoundryVcsFile       *file)
{
  ListCommits *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));
  dex_return_error_if_fail (FOUNDRY_IS_VCS_FILE (file));

  state = g_new0 (ListCommits, 1);
  state->paths = _foundry_git_repository_dup_paths (self);
  state->relative_path = foundry_vcs_file_dup_relative_path (file);

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-list-commits]",
                                 foundry_git_repository_list_commits_thread,
                                 state,
                                 (GDestroyNotify) list_commits_free);
}

typedef struct _HistoryLane
{
  git_oid oid;
  guint color_id;
  guint inactive;
} HistoryLane;

typedef struct _HistoryCollapsedLane
{
  git_oid oid;
  guint color_id;
  guint index;
} HistoryCollapsedLane;

typedef struct _LoadGraph
{
  FoundryGitRepositoryPaths *paths;
  git_oid start_oid;
  git_oid end_oid;
  guint limit;
  guint has_end : 1;
} LoadGraph;

static void
load_graph_free (LoadGraph *state)
{
  g_clear_pointer (&state->paths, foundry_git_repository_paths_unref);
  g_free (state);
}

static gssize
history_find_lane (GArray        *lanes,
                   const git_oid *oid)
{
  g_assert (lanes != NULL);
  g_assert (oid != NULL);

  for (guint i = 0; i < lanes->len; i++)
    {
      const HistoryLane *lane = &g_array_index (lanes, HistoryLane, i);

      if (git_oid_equal (&lane->oid, oid))
        return i;
    }

  return -1;
}

static guint
history_append_lane (GArray        *lanes,
                     const git_oid *oid,
                     guint         *next_color_id)
{
  HistoryLane lane;

  g_assert (lanes != NULL);
  g_assert (oid != NULL);
  g_assert (next_color_id != NULL);

  lane.oid = *oid;
  lane.color_id = (*next_color_id)++;
  lane.inactive = 0;
  g_array_append_val (lanes, lane);

  return lanes->len - 1;
}

static guint
history_insert_lane (GArray        *lanes,
                     const git_oid *oid,
                     guint          color_id,
                     guint          index)
{
  HistoryLane lane;

  g_assert (lanes != NULL);
  g_assert (oid != NULL);

  lane.oid = *oid;
  lane.color_id = color_id;
  lane.inactive = 0;

  index = MIN (index, lanes->len);
  g_array_insert_val (lanes, index, lane);

  return index;
}

static gboolean
history_expand_collapsed_lane (GArray        *lanes,
                               GHashTable    *collapsed,
                               const git_oid *oid)
{
  g_autofree char *key = NULL;
  HistoryCollapsedLane *lane;

  g_assert (lanes != NULL);
  g_assert (collapsed != NULL);
  g_assert (oid != NULL);

  key = _foundry_git_oid_dup_string (oid);

  if (!(lane = g_hash_table_lookup (collapsed, key)))
    return FALSE;

  history_insert_lane (lanes, &lane->oid, lane->color_id, lane->index);
  g_hash_table_remove (collapsed, key);

  return TRUE;
}

static void
history_collapse_lane (GArray     *lanes,
                       GHashTable *collapsed,
                       guint       index)
{
  HistoryCollapsedLane *collapsed_lane;
  const HistoryLane *lane;

  g_assert (lanes != NULL);
  g_assert (collapsed != NULL);
  g_assert (index < lanes->len);

  lane = &g_array_index (lanes, HistoryLane, index);
  collapsed_lane = g_new0 (HistoryCollapsedLane, 1);
  collapsed_lane->oid = lane->oid;
  collapsed_lane->color_id = lane->color_id;
  collapsed_lane->index = index;

  g_hash_table_replace (collapsed,
                        _foundry_git_oid_dup_string (&lane->oid),
                        collapsed_lane);
  g_array_remove_index (lanes, index);
}

static gboolean
history_lane_is_parent_of_active_lane (git_repository *repository,
                                       GArray         *lanes,
                                       const git_oid  *parent_oid,
                                       guint           parent_index)
{
  g_assert (repository != NULL);
  g_assert (lanes != NULL);
  g_assert (parent_oid != NULL);

  for (guint i = 0; i < lanes->len; i++)
    {
      g_autoptr(git_commit) commit = NULL;
      const HistoryLane *lane;
      guint parent_count;

      if (i == parent_index)
        continue;

      lane = &g_array_index (lanes, HistoryLane, i);

      if (git_commit_lookup (&commit, repository, &lane->oid) != 0)
        continue;

      parent_count = git_commit_parentcount (commit);

      for (guint j = 0; j < parent_count; j++)
        {
          const git_oid *oid = git_commit_parent_id (commit, j);

          if (git_oid_equal (oid, parent_oid))
            return TRUE;
        }
    }

  return FALSE;
}

static void
history_collapse_inactive_lanes (git_repository *repository,
                                 GArray         *lanes,
                                 GHashTable     *collapsed)
{
  enum {
    INACTIVE_MAX = 30,
    INACTIVE_GAP = 10,
  };

  g_assert (repository != NULL);
  g_assert (collapsed != NULL);
  g_assert (lanes != NULL);

  /* TODO: Render folded lanes before enabling this; hiding them creates breaks. */
  return;

  for (guint i = lanes->len; i > 0; i--)
    {
      const HistoryLane *lane = &g_array_index (lanes, HistoryLane, i - 1);

      if (i > 1 &&
          lane->inactive >= INACTIVE_MAX + INACTIVE_GAP &&
          !history_lane_is_parent_of_active_lane (repository, lanes, &lane->oid, i - 1))
        history_collapse_lane (lanes, collapsed, i - 1);
    }
}

static void
history_append_segment (GArray               *segments,
                        guint                 from_lane,
                        guint                 to_lane,
                        FoundryVcsGraphPoint  from_point,
                        FoundryVcsGraphPoint  to_point,
                        guint                 color_id)
{
  FoundryVcsGraphSegment segment;

  g_assert (segments != NULL);

  segment.from_lane = from_lane;
  segment.to_lane = to_lane;
  segment.from_point = from_point;
  segment.to_point = to_point;
  segment.color_id = color_id;

  g_array_append_val (segments, segment);
}

static GArray *
history_copy_lanes (GArray *lanes)
{
  GArray *copy;

  g_assert (lanes != NULL);

  copy = g_array_sized_new (FALSE, FALSE, sizeof (HistoryLane), lanes->len);

  if (lanes->len > 0)
    g_array_append_vals (copy, lanes->data, lanes->len);

  return copy;
}

static gssize
history_find_matching_lane (GArray            *lanes,
                            const HistoryLane *match)
{
  g_assert (lanes != NULL);
  g_assert (match != NULL);

  for (guint i = 0; i < lanes->len; i++)
    {
      const HistoryLane *lane = &g_array_index (lanes, HistoryLane, i);

      if (lane->color_id == match->color_id &&
          git_oid_equal (&lane->oid, &match->oid))
        return i;
    }

  return -1;
}

static void
history_remap_shifted_segments (GArray *segments,
                                GArray *before,
                                GArray *after)
{
  g_assert (segments != NULL);
  g_assert (before != NULL);
  g_assert (after != NULL);

  if (before->len == after->len)
    return;

  for (guint i = 0; i < segments->len; i++)
    {
      FoundryVcsGraphSegment *segment;
      const HistoryLane *lane;
      gssize new_lane;

      segment = &g_array_index (segments, FoundryVcsGraphSegment, i);

      if (segment->to_point != FOUNDRY_VCS_GRAPH_POINT_BOTTOM ||
          segment->to_lane >= before->len)
        continue;

      lane = &g_array_index (before, HistoryLane, segment->to_lane);

      if ((new_lane = history_find_matching_lane (after, lane)) >= 0)
        segment->to_lane = (guint)new_lane;
    }
}

static void
history_remap_expanded_segments (GArray *segments,
                                 GArray *before,
                                 GArray *after)
{
  g_assert (segments != NULL);
  g_assert (before != NULL);
  g_assert (after != NULL);

  if (before->len == after->len)
    return;

  for (guint i = 0; i < segments->len; i++)
    {
      FoundryVcsGraphSegment *segment;
      const HistoryLane *lane;
      gssize old_lane;

      segment = &g_array_index (segments, FoundryVcsGraphSegment, i);

      if (segment->from_point != FOUNDRY_VCS_GRAPH_POINT_TOP ||
          segment->from_lane >= after->len)
        continue;

      lane = &g_array_index (after, HistoryLane, segment->from_lane);

      if ((old_lane = history_find_matching_lane (before, lane)) >= 0)
        segment->from_lane = (guint)old_lane;
    }
}

static DexFuture *
foundry_git_repository_load_graph_thread (gpointer data)
{
  LoadGraph *state = data;
  g_autoptr(FoundryGitGraphBuilder) builder = NULL;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_revwalk) walker = NULL;
  g_autoptr(GHashTable) collapsed = NULL;
  g_autoptr(GArray) lanes = NULL;
  g_autoptr(GError) error = NULL;
  guint next_color_id = 0;
  guint n_items = 0;
  guint graph_n_lanes = 0;
  git_oid oid;

  g_assert (state != NULL);
  g_assert (state->paths != NULL);

  FOUNDRY_TRACE_SCOPE_FUNC ();

  builder = foundry_git_graph_builder_new ();
  lanes = g_array_new (FALSE, FALSE, sizeof (HistoryLane));
  collapsed = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  if (!foundry_git_repository_paths_open (state->paths, &repository, &error))
    return dex_future_new_reject (error->domain, error->code, "%s", error->message);

  if (git_revwalk_new (&walker, repository) != 0)
    return foundry_git_reject_last_error ();

  git_revwalk_sorting (walker, (GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME));

  if (git_revwalk_push (walker, &state->start_oid) != 0)
    return foundry_git_reject_last_error ();

  if (state->has_end && git_revwalk_hide (walker, &state->end_oid) != 0)
    return foundry_git_reject_last_error ();

  while (git_revwalk_next (&oid, walker) == 0)
    {
      g_autoptr(git_commit) commit = NULL;
      g_autoptr(GArray) parent_lanes = NULL;
      g_autoptr(GArray) segments = NULL;
      g_autoptr(GArray) lanes_before_expansion = NULL;
      g_autoptr(GArray) lanes_before_removal = NULL;
      gssize found_lane;
      guint commit_lane;
      guint commit_color_id;
      guint n_lanes;
      guint parent_count;
      gboolean was_active;
      gboolean was_expanded;
      gboolean remove_commit_lane = FALSE;

      if (state->limit > 0 && n_items >= state->limit)
        break;

      if (git_commit_lookup (&commit, repository, &oid) != 0)
        return foundry_git_reject_last_error ();

      parent_count = git_commit_parentcount (commit);

      for (guint i = 0; i < lanes->len; i++)
        {
          HistoryLane *lane = &g_array_index (lanes, HistoryLane, i);

          lane->inactive++;
        }

      lanes_before_expansion = history_copy_lanes (lanes);

      was_expanded = history_expand_collapsed_lane (lanes, collapsed, &oid);

      for (guint i = 0; i < parent_count; i++)
        {
          const git_oid *parent_oid = git_commit_parent_id (commit, i);

          history_expand_collapsed_lane (lanes, collapsed, parent_oid);
        }

      parent_lanes = g_array_sized_new (FALSE, FALSE, sizeof (guint), parent_count);
      segments = g_array_new (FALSE, FALSE, sizeof (FoundryVcsGraphSegment));
      found_lane = history_find_lane (lanes, &oid);
      was_active = found_lane >= 0;

      if (was_active)
        commit_lane = found_lane;
      else
        commit_lane = history_append_lane (lanes, &oid, &next_color_id);

      {
        HistoryLane *lane = &g_array_index (lanes, HistoryLane, commit_lane);

        lane->inactive = 0;
        commit_color_id = lane->color_id;
      }

      n_lanes = lanes->len;

      for (guint i = 0; i < lanes->len; i++)
        {
          const HistoryLane *lane = &g_array_index (lanes, HistoryLane, i);

          if (i == commit_lane)
            {
              if (was_active && !was_expanded)
                history_append_segment (segments,
                                        i,
                                        i,
                                        FOUNDRY_VCS_GRAPH_POINT_TOP,
                                        FOUNDRY_VCS_GRAPH_POINT_CENTER,
                                        lane->color_id);
            }
          else
            {
              history_append_segment (segments,
                                      i,
                                      i,
                                      FOUNDRY_VCS_GRAPH_POINT_TOP,
                                      FOUNDRY_VCS_GRAPH_POINT_BOTTOM,
                                      lane->color_id);
          }
        }

      history_remap_expanded_segments (segments, lanes_before_expansion, lanes);

      for (guint i = 0; i < parent_count; i++)
        {
          const git_oid *parent_oid = git_commit_parent_id (commit, i);
          gssize parent_lane;
          guint lane_index;

          if ((parent_lane = history_find_lane (lanes, parent_oid)) >= 0 &&
              parent_lane != commit_lane)
            {
              lane_index = parent_lane;

              if (i == 0)
                remove_commit_lane = TRUE;
            }
          else if (i == 0)
            {
              lane_index = commit_lane;
            }
          else
            {
              lane_index = history_append_lane (lanes, parent_oid, &next_color_id);
              n_lanes = MAX (n_lanes, lanes->len);
            }

          g_array_append_val (parent_lanes, lane_index);

          {
            HistoryLane *lane = &g_array_index (lanes, HistoryLane, lane_index);

            lane->inactive = 0;
          }
        }

      for (guint i = 0; i < parent_lanes->len; i++)
        {
          guint lane_index = g_array_index (parent_lanes, guint, i);
          const HistoryLane *lane = &g_array_index (lanes, HistoryLane, lane_index);

          history_append_segment (segments,
                                  commit_lane,
                                  lane_index,
                                  FOUNDRY_VCS_GRAPH_POINT_CENTER,
                                  FOUNDRY_VCS_GRAPH_POINT_BOTTOM,
                                  i == 0 ? commit_color_id : lane->color_id);
        }

      if (parent_count > 0 && !remove_commit_lane)
        {
          const git_oid *parent_oid = git_commit_parent_id (commit, 0);
          HistoryLane *lane = &g_array_index (lanes, HistoryLane, commit_lane);

          lane->oid = *parent_oid;
          lane->inactive = 0;
        }

      lanes_before_removal = history_copy_lanes (lanes);

      if (parent_count == 0 || remove_commit_lane)
        g_array_remove_index (lanes, commit_lane);

      history_collapse_inactive_lanes (repository, lanes, collapsed);
      history_remap_shifted_segments (segments, lanes_before_removal, lanes);

      foundry_git_graph_builder_add (builder, &oid, commit_lane, n_lanes, segments);

      graph_n_lanes = MAX (graph_n_lanes, n_lanes);
      n_items++;
    }

  return dex_future_new_take_object (foundry_git_graph_builder_finish (g_steal_pointer (&builder), graph_n_lanes));
}

DexFuture *
_foundry_git_repository_load_graph (FoundryGitRepository *self,
                                    FoundryVcsCommit     *start,
                                    FoundryVcsCommit     *end,
                                    guint                 limit)
{
  LoadGraph *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));
  dex_return_error_if_fail (FOUNDRY_IS_GIT_COMMIT (start));
  dex_return_error_if_fail (!end || FOUNDRY_IS_GIT_COMMIT (end));

  state = g_new0 (LoadGraph, 1);
  state->paths = _foundry_git_repository_dup_paths (self);
  state->limit = limit;

  _foundry_git_commit_get_oid (FOUNDRY_GIT_COMMIT (start), &state->start_oid);

  if (end != NULL)
    {
      state->has_end = TRUE;
      _foundry_git_commit_get_oid (FOUNDRY_GIT_COMMIT (end), &state->end_oid);
    }

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-load-graph]",
                                 foundry_git_repository_load_graph_thread,
                                 state,
                                 (GDestroyNotify) load_graph_free);
}

DexFuture *
_foundry_git_repository_diff (FoundryGitRepository *self,
                              FoundryGitTree       *tree_a,
                              FoundryGitTree       *tree_b)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));
  dex_return_error_if_fail (FOUNDRY_IS_GIT_TREE (tree_a));
  dex_return_error_if_fail (FOUNDRY_IS_GIT_TREE (tree_b));

  return _foundry_git_tree_diff (tree_a, tree_b, self->paths);
}

typedef struct _DescribeLineChanges
{
  FoundryGitRepository *self;
  FoundryGitFile       *file;
  GBytes               *contents;
} DescribeLineChanges;

static void
describe_line_changes_free (DescribeLineChanges *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->file);
  g_clear_pointer (&state->contents, g_bytes_unref);
  g_free (state);
}

typedef struct _Range
{
  int old_start;
  int old_lines;
  int new_start;
  int new_lines;
} Range;

static int
diff_hunk_cb (const git_diff_delta *delta,
              const git_diff_hunk  *hunk,
              gpointer              user_data)
{
  GArray *ranges = user_data;
  Range range;

  g_assert (delta != NULL);
  g_assert (hunk != NULL);
  g_assert (ranges != NULL);

  range.old_start = hunk->old_start;
  range.old_lines = hunk->old_lines;
  range.new_start = hunk->new_start;
  range.new_lines = hunk->new_lines;

  g_array_append_val (ranges, range);

  return 0;
}

static DexFuture *
foundry_git_repository_describe_line_changes_fiber (gpointer data)
{
  DescribeLineChanges *state = data;
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(git_tree_entry) entry = NULL;
  g_autoptr(git_commit) commit = NULL;
  g_autoptr(git_blob) blob = NULL;
  g_autoptr(git_tree) tree = NULL;
  g_autoptr(GArray) ranges = NULL;
  g_autoptr(LineCache) cache = NULL;
  g_autofree char *path = NULL;
  FoundryGitRepository *self;
  FoundryGitFile *file;
  git_diff_options options;
  git_oid oid;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_GIT_REPOSITORY (state->self));
  g_assert (FOUNDRY_IS_GIT_FILE (state->file));
  g_assert (state->contents != NULL);

  FOUNDRY_TRACE_SCOPE_FUNC ();

  self = state->self;
  file = state->file;
  path = foundry_vcs_file_dup_relative_path (FOUNDRY_VCS_FILE (file));
  ranges = g_array_new (FALSE, FALSE, sizeof (Range));

  locker = g_mutex_locker_new (&state->self->mutex);

  if (git_reference_name_to_id (&oid, self->repository, "HEAD") != 0)
    return foundry_git_reject_last_error ();

  if (git_commit_lookup (&commit, self->repository, &oid) != 0)
    return foundry_git_reject_last_error ();

  if (git_commit_tree (&tree, commit) != 0)
    return foundry_git_reject_last_error ();

  if (git_tree_entry_bypath (&entry, tree, path) != 0)
    return foundry_git_reject_last_error ();

  if (git_blob_lookup (&blob, self->repository, git_tree_entry_id (entry)) != 0)
    return foundry_git_reject_last_error ();

  git_diff_options_init (&options, GIT_DIFF_OPTIONS_VERSION);
  options.context_lines = 0;

  git_diff_blob_to_buffer (blob,
                           path,
                           g_bytes_get_data (state->contents, NULL),
                           g_bytes_get_size (state->contents),
                           path,
                           &options,
                           NULL,         /* File Callback */
                           NULL,         /* Binary Callback */
                           diff_hunk_cb, /* Hunk Callback */
                           NULL,
                           ranges);

  cache = line_cache_new ();

  for (guint i = 0; i < ranges->len; i++)
    {
      const Range *range = &g_array_index (ranges, Range, i);
      int start_line = range->new_start - 1;
      int end_line = range->new_start + range->new_lines - 1;

      if (range->old_lines == 0 && range->new_lines > 0)
        {
          line_cache_mark_range (cache, start_line, end_line, LINE_MARK_ADDED);
        }
      else if (range->new_lines == 0 && range->old_lines > 0)
        {
          if (start_line < 0)
            line_cache_mark_range (cache, 0, 0, LINE_MARK_PREVIOUS_REMOVED);
          else
            line_cache_mark_range (cache, start_line + 1, start_line + 1, LINE_MARK_REMOVED);
        }
      else
        {
          line_cache_mark_range (cache, start_line, end_line, LINE_MARK_CHANGED);
        }
    }

  return dex_future_new_take_object (_foundry_git_line_changes_new (g_steal_pointer (&cache)));
}

DexFuture *
_foundry_git_repository_describe_line_changes (FoundryGitRepository *self,
                                               FoundryVcsFile       *file,
                                               GBytes               *contents)
{
  DescribeLineChanges *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));
  dex_return_error_if_fail (FOUNDRY_IS_GIT_FILE (file));
  dex_return_error_if_fail (contents != NULL);

  state = g_new0 (DescribeLineChanges, 1);
  state->self = g_object_ref (self);
  state->file = g_object_ref (FOUNDRY_GIT_FILE (file));
  state->contents = g_bytes_ref (contents);

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              foundry_git_repository_describe_line_changes_fiber,
                              state,
                              (GDestroyNotify) describe_line_changes_free);
}

typedef struct _QueryFileStatus
{
  FoundryGitRepository *self;
  DexPromise *promise;
  char *path;
} QueryFileStatus;

static void
foundry_git_repository_query_file_status_worker (gpointer data)
{
  QueryFileStatus *state = data;
  git_status_t status;
  int rval;

  FOUNDRY_TRACE_SCOPE_FUNC ();

  g_mutex_lock (&state->self->mutex);
  rval = git_status_file (&status, state->self->repository, state->path);
  g_mutex_unlock (&state->self->mutex);

  if (rval != 0)
    {
      dex_promise_reject (state->promise,
                          g_error_new (G_IO_ERROR,
                                       G_IO_ERROR_INVAL,
                                       "Invalid parameter"));
    }
  else
    {
      FoundryVcsFileStatus flags = 0;
      g_auto(GValue) value = G_VALUE_INIT;

      if (status & GIT_STATUS_WT_NEW)
        flags |= FOUNDRY_VCS_FILE_STATUS_NEW_IN_TREE;

      if (status & GIT_STATUS_WT_MODIFIED)
        flags |= FOUNDRY_VCS_FILE_STATUS_MODIFIED_IN_TREE;

      if (status & GIT_STATUS_WT_DELETED)
        flags |= FOUNDRY_VCS_FILE_STATUS_DELETED_IN_TREE;

      if (status & GIT_STATUS_INDEX_NEW)
        flags |= FOUNDRY_VCS_FILE_STATUS_NEW_IN_STAGE;

      if (status & GIT_STATUS_INDEX_MODIFIED)
        flags |= FOUNDRY_VCS_FILE_STATUS_MODIFIED_IN_STAGE;

      if (status & GIT_STATUS_INDEX_DELETED)
        flags |= FOUNDRY_VCS_FILE_STATUS_DELETED_IN_STAGE;

      g_value_init (&value, FOUNDRY_TYPE_VCS_FILE_STATUS);
      g_value_set_flags (&value, flags);

      dex_promise_resolve (state->promise, &value);
    }

  g_clear_object (&state->self);
  g_clear_pointer (&state->path, g_free);
  dex_clear (&state->promise);
  g_free (state);
}

DexFuture *
_foundry_git_repository_query_file_status (FoundryGitRepository *self,
                                           GFile                *file)
{
  QueryFileStatus *state;
  DexPromise *promise;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));
  dex_return_error_if_fail (G_IS_FILE (file));

  if (!g_file_has_prefix (file, self->workdir))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "Not found");

  promise = dex_promise_new ();

  state = g_new0 (QueryFileStatus, 1);
  state->self = g_object_ref (self);
  state->path = g_file_get_relative_path (self->workdir, file);
  state->promise = dex_ref (promise);

  dex_scheduler_push (dex_thread_pool_scheduler_get_default (),
                      foundry_git_repository_query_file_status_worker,
                      state);

  return DEX_FUTURE (promise);
}

static DexFuture *
foundry_git_repository_list_status_thread (gpointer data)
{
  const char *git_dir = data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_status_list) status_list = NULL;
  git_status_options opts = GIT_STATUS_OPTIONS_INIT;

  g_assert (git_dir != NULL);

  FOUNDRY_TRACE_SCOPE_FUNC ();

  if (git_repository_open (&repository, git_dir) != 0)
    return foundry_git_reject_last_error ();

  opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
  opts.flags = (GIT_STATUS_OPT_INCLUDE_UNTRACKED |
                GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
                GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
                GIT_STATUS_OPT_SORT_CASE_SENSITIVELY);

  if (git_status_list_new (&status_list, repository, &opts) != 0)
    return foundry_git_reject_last_error ();

  return dex_future_new_take_object (_foundry_git_status_list_new (g_steal_pointer (&status_list)));
}

DexFuture *
_foundry_git_repository_list_status (FoundryGitRepository *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-list-status]",
                                 foundry_git_repository_list_status_thread,
                                 g_strdup (self->git_dir),
                                 g_free);
}

typedef struct _Stage
{
  FoundryGitRepository *self;
  FoundryGitStatusEntry *entry;
  GBytes *contents;
  char *git_dir;
} Stage;

static void
stage_free (Stage *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->entry);
  g_clear_pointer (&state->contents, g_bytes_unref);
  g_clear_pointer (&state->git_dir, g_free);
  g_free (state);
}

static DexFuture *
foundry_git_repository_stage_entry_thread (gpointer data)
{
  Stage *state = data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_index) index = NULL;
  g_autofree char *path = NULL;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_GIT_REPOSITORY (state->self));
  g_assert (FOUNDRY_IS_GIT_STATUS_ENTRY (state->entry));

  FOUNDRY_TRACE_SCOPE_FUNC ();

  path = foundry_git_status_entry_dup_path (state->entry);

  if (git_repository_open (&repository, state->git_dir) != 0)
    return foundry_git_reject_last_error ();

  if (git_repository_index (&index, repository) != 0)
    return foundry_git_reject_last_error ();

  if (state->contents == NULL)
    {
      if (git_index_add_bypath (index, path) != 0)
        return foundry_git_reject_last_error ();
    }
  else
    {
      git_index_entry entry;
      git_oid blob_oid;
      const char *buf;
      gsize buf_len;

      buf = g_bytes_get_data (state->contents, &buf_len);

      if (git_blob_create_from_buffer (&blob_oid, repository, buf, buf_len) != 0)
        return foundry_git_reject_last_error ();

      entry = (git_index_entry) {
        .mode = GIT_FILEMODE_BLOB,
        .id = blob_oid,
        .path = path,
      };

      if (git_index_add (index, &entry) != 0)
        return foundry_git_reject_last_error ();
    }

  if (git_index_write (index) != 0)
    return foundry_git_reject_last_error ();

  return dex_future_new_true ();
}

DexFuture *
_foundry_git_repository_stage_entry (FoundryGitRepository  *self,
                                     FoundryGitStatusEntry *entry,
                                     GBytes                *contents)
{
  Stage *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));

  state = g_new0 (Stage, 1);
  state->self = g_object_ref (self);
  state->git_dir = g_strdup (self->git_dir);
  state->entry = g_object_ref (entry);
  state->contents = contents ? g_bytes_ref (contents) : NULL;

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-stage-entry]",
                                 foundry_git_repository_stage_entry_thread,
                                 state,
                                 (GDestroyNotify) stage_free);
}

static DexFuture *
foundry_git_repository_unstage_entry_thread (gpointer data)
{
  FoundryPair *pair = data;
  FoundryGitRepository *self = FOUNDRY_GIT_REPOSITORY (pair->first);
  FoundryGitStatusEntry *entry = FOUNDRY_GIT_STATUS_ENTRY (pair->second);
  g_autofree char *path = foundry_git_status_entry_dup_path (entry);
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_index) index = NULL;
  git_oid head_oid;
  int err;

  FOUNDRY_TRACE_SCOPE_FUNC ();

  if (git_repository_open (&repository, self->git_dir) != 0)
    return foundry_git_reject_last_error ();

  if (git_repository_index (&index, repository) != 0)
    return foundry_git_reject_last_error ();

  if (git_reference_name_to_id (&head_oid, repository, "HEAD") != 0)
    {
      if (git_index_remove_bypath (index, path) != 0)
        return foundry_git_reject_last_error ();
    }
  else
    {
      g_autoptr(git_tree_entry) tree_entry = NULL;
      g_autoptr(git_reference) head_ref = NULL;
      g_autoptr(git_commit) head_commit = NULL;
      g_autoptr(git_tree) head_tree = NULL;

      if (git_repository_head (&head_ref, repository) != 0)
        return foundry_git_reject_last_error ();

      if (git_reference_peel ((git_object **)&head_commit, head_ref, GIT_OBJECT_COMMIT) != 0)
        return foundry_git_reject_last_error ();

      if (git_commit_tree (&head_tree, head_commit) != 0)
        return foundry_git_reject_last_error ();

      if ((err = git_tree_entry_bypath (&tree_entry, head_tree, path)))
        {
          if (err != GIT_ENOTFOUND)
            return foundry_git_reject_last_error ();

          if (git_index_remove_bypath (index, path) != 0)
            return foundry_git_reject_last_error ();
        }
      else
        {
          const git_index_entry ientry = {
            .path = path,
            .mode = git_tree_entry_filemode (tree_entry),
            .id = *git_tree_entry_id (tree_entry),
          };
          g_autoptr(git_blob) blob = NULL;
          const char *buf = NULL;
          gsize buf_len = 0;

          if (git_blob_lookup (&blob, repository, &ientry.id) == 0)
            {
              buf = git_blob_rawcontent (blob);
              buf_len = git_blob_rawsize (blob);
            }

          if (git_index_add_frombuffer (index, &ientry, buf, buf_len) != 0)
            return foundry_git_reject_last_error ();
        }
    }

  if (git_index_write (index) != 0)
    return foundry_git_reject_last_error ();

  return dex_future_new_true ();
}

DexFuture *
_foundry_git_repository_unstage_entry (FoundryGitRepository  *self,
                                       FoundryGitStatusEntry *entry)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-unstage-entry]",
                                 foundry_git_repository_unstage_entry_thread,
                                 foundry_pair_new (self, entry),
                                 (GDestroyNotify) foundry_pair_free);
}

typedef struct _Commit
{
  FoundryGitRepositoryPaths *paths;
  char *message;
  char *author_name;
  char *author_email;
} Commit;

static void
commit_free (Commit *state)
{
  g_clear_pointer (&state->paths, foundry_git_repository_paths_unref);
  g_clear_pointer (&state->message, g_free);
  g_clear_pointer (&state->author_name, g_free);
  g_clear_pointer (&state->author_email, g_free);
  g_free (state);
}

static DexFuture *
foundry_git_repository_commit_thread (gpointer data)
{
  Commit *state = data;
  g_autofree char *author_name = NULL;
  g_autofree char *author_email = NULL;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_config) config = NULL;
  g_autoptr(git_index) index = NULL;
  g_autoptr(git_tree) tree = NULL;
  g_autoptr(git_signature) author = NULL;
  g_autoptr(git_signature) committer = NULL;
  g_autoptr(git_object) parent = NULL;
  g_autoptr(git_commit) commit = NULL;
  g_autoptr(GError) error = NULL;
  git_oid tree_oid;
  git_oid commit_oid;
  int err;

  g_assert (state != NULL);
  g_assert (state->paths != NULL);
  g_assert (state->message != NULL);

  FOUNDRY_TRACE_SCOPE_FUNC ();

  if (!foundry_git_repository_paths_open (state->paths, &repository, &error))
    return dex_future_new_reject (error->domain, error->code, "%s", error->message);

  if (git_repository_config (&config, repository) != 0)
    return foundry_git_reject_last_error ();

  if (!g_set_str (&author_name, state->author_name))
    {
      g_autoptr(git_config_entry) entry = NULL;
      const char *real_name = g_get_real_name ();

      if (git_config_get_entry (&entry, config, "user.name") == 0)
        author_name = g_strdup (entry->value);
      else
        author_name = g_strdup (real_name ? real_name : g_get_user_name ());
    }

  if (!g_set_str (&author_email, state->author_email))
    {
      g_autoptr(git_config_entry) entry = NULL;

      if (git_config_get_entry (&entry, config, "user.email") == 0)
        author_email = g_strdup (entry->value);
      else
        author_email = g_strdup_printf ("%s@localhost", g_get_user_name ());
    }

  if (git_repository_index (&index, repository) != 0)
    return foundry_git_reject_last_error ();

  if (git_index_write_tree (&tree_oid, index) != 0)
    return foundry_git_reject_last_error ();

  if (git_tree_lookup (&tree, repository, &tree_oid) != 0)
    return foundry_git_reject_last_error ();

  if (git_signature_now (&author, author_name, author_email) != 0)
    return foundry_git_reject_last_error ();

  if (git_signature_dup (&committer, author) != 0)
    return foundry_git_reject_last_error ();

  if ((err = git_revparse_single (&parent, repository, "HEAD^{commit}")) != 0)
    {
      if (err != GIT_ENOTFOUND)
        return foundry_git_reject_last_error ();

      if (git_commit_create_v (&commit_oid, repository, "HEAD", author, committer, NULL, state->message, tree, 0) != 0)
        return foundry_git_reject_last_error ();
    }
  else
    {
      if (git_commit_create_v (&commit_oid, repository, "HEAD", author, committer, NULL, state->message, tree, 1, parent) != 0)
        return foundry_git_reject_last_error ();
    }

  if (git_commit_lookup (&commit, repository, &commit_oid) != 0)
    return foundry_git_reject_last_error ();

  return dex_future_new_take_object (_foundry_git_commit_new (g_steal_pointer (&commit),
                                                              (GDestroyNotify) git_commit_free,
                                                              foundry_git_repository_paths_ref (state->paths)));
}

DexFuture *
_foundry_git_repository_commit (FoundryGitRepository *self,
                                const char           *message,
                                const char           *author_name,
                                const char           *author_email)
{
  Commit *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));
  dex_return_error_if_fail (message != NULL);

  state = g_new0 (Commit, 1);
  state->paths = _foundry_git_repository_dup_paths (self);
  state->message = g_strdup (message);
  state->author_name = g_strdup (author_name);
  state->author_email = g_strdup (author_email);

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-commit]",
                                 foundry_git_repository_commit_thread,
                                 state,
                                 (GDestroyNotify) commit_free);
}

/**
 * _foundry_git_repository_create_monitor:
 * @self: a [class@Foundry.GitRepository]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   #FoundryGitMonitor
 */
DexFuture *
_foundry_git_repository_create_monitor (FoundryGitRepository *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));

  if (self->monitor == NULL)
    self->monitor = foundry_git_monitor_new (self->git_dir);

  return dex_ref (self->monitor);
}

typedef struct _QueryConfig
{
  FoundryGitRepository *self;
  char *key;
} QueryConfig;

static void
query_config_free (QueryConfig *state)
{
  g_clear_object (&state->self);
  g_clear_pointer (&state->key, g_free);
  g_free (state);
}

static DexFuture *
foundry_git_repository_query_config_thread (gpointer data)
{
  QueryConfig *state = data;
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(git_config) config = NULL;
  g_autoptr(git_config_entry) entry = NULL;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_GIT_REPOSITORY (state->self));
  g_assert (state->key != NULL);

  FOUNDRY_TRACE_SCOPE_FUNC ();

  locker = g_mutex_locker_new (&state->self->mutex);

  if (git_repository_config (&config, state->self->repository) != 0)
    return foundry_git_reject_last_error ();

  if (git_config_get_entry (&entry, config, state->key) != 0)
    return foundry_git_reject_last_error ();

  if (entry->value == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "Config key not found");

  return dex_future_new_take_string (g_strdup (entry->value));
}

/**
 * _foundry_git_repository_query_config:
 * @self: a [class@Foundry.GitRepository]
 * @key: the config key to query
 *
 * Queries a git configuration value by key from the repository.
 *
 * The method runs asynchronously in a background thread and returns a
 * [class@Dex.Future] that resolves to the config value as a string.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a string
 *   containing the config value, or rejects with an error if the key is not
 *   found or an error occurs
 *
 * Since: 1.1
 */
DexFuture *
_foundry_git_repository_query_config (FoundryGitRepository *self,
                                      const char           *key)
{
  QueryConfig *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));
  dex_return_error_if_fail (key != NULL);

  state = g_new0 (QueryConfig, 1);
  state->self = g_object_ref (self);
  state->key = g_strdup (key);

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-query-config]",
                                 foundry_git_repository_query_config_thread,
                                 state,
                                 (GDestroyNotify) query_config_free);
}

static DexFuture *
foundry_git_repository_stash_thread (gpointer data)
{
  Stash *state = data;
  g_autofree char *author_name = NULL;
  g_autofree char *author_email = NULL;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_config) config = NULL;
  g_autoptr(git_signature) stasher = NULL;
  g_autoptr(git_commit) commit = NULL;
  g_autoptr(GError) error = NULL;
  git_stash_flags flags = GIT_STASH_DEFAULT;
  git_oid stash_oid;

  g_assert (state != NULL);
  g_assert (state->paths != NULL);

  FOUNDRY_TRACE_SCOPE_FUNC ();

  if (!foundry_git_repository_paths_open (state->paths, &repository, &error))
    return dex_future_new_reject (error->domain, error->code, "%s", error->message);

  if (git_repository_config (&config, repository) != 0)
    return foundry_git_reject_last_error ();

  {
    g_autoptr(git_config_entry) entry = NULL;
    const char *real_name = g_get_real_name ();

    if (git_config_get_entry (&entry, config, "user.name") == 0)
      author_name = g_strdup (entry->value);
    else
      author_name = g_strdup (real_name ? real_name : g_get_user_name ());
  }

  {
    g_autoptr(git_config_entry) entry = NULL;

    if (git_config_get_entry (&entry, config, "user.email") == 0)
      author_email = g_strdup (entry->value);
    else
      author_email = g_strdup_printf ("%s@localhost", g_get_user_name ());
  }

  if (git_signature_now (&stasher, author_name, author_email) != 0)
    return foundry_git_reject_last_error ();

  if (state->include_untracked)
    flags |= GIT_STASH_INCLUDE_UNTRACKED;

  if (git_stash_save (&stash_oid, repository, stasher, NULL, flags) != 0)
    return foundry_git_reject_last_error ();

  if (git_commit_lookup (&commit, repository, &stash_oid) != 0)
    return foundry_git_reject_last_error ();

  return dex_future_new_take_object (_foundry_git_commit_new (g_steal_pointer (&commit),
                                                              (GDestroyNotify) git_commit_free,
                                                              foundry_git_repository_paths_ref (state->paths)));
}

static void
stash_free (gpointer data)
{
  Stash *state = data;

  if (state != NULL)
    {
      g_clear_pointer (&state->paths, foundry_git_repository_paths_unref);
      g_free (state);
    }
}

DexFuture *
_foundry_git_repository_stash (FoundryGitRepository *self,
                               gboolean             include_untracked)
{
  Stash *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));

  state = g_new0 (Stash, 1);
  state->paths = _foundry_git_repository_dup_paths (self);
  state->include_untracked = include_untracked;

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-stash]",
                                 foundry_git_repository_stash_thread,
                                 state,
                                 stash_free);
}

static DexFuture *
foundry_git_repository_discard_changes_thread (gpointer data)
{
  FoundryGitRepositoryPaths *paths = data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_object) head = NULL;
  g_autoptr(GError) error = NULL;
  git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;

  g_assert (paths != NULL);

  FOUNDRY_TRACE_SCOPE_FUNC ();

  if (!foundry_git_repository_paths_open (paths, &repository, &error))
    return dex_future_new_reject (error->domain, error->code, "%s", error->message);

  if (git_revparse_single (&head, repository, "HEAD") != 0)
    return foundry_git_reject_last_error ();

  checkout_opts.checkout_strategy = (GIT_CHECKOUT_FORCE | GIT_CHECKOUT_REMOVE_UNTRACKED);

  if (git_reset (repository, head, GIT_RESET_HARD, &checkout_opts) != 0)
    return foundry_git_reject_last_error ();

  return dex_future_new_true ();
}

DexFuture *
_foundry_git_repository_discard_changes (FoundryGitRepository *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_REPOSITORY (self));

  return dex_thread_pool_submit (_foundry_git_get_thread_pool (),
                                 "[git-discard-changes]",
                                 foundry_git_repository_discard_changes_thread,
                                 _foundry_git_repository_dup_paths (self),
                                 (GDestroyNotify) foundry_git_repository_paths_unref);
}
