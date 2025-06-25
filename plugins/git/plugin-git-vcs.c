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
#include "plugin-git-vcs-branch.h"
#include "plugin-git-vcs-file.h"
#include "plugin-git-vcs-remote.h"
#include "plugin-git-vcs-tag.h"

struct _PluginGitVcs
{
  FoundryVcs      parent_instance;
  git_repository *repository;
  char           *branch_name;
  GFile          *workdir;
};

G_DEFINE_FINAL_TYPE (PluginGitVcs, plugin_git_vcs, FOUNDRY_TYPE_VCS)

static DexFuture *
wrap_last_error (void)
{
  const git_error *error = git_error_last ();
  const char *message = error->message ? error->message : "Unknown error";

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_FAILED,
                                "%s", message);
}

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

static DexFuture *
plugin_git_vcs_list_branches (FoundryVcs *vcs)
{
  PluginGitVcs *self = (PluginGitVcs *)vcs;
  g_autoptr(git_branch_iterator) iter = NULL;
  g_autoptr(GListStore) store = NULL;

  g_assert (PLUGIN_IS_GIT_VCS (self));

  if (git_branch_iterator_new (&iter, self->repository, GIT_BRANCH_ALL) < 0)
    return wrap_last_error ();

  store = g_list_store_new (FOUNDRY_TYPE_VCS_BRANCH);

  for (;;)
    {
      g_autoptr(PluginGitVcsBranch) branch = NULL;
      g_autoptr(git_reference) ref = NULL;
      git_branch_t branch_type;

      if (git_branch_next (&ref, &branch_type, iter) != 0)
        break;

      branch = plugin_git_vcs_branch_new (g_steal_pointer (&ref), branch_type);
      g_list_store_append (store, branch);
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static DexFuture *
plugin_git_vcs_list_tags (FoundryVcs *vcs)
{
  PluginGitVcs *self = (PluginGitVcs *)vcs;
  g_autoptr(git_reference_iterator) iter = NULL;
  g_autoptr(GListStore) store = NULL;

  g_assert (PLUGIN_IS_GIT_VCS (self));

  if (git_reference_iterator_new (&iter, self->repository) < 0)
    return wrap_last_error ();

  store = g_list_store_new (FOUNDRY_TYPE_VCS_TAG);

  for (;;)
    {
      g_autoptr(git_reference) ref = NULL;
      const char *name;

      if (git_reference_next (&ref, iter) != 0)
        break;

      if ((name = git_reference_name (ref)))
        {
          if (g_str_has_prefix (name, "refs/tags/") ||
              strstr (name, "/tags/") != NULL)
            {
              g_autoptr(PluginGitVcsTag) tag = NULL;
              tag = plugin_git_vcs_tag_new (g_steal_pointer (&ref));
              g_list_store_append (store, tag);
            }
        }
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static DexFuture *
plugin_git_vcs_find_file (FoundryVcs *vcs,
                          GFile      *file)
{
  PluginGitVcs *self = (PluginGitVcs *)vcs;
  g_autofree char *relative_path = NULL;

  dex_return_error_if_fail (PLUGIN_IS_GIT_VCS (self));
  dex_return_error_if_fail (G_IS_FILE (file));

  if (!g_file_has_prefix (file, self->workdir))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "File does not exist in working tree");

  relative_path = g_file_get_relative_path (self->workdir, file);

  g_assert (relative_path != NULL);

  return dex_future_new_take_object (plugin_git_vcs_file_new (self->workdir, relative_path));
}

static DexFuture *
plugin_git_vcs_list_remotes (FoundryVcs *vcs)
{
  PluginGitVcs *self = (PluginGitVcs *)vcs;
  g_autoptr(GListStore) store = NULL;
  g_auto(git_strarray) remotes = {0};

  dex_return_error_if_fail (PLUGIN_IS_GIT_VCS (self));
  dex_return_error_if_fail (self->repository != NULL);

  if (git_remote_list (&remotes, self->repository) != 0)
    return wrap_last_error ();

  store = g_list_store_new (FOUNDRY_TYPE_VCS_REMOTE);

  for (gsize i = 0; i < remotes.count; i++)
    {
      g_autoptr(FoundryVcsRemote) remote = plugin_git_vcs_remote_new (remotes.strings[i]);

      g_list_store_append (store, remote);
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static int
credentials_cb (git_cred     **out,
                const char    *url,
                const char    *username_from_url,
                unsigned int   allowed_types,
                void          *payload)
{
  if (allowed_types & GIT_CREDTYPE_SSH_KEY)
    return git_cred_ssh_key_from_agent (out, username_from_url);

  if (allowed_types & GIT_CREDTYPE_DEFAULT)
    return git_cred_default_new (out);

  /* TODO: We don't have user/pass credentials here and that might be something
   *       we want someday. However, that will require a way to request that
   *       information from the UI (say libfoundry-gtk) through an abstracted
   *       auth agent.
   */

  return 1;
}

typedef struct _Fetch
{
  char             *git_dir;
  char             *remote_name;
  FoundryOperation *operation;
} Fetch;

static void
fetch_free (Fetch *state)
{
  g_clear_pointer (&state->git_dir, g_free);
  g_clear_pointer (&state->remote_name, g_free);
  g_clear_object (&state->operation);
  g_free (state);
}

static DexFuture *
plugin_git_vcs_fetch_thread (gpointer user_data)
{
  Fetch *state = user_data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_remote) remote = NULL;
  git_fetch_options fetch_opts;

  g_assert (state != NULL);
  g_assert (state->git_dir != NULL);
  g_assert (state->remote_name != NULL);
  g_assert (FOUNDRY_IS_OPERATION (state->operation));

  /* TODO: update operation with callbacks */

  if (git_repository_open (&repository, state->git_dir) != 0)
    return wrap_last_error ();

  if (git_remote_lookup (&remote, repository, state->remote_name) != 0)
    return wrap_last_error ();

  git_fetch_options_init (&fetch_opts, GIT_FETCH_OPTIONS_VERSION);
  git_remote_init_callbacks (&fetch_opts.callbacks, GIT_REMOTE_CALLBACKS_VERSION);

  fetch_opts.download_tags = GIT_REMOTE_DOWNLOAD_TAGS_ALL;
  fetch_opts.update_fetchhead = 1;
  fetch_opts.callbacks.credentials = credentials_cb;

  if (git_remote_fetch (remote, NULL, &fetch_opts, NULL) != 0)
    return wrap_last_error ();

  return dex_future_new_true ();
}

static DexFuture *
plugin_git_vcs_fetch (FoundryVcs       *vcs,
                      FoundryVcsRemote *remote,
                      FoundryOperation *operation)
{
  PluginGitVcs *self = (PluginGitVcs *)vcs;
  Fetch *state;

  dex_return_error_if_fail (PLUGIN_IS_GIT_VCS (self));
  dex_return_error_if_fail (PLUGIN_IS_GIT_VCS_REMOTE (remote));
  dex_return_error_if_fail (FOUNDRY_IS_OPERATION (operation));
  dex_return_error_if_fail (self->repository != NULL);

  state = g_new0 (Fetch, 1);
  state->remote_name = foundry_vcs_remote_dup_name (remote);
  state->git_dir = g_strdup (git_repository_path (self->repository));
  state->operation = g_object_ref (operation);

  return dex_thread_spawn ("[git-fetch]",
                           plugin_git_vcs_fetch_thread,
                           state,
                           (GDestroyNotify) fetch_free);
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

  vcs_class->blame = plugin_git_vcs_blame;
  vcs_class->dup_branch_name = plugin_git_vcs_dup_branch_name;
  vcs_class->dup_id = plugin_git_vcs_dup_id;
  vcs_class->dup_name = plugin_git_vcs_dup_name;
  vcs_class->fetch = plugin_git_vcs_fetch;
  vcs_class->find_file = plugin_git_vcs_find_file;
  vcs_class->get_priority = plugin_git_vcs_get_priority;
  vcs_class->is_file_ignored = plugin_git_vcs_is_file_ignored;
  vcs_class->is_ignored = plugin_git_vcs_is_ignored;
  vcs_class->list_branches = plugin_git_vcs_list_branches;
  vcs_class->list_files = plugin_git_vcs_list_files;
  vcs_class->list_remotes = plugin_git_vcs_list_remotes;
  vcs_class->list_tags = plugin_git_vcs_list_tags;
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
