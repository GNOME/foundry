/* foundry-git-vcs.c
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

#include <libssh2.h>

#include "foundry-auth-prompt.h"
#include "foundry-git-autocleanups.h"
#include "foundry-git-file-list.h"
#include "foundry-git-error.h"
#include "foundry-git-blame-private.h"
#include "foundry-git-branch-private.h"
#include "foundry-git-file-private.h"
#include "foundry-git-remote-private.h"
#include "foundry-git-tag-private.h"
#include "foundry-git-vcs-private.h"
#include "foundry-operation.h"
#include "foundry-util.h"

struct _FoundryGitVcs
{
  FoundryVcs      parent_instance;
  git_repository *repository;
  char           *branch_name;
  GFile          *workdir;
};

G_DEFINE_FINAL_TYPE (FoundryGitVcs, foundry_git_vcs, FOUNDRY_TYPE_VCS)

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
foundry_git_vcs_get_priority (FoundryVcs *vcs)
{
  return 100;
}

static char *
foundry_git_vcs_dup_id (FoundryVcs *vcs)
{
  return g_strdup ("git");
}

static char *
foundry_git_vcs_dup_name (FoundryVcs *vcs)
{
  return g_strdup ("Git");
}

static char *
foundry_git_vcs_dup_branch_name (FoundryVcs *vcs)
{
  FoundryGitVcs *self = FOUNDRY_GIT_VCS (vcs);

  return g_strdup (self->branch_name);
}

static gboolean
foundry_git_vcs_is_ignored (FoundryVcs *vcs,
                           const char *relative_path)
{
  FoundryGitVcs *self = (FoundryGitVcs *)vcs;
  gboolean ignored = FALSE;

  g_assert (FOUNDRY_IS_GIT_VCS (vcs));
  g_assert (relative_path != NULL);

  if (git_ignore_path_is_ignored (&ignored, self->repository, relative_path) == GIT_OK)
    return ignored;

  return FALSE;
}

static gboolean
foundry_git_vcs_is_file_ignored (FoundryVcs *vcs,
                                GFile      *file)
{
  FoundryGitVcs *self = (FoundryGitVcs *)vcs;
  g_autofree char *relative_path = NULL;
  gboolean ignored = FALSE;

  g_assert (FOUNDRY_IS_GIT_VCS (vcs));
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
foundry_git_vcs_list_files (FoundryVcs *vcs)
{
  FoundryGitVcs *self = (FoundryGitVcs *)vcs;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(git_index) index = NULL;

  g_assert (FOUNDRY_IS_GIT_VCS (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  /* It appears that you can use the git_index independently of the
   * git_repository and that their lifetimes do not need to be
   * controlled together.
   *
   * This allows us to make the file list objects on demand as they
   * are requested from the model.
   */
  if (git_repository_index (&index, self->repository) == 0)
    return dex_future_new_take_object (foundry_git_file_list_new (context,
                                                                  self->workdir,
                                                                  g_steal_pointer (&index)));

  return foundry_future_new_disposed ();
}

typedef struct _Blame
{
  char   *git_dir;
  char   *relative_path;
  GBytes *bytes;
} Blame;

static void
blame_free (Blame *state)
{
  g_clear_pointer (&state->git_dir, g_free);
  g_clear_pointer (&state->relative_path, g_free);
  g_clear_pointer (&state->bytes, g_bytes_unref);
  g_free (state);
}

static DexFuture *
foundry_git_vcs_blame_thread (gpointer user_data)
{
  Blame *state = user_data;
  g_autoptr(git_blame) blame = NULL;
  g_autoptr(git_blame) bytes_blame = NULL;
  g_autoptr(git_repository) repository = NULL;

  g_assert (state->git_dir != NULL);
  g_assert (state->relative_path != NULL);

  if (git_repository_open (&repository, state->git_dir) != 0)
    return wrap_last_error ();

  if (git_blame_file (&blame, repository, state->relative_path, NULL) != 0)
    return wrap_last_error ();

  if (state->bytes != NULL)
    {
      gconstpointer data = g_bytes_get_data (state->bytes, NULL);
      gsize size = g_bytes_get_size (state->bytes);

      if (git_blame_buffer (&bytes_blame, blame, data, size) != 0)
        return wrap_last_error ();
    }

  return dex_future_new_take_object (foundry_git_blame_new (g_steal_pointer (&blame),
                                                            g_steal_pointer (&bytes_blame)));
}

static DexFuture *
foundry_git_vcs_blame (FoundryVcs     *vcs,
                       FoundryVcsFile *file,
                       GBytes         *bytes)
{
  FoundryGitVcs *self = (FoundryGitVcs *)vcs;
  Blame *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self));
  dex_return_error_if_fail (FOUNDRY_IS_VCS_FILE (file));

  state = g_new0 (Blame, 1);
  state->git_dir = g_strdup (git_repository_path (self->repository));
  state->relative_path = foundry_vcs_file_dup_relative_path (file);
  state->bytes = bytes ? g_bytes_ref (bytes) : NULL;

  return dex_thread_spawn ("[git-blame]",
                           foundry_git_vcs_blame_thread,
                           state,
                           (GDestroyNotify) blame_free);
}

static DexFuture *
foundry_git_vcs_list_branches (FoundryVcs *vcs)
{
  FoundryGitVcs *self = (FoundryGitVcs *)vcs;
  g_autoptr(git_branch_iterator) iter = NULL;
  g_autoptr(GListStore) store = NULL;

  g_assert (FOUNDRY_IS_GIT_VCS (self));

  if (git_branch_iterator_new (&iter, self->repository, GIT_BRANCH_ALL) < 0)
    return wrap_last_error ();

  store = g_list_store_new (FOUNDRY_TYPE_VCS_BRANCH);

  for (;;)
    {
      g_autoptr(FoundryGitBranch) branch = NULL;
      g_autoptr(git_reference) ref = NULL;
      git_branch_t branch_type;

      if (git_branch_next (&ref, &branch_type, iter) != 0)
        break;

      if ((branch = foundry_git_branch_new (ref, branch_type)))
        g_list_store_append (store, branch);
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static DexFuture *
foundry_git_vcs_list_tags (FoundryVcs *vcs)
{
  FoundryGitVcs *self = (FoundryGitVcs *)vcs;
  g_autoptr(git_reference_iterator) iter = NULL;
  g_autoptr(GListStore) store = NULL;

  g_assert (FOUNDRY_IS_GIT_VCS (self));

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
              g_autoptr(FoundryGitTag) tag = NULL;

              if ((tag = foundry_git_tag_new (ref)))
                g_list_store_append (store, tag);
            }
        }
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static DexFuture *
foundry_git_vcs_find_remote (FoundryVcs *vcs,
                             const char *name)
{
  FoundryGitVcs *self = (FoundryGitVcs *)vcs;
  g_autoptr(git_remote) remote = NULL;

  g_assert (FOUNDRY_IS_GIT_VCS (self));
  g_assert (name != NULL);

  if (git_remote_lookup (&remote, self->repository, name) == 0)
    return dex_future_new_take_object (foundry_git_remote_new (self, name, remote));

  if (git_remote_create_anonymous (&remote, self->repository, name) == 0)
    return dex_future_new_take_object (foundry_git_remote_new (self, name, remote));

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_FOUND,
                                "Not found");
}

static DexFuture *
foundry_git_vcs_find_file (FoundryVcs *vcs,
                           GFile      *file)
{
  FoundryGitVcs *self = (FoundryGitVcs *)vcs;
  g_autofree char *relative_path = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self));
  dex_return_error_if_fail (G_IS_FILE (file));

  if (!g_file_has_prefix (file, self->workdir))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "File does not exist in working tree");

  relative_path = g_file_get_relative_path (self->workdir, file);

  g_assert (relative_path != NULL);

  return dex_future_new_take_object (foundry_git_file_new (self->workdir, relative_path));
}

static DexFuture *
foundry_git_vcs_list_remotes (FoundryVcs *vcs)
{
  FoundryGitVcs *self = (FoundryGitVcs *)vcs;
  g_autoptr(GListStore) store = NULL;
  g_auto(git_strarray) remotes = {0};

  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self));
  dex_return_error_if_fail (self->repository != NULL);

  if (git_remote_list (&remotes, self->repository) != 0)
    return wrap_last_error ();

  store = g_list_store_new (FOUNDRY_TYPE_VCS_REMOTE);

  for (gsize i = 0; i < remotes.count; i++)
    {
      g_autoptr(FoundryVcsRemote) vcs_remote = NULL;
      g_autoptr(git_remote) remote = NULL;

      if (git_remote_lookup (&remote, self->repository, remotes.strings[i]) != 0)
        continue;

      vcs_remote = foundry_git_remote_new (self, remotes.strings[i], g_steal_pointer (&remote));

      g_list_store_append (store, vcs_remote);
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

typedef struct _CallbackState
{
  FoundryContext *context;
  guint tried;
} CallbackState;

static void
ssh_interactive_prompt (const char                            *name,
                        int                                    name_len,
                        const char                            *instruction,
                        int                                    instruction_len,
                        int                                    num_prompts,
                        const LIBSSH2_USERAUTH_KBDINT_PROMPT  *prompts,
                        LIBSSH2_USERAUTH_KBDINT_RESPONSE      *responses,
                        void                                 **abstract)
{
  CallbackState *state = *abstract;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_CONTEXT (state->context));

  for (int i = 0; i < num_prompts; i++)
    {
      g_autoptr(FoundryAuthPromptBuilder) builder = NULL;
      g_autoptr(FoundryAuthPrompt) prompt = NULL;
      g_autofree char *instruction_copy = g_strndup (instruction, instruction_len);

      builder = foundry_auth_prompt_builder_new (state->context);
      foundry_auth_prompt_builder_set_title (builder, instruction);

      for (int j = 0; j < num_prompts; j++)
        {
          const char *prompt_text = (const char *)prompts[j].text;
          gboolean hidden = !prompts[j].echo;

          foundry_auth_prompt_builder_add_param (builder, prompt_text, prompt_text, NULL, hidden);
        }

      prompt = foundry_auth_prompt_builder_end (builder);

      if (!dex_thread_wait_for (foundry_auth_prompt_query (prompt), NULL))
        return;

      for (int j = 0; j < num_prompts; j++)
        {
          const char *prompt_text = (const char *)prompts[j].text;
          g_autofree char *value = foundry_auth_prompt_dup_prompt_value (prompt, prompt_text);

          responses[j].text = strdup (value);
          responses[j].length = value ? strlen (value) : 0;
        }
    }
}

static int
credentials_cb (git_cred     **out,
                const char    *url,
                const char    *username_from_url,
                unsigned int   allowed_types,
                void          *payload)
{
  CallbackState *state = payload;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_CONTEXT (state->context));

  allowed_types &= ~state->tried;

  if (allowed_types & GIT_CREDENTIAL_USERNAME)
    {
      state->tried |= GIT_CREDENTIAL_USERNAME;
      return git_cred_username_new (out,
                                    username_from_url ?
                                      username_from_url :
                                      g_get_user_name ());
    }

  if (allowed_types & GIT_CREDENTIAL_SSH_KEY)
    {
      state->tried |= GIT_CREDENTIAL_SSH_KEY;
      return git_cred_ssh_key_from_agent (out,
                                          username_from_url ?
                                            username_from_url :
                                            g_get_user_name ());
    }

  if (allowed_types & GIT_CREDENTIAL_DEFAULT)
    {
      state->tried |= GIT_CREDENTIAL_DEFAULT;
      return git_cred_default_new (out);
    }

  if (allowed_types & GIT_CREDENTIAL_SSH_INTERACTIVE)
    {
      g_autofree char *username = g_strdup (username_from_url);

      state->tried |= GIT_CREDENTIAL_SSH_INTERACTIVE;

      if (username == NULL)
        {
          g_autoptr(FoundryAuthPromptBuilder) builder = NULL;
          g_autoptr(FoundryAuthPrompt) prompt = NULL;

          builder = foundry_auth_prompt_builder_new (state->context);
          foundry_auth_prompt_builder_set_title (builder, _("Credentials"));
          foundry_auth_prompt_builder_add_param (builder,
                                                 "username",
                                                 _("Username"),
                                                 g_get_user_name (),
                                                 FALSE);

          prompt = foundry_auth_prompt_builder_end (builder);

          if (!dex_thread_wait_for (foundry_auth_prompt_query (prompt), NULL))
            return GIT_PASSTHROUGH;

          g_set_str (&username, foundry_auth_prompt_get_value (prompt, "username"));
        }

      return git_cred_ssh_interactive_new (out, username, ssh_interactive_prompt, state);
    }

  if (allowed_types & GIT_CREDENTIAL_USERPASS_PLAINTEXT)
    {
      g_autoptr(FoundryAuthPromptBuilder) builder = NULL;
      g_autoptr(FoundryAuthPrompt) prompt = NULL;

      state->tried |= GIT_CREDENTIAL_USERPASS_PLAINTEXT;

      builder = foundry_auth_prompt_builder_new (state->context);
      foundry_auth_prompt_builder_set_title (builder, _("Credentials"));
      foundry_auth_prompt_builder_add_param (builder,
                                             "username",
                                             _("Username"),
                                             username_from_url ? username_from_url : g_get_user_name (),
                                             FALSE);
      foundry_auth_prompt_builder_add_param (builder,
                                             "password",
                                             _("Password"),
                                             NULL,
                                             TRUE);

      prompt = foundry_auth_prompt_builder_end (builder);

      if (!dex_thread_wait_for (foundry_auth_prompt_query (prompt), NULL))
        return GIT_PASSTHROUGH;

      return git_cred_userpass_plaintext_new (out,
                                              foundry_auth_prompt_get_value (prompt, "username"),
                                              foundry_auth_prompt_get_value (prompt, "password"));
    }

  return GIT_PASSTHROUGH;
}

typedef struct _Fetch
{
  char             *git_dir;
  char             *remote_name;
  FoundryOperation *operation;
  FoundryContext   *context;
} Fetch;

static void
fetch_free (Fetch *state)
{
  g_clear_pointer (&state->git_dir, g_free);
  g_clear_pointer (&state->remote_name, g_free);
  g_clear_object (&state->operation);
  g_clear_object (&state->context);
  g_free (state);
}

static DexFuture *
foundry_git_vcs_fetch_thread (gpointer user_data)
{
  Fetch *state = user_data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_remote) remote = NULL;
  git_fetch_options fetch_opts;
  CallbackState callback_state = {0};

  g_assert (state != NULL);
  g_assert (state->git_dir != NULL);
  g_assert (state->remote_name != NULL);
  g_assert (FOUNDRY_IS_OPERATION (state->operation));

  /* TODO: update operation with callbacks */

  if (git_repository_open (&repository, state->git_dir) != 0)
    return wrap_last_error ();

  if (git_remote_lookup (&remote, repository, state->remote_name) != 0 &&
      git_remote_create_anonymous (&remote, repository, state->remote_name) != 0)
    return wrap_last_error ();

  git_fetch_options_init (&fetch_opts, GIT_FETCH_OPTIONS_VERSION);
  git_remote_init_callbacks (&fetch_opts.callbacks, GIT_REMOTE_CALLBACKS_VERSION);

  fetch_opts.download_tags = GIT_REMOTE_DOWNLOAD_TAGS_ALL;
  fetch_opts.update_fetchhead = 1;
  fetch_opts.callbacks.credentials = credentials_cb;
  fetch_opts.callbacks.payload = &callback_state;

  callback_state.context = state->context;
  callback_state.tried = 0;

  if (git_remote_fetch (remote, NULL, &fetch_opts, NULL) != 0)
    return wrap_last_error ();

  return dex_future_new_true ();
}

static DexFuture *
foundry_git_vcs_fetch (FoundryVcs       *vcs,
                       FoundryVcsRemote *remote,
                       FoundryOperation *operation)
{
  FoundryGitVcs *self = (FoundryGitVcs *)vcs;
  Fetch *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self));
  dex_return_error_if_fail (FOUNDRY_IS_GIT_REMOTE (remote));
  dex_return_error_if_fail (FOUNDRY_IS_OPERATION (operation));
  dex_return_error_if_fail (self->repository != NULL);

  state = g_new0 (Fetch, 1);
  state->remote_name = foundry_vcs_remote_dup_name (remote);
  state->git_dir = g_strdup (git_repository_path (self->repository));
  state->operation = g_object_ref (operation);
  state->context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (vcs));

  return dex_thread_spawn ("[git-fetch]",
                           foundry_git_vcs_fetch_thread,
                           state,
                           (GDestroyNotify) fetch_free);
}

static void
foundry_git_vcs_finalize (GObject *object)
{
  FoundryGitVcs *self = (FoundryGitVcs *)object;

  g_clear_pointer (&self->repository, git_repository_free);
  g_clear_pointer (&self->branch_name, g_free);
  g_clear_object (&self->workdir);

  G_OBJECT_CLASS (foundry_git_vcs_parent_class)->finalize (object);
}

static void
foundry_git_vcs_class_init (FoundryGitVcsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsClass *vcs_class = FOUNDRY_VCS_CLASS (klass);

  object_class->finalize = foundry_git_vcs_finalize;

  vcs_class->blame = foundry_git_vcs_blame;
  vcs_class->dup_branch_name = foundry_git_vcs_dup_branch_name;
  vcs_class->dup_id = foundry_git_vcs_dup_id;
  vcs_class->dup_name = foundry_git_vcs_dup_name;
  vcs_class->fetch = foundry_git_vcs_fetch;
  vcs_class->find_file = foundry_git_vcs_find_file;
  vcs_class->find_remote = foundry_git_vcs_find_remote;
  vcs_class->get_priority = foundry_git_vcs_get_priority;
  vcs_class->is_file_ignored = foundry_git_vcs_is_file_ignored;
  vcs_class->is_ignored = foundry_git_vcs_is_ignored;
  vcs_class->list_branches = foundry_git_vcs_list_branches;
  vcs_class->list_files = foundry_git_vcs_list_files;
  vcs_class->list_remotes = foundry_git_vcs_list_remotes;
  vcs_class->list_tags = foundry_git_vcs_list_tags;
}

static void
foundry_git_vcs_init (FoundryGitVcs *self)
{
}

static DexFuture *
_foundry_git_vcs_load_fiber (gpointer data)
{
  FoundryGitVcs *self = data;
  g_autoptr(git_reference) head = NULL;

  g_assert (FOUNDRY_IS_GIT_VCS (self));

  if (git_repository_head (&head, self->repository) == GIT_OK)
    {
      const char *branch_name = NULL;

      if (git_branch_name (&branch_name, head) == GIT_OK)
        g_set_str (&self->branch_name, branch_name);
    }

  return dex_future_new_take_object (g_object_ref (self));
}

static DexFuture *
_foundry_git_vcs_load (FoundryGitVcs *self)
{
  g_assert (FOUNDRY_IS_GIT_VCS (self));

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              _foundry_git_vcs_load_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

DexFuture *
_foundry_git_vcs_new (FoundryContext *context,
                      git_repository *repository)
{
  FoundryGitVcs *self;
  const char *workdir;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (repository != NULL, NULL);

  workdir = git_repository_workdir (repository);

  self = g_object_new (FOUNDRY_TYPE_GIT_VCS,
                       "context", context,
                       NULL);

  self->repository = g_steal_pointer (&repository);
  self->workdir = g_file_new_for_path (workdir);

  return _foundry_git_vcs_load (self);
}
