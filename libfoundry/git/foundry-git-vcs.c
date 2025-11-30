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

#include "ssh-agent-sign.h"

#include "foundry-auth-provider.h"
#include "foundry-git-autocleanups.h"
#include "foundry-git-file.h"
#include "foundry-git-file-list-private.h"
#include "foundry-git-error.h"
#include "foundry-git-blame-private.h"
#include "foundry-git-branch-private.h"
#include "foundry-git-file-private.h"
#include "foundry-git-reference-private.h"
#include "foundry-git-repository-private.h"
#include "foundry-git-repository-paths-private.h"
#include "foundry-git-remote-private.h"
#include "foundry-git-tag-private.h"
#include "foundry-git-tree.h"
#include "foundry-git-vcs-private.h"
#include "foundry-operation.h"
#include "foundry-util.h"

/**
 * FoundryGitVcs:
 *
 * Git implementation of the version control system interface.
 *
 * FoundryGitVcs provides Git-specific functionality for version control operations
 * including repository management, branch operations, and remote synchronization.
 * It integrates with the Git library to provide a unified interface for Git
 * operations within the Foundry development environment.
 */

struct _FoundryGitVcs
{
  FoundryVcs            parent_instance;
  FoundryGitMonitor    *monitor;
  FoundryGitRepository *repository;
  GFile                *workdir;
};

G_DEFINE_FINAL_TYPE (FoundryGitVcs, foundry_git_vcs, FOUNDRY_TYPE_VCS)

static char *
foundry_git_vcs_dup_id (FoundryVcs *vcs)
{
  return g_strdup ("git");
}

static char *
foundry_git_vcs_dup_name (FoundryVcs *vcs)
{
  return g_strdup (_("Git"));
}

static char *
foundry_git_vcs_dup_branch_name (FoundryVcs *vcs)
{
  char *branch_name;

  branch_name = _foundry_git_repository_dup_branch_name (FOUNDRY_GIT_VCS (vcs)->repository);

  if (branch_name == NULL)
    return g_strdup ("main");

  return branch_name;
}

static guint
foundry_git_vcs_get_priority (FoundryVcs *vcs)
{
  return 100;
}

static gboolean
foundry_git_vcs_is_ignored (FoundryVcs *vcs,
                            const char *relative_path)
{
  FoundryGitVcs *self = FOUNDRY_GIT_VCS (vcs);

  return _foundry_git_repository_is_ignored (self->repository, relative_path);
}

static gboolean
foundry_git_vcs_is_file_ignored (FoundryVcs *vcs,
                                 GFile      *file)
{
  FoundryGitVcs *self = FOUNDRY_GIT_VCS (vcs);
  g_autofree char *relative_path = NULL;

  if (!g_file_has_prefix (file, self->workdir))
    return FALSE;

  relative_path = g_file_get_relative_path (self->workdir, file);

  return _foundry_git_repository_is_ignored (self->repository, relative_path);
}

static DexFuture *
foundry_git_vcs_list_files (FoundryVcs *vcs)
{
  return _foundry_git_repository_list_files (FOUNDRY_GIT_VCS (vcs)->repository);
}

static DexFuture *
foundry_git_vcs_list_branches (FoundryVcs *vcs)
{
  return _foundry_git_repository_list_branches (FOUNDRY_GIT_VCS (vcs)->repository);
}

static DexFuture *
foundry_git_vcs_list_tags (FoundryVcs *vcs)
{
  return _foundry_git_repository_list_tags (FOUNDRY_GIT_VCS (vcs)->repository);
}

static DexFuture *
foundry_git_vcs_list_remotes (FoundryVcs *vcs)
{
  return _foundry_git_repository_list_remotes (FOUNDRY_GIT_VCS (vcs)->repository);
}

static DexFuture *
foundry_git_vcs_find_file (FoundryVcs *vcs,
                           GFile      *file)
{
  return _foundry_git_repository_find_file (FOUNDRY_GIT_VCS (vcs)->repository, file);
}

static DexFuture *
foundry_git_vcs_find_remote (FoundryVcs *vcs,
                             const char *name)
{
  return _foundry_git_repository_find_remote (FOUNDRY_GIT_VCS (vcs)->repository, name);
}

static DexFuture *
foundry_git_vcs_find_commit (FoundryVcs *vcs,
                             const char *id)
{
  return _foundry_git_repository_find_commit (FOUNDRY_GIT_VCS (vcs)->repository, id);
}

static DexFuture *
foundry_git_vcs_find_tree (FoundryVcs *vcs,
                           const char *id)
{
  return _foundry_git_repository_find_tree (FOUNDRY_GIT_VCS (vcs)->repository, id);
}

static DexFuture *
foundry_git_vcs_blame (FoundryVcs     *vcs,
                       FoundryVcsFile *file,
                       GBytes         *bytes)
{
  FoundryGitVcs *self = FOUNDRY_GIT_VCS (vcs);
  g_autofree char *relative_path = foundry_vcs_file_dup_relative_path (file);

  return _foundry_git_repository_blame (self->repository, relative_path, bytes);
}

static DexFuture *
foundry_git_vcs_diff (FoundryVcs     *vcs,
                      FoundryVcsTree *tree_a,
                      FoundryVcsTree *tree_b)
{
  FoundryGitVcs *self = (FoundryGitVcs *)vcs;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self));
  dex_return_error_if_fail (FOUNDRY_IS_GIT_TREE (tree_a));
  dex_return_error_if_fail (FOUNDRY_IS_GIT_TREE (tree_b));

  return _foundry_git_repository_diff (self->repository,
                                       FOUNDRY_GIT_TREE (tree_a),
                                       FOUNDRY_GIT_TREE (tree_b));
}

static DexFuture *
foundry_git_vcs_fetch (FoundryVcs       *vcs,
                       FoundryVcsRemote *remote,
                       FoundryOperation *operation)
{
  g_autoptr(FoundryAuthProvider) auth_provider = foundry_operation_dup_auth_provider (operation);

  if (auth_provider == NULL)
    {
      g_autoptr(FoundryContext) context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (vcs));
      auth_provider = foundry_auth_provider_new_for_context (context);
    }

  return _foundry_git_repository_fetch (FOUNDRY_GIT_VCS (vcs)->repository, auth_provider, remote, operation);
}

static DexFuture *
foundry_git_vcs_list_commits_with_file (FoundryVcs     *vcs,
                                        FoundryVcsFile *file)
{
  FoundryGitVcs *self = (FoundryGitVcs *)vcs;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self));
  dex_return_error_if_fail (FOUNDRY_IS_GIT_FILE (file));

  return _foundry_git_repository_list_commits_with_file (self->repository, file);
}

static DexFuture *
foundry_git_vcs_describe_line_changes (FoundryVcs     *vcs,
                                       FoundryVcsFile *file,
                                       GBytes         *contents)
{
  FoundryGitVcs *self = (FoundryGitVcs *)vcs;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self));
  dex_return_error_if_fail (FOUNDRY_IS_GIT_FILE (file));
  dex_return_error_if_fail (contents != NULL);

  return _foundry_git_repository_describe_line_changes (self->repository, file, contents);
}

static DexFuture *
foundry_git_vcs_query_file_status (FoundryVcs *vcs,
                                   GFile      *file)
{
  FoundryGitVcs *self = FOUNDRY_GIT_VCS (vcs);

  return _foundry_git_repository_query_file_status (self->repository, file);
}

static void
foundry_git_vcs_finalize (GObject *object)
{
  FoundryGitVcs *self = (FoundryGitVcs *)object;

  g_clear_object (&self->monitor);
  g_clear_object (&self->repository);
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
  vcs_class->find_commit = foundry_git_vcs_find_commit;
  vcs_class->find_tree = foundry_git_vcs_find_tree;
  vcs_class->find_file = foundry_git_vcs_find_file;
  vcs_class->find_remote = foundry_git_vcs_find_remote;
  vcs_class->get_priority = foundry_git_vcs_get_priority;
  vcs_class->is_file_ignored = foundry_git_vcs_is_file_ignored;
  vcs_class->is_ignored = foundry_git_vcs_is_ignored;
  vcs_class->list_branches = foundry_git_vcs_list_branches;
  vcs_class->list_files = foundry_git_vcs_list_files;
  vcs_class->list_remotes = foundry_git_vcs_list_remotes;
  vcs_class->list_tags = foundry_git_vcs_list_tags;
  vcs_class->list_commits_with_file = foundry_git_vcs_list_commits_with_file;
  vcs_class->diff = foundry_git_vcs_diff;
  vcs_class->describe_line_changes = foundry_git_vcs_describe_line_changes;
  vcs_class->query_file_status = foundry_git_vcs_query_file_status;
}

static void
foundry_git_vcs_init (FoundryGitVcs *self)
{
}

static void
foundry_git_vcs_changed_cb (FoundryGitVcs     *self,
                            FoundryGitMonitor *monitor)
{
  g_assert (FOUNDRY_IS_GIT_VCS (self));
  g_assert (FOUNDRY_IS_GIT_MONITOR (monitor));

  g_object_notify (G_OBJECT (self), "branch-name");
}

static DexFuture *
foundry_git_vcs_setup_monitor_cb (DexFuture *future,
                                  gpointer   user_data)
{
  FoundryGitVcs *self = user_data;

  g_assert (FOUNDRY_IS_GIT_VCS (self));

  if ((self->monitor = dex_await_object (dex_ref (future), NULL)))
    g_signal_connect_object (self->monitor,
                             "changed",
                             G_CALLBACK (foundry_git_vcs_changed_cb),
                             self,
                             G_CONNECT_SWAPPED);

  return dex_future_new_true ();
}

DexFuture *
_foundry_git_vcs_new (FoundryContext *context,
                      git_repository *repository)
{
  FoundryGitVcs *self;
  DexFuture *future;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (repository != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_VCS,
                       "context", context,
                       NULL);

  self->workdir = g_file_new_for_path (git_repository_workdir (repository));
  self->repository = _foundry_git_repository_new (g_steal_pointer (&repository));

  future = _foundry_git_repository_create_monitor (self->repository);
  future = dex_future_then (future,
                            foundry_git_vcs_setup_monitor_cb,
                            g_object_ref (self),
                            g_object_unref);
  future = dex_future_then (future,
                            foundry_future_return_object,
                            g_object_ref (self),
                            g_object_unref);

  return g_steal_pointer (&future);
}

typedef struct _Initialize
{
  GFile *directory;
  guint bare : 1;
} Initialize;

static void
initialize_free (Initialize *state)
{
  g_clear_object (&state->directory);
  g_free (state);
}

static DexFuture *
foundry_git_initialize_thread (gpointer data)
{
  Initialize *state = data;
  g_autoptr(git_repository) repository = NULL;
  g_autofree char *path = NULL;

  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->directory));
  g_assert (g_file_is_native (state->directory));

  path = g_file_get_path (state->directory);

  if (git_repository_init (&repository, path, state->bare) != 0)
    return foundry_git_reject_last_error ();

  return dex_future_new_true ();
}

/**
 * foundry_git_initialize:
 *
 * Initializes a new git repository.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   any value if successful or rejects with error.
 */
DexFuture *
foundry_git_initialize (GFile    *directory,
                        gboolean  bare)
{
  Initialize *state;

  dex_return_error_if_fail (G_IS_FILE (directory));
  dex_return_error_if_fail (g_file_is_native (directory));

  state = g_new0 (Initialize, 1);
  state->directory = g_object_ref (directory);
  state->bare = !!bare;

  return dex_thread_spawn ("[git-initialize]",
                           foundry_git_initialize_thread,
                           state,
                           (GDestroyNotify) initialize_free);

}

/**
 * foundry_git_vcs_list_status:
 * @self: a [class@Foundry.GitVcs]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.GitStatusEntry].
 */
DexFuture *
foundry_git_vcs_list_status (FoundryGitVcs *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self));

  return _foundry_git_repository_list_status (self->repository);
}

/**
 * foundry_git_vcs_stage_entry:
 * @self: a [class@Foundry.GitVcs]
 * @entry: a [class@Foundry.GitStatusEntry]
 * @contents: (nullable): optional contents to use instead of what is in
 *   the working tree.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any value
 *   or rejects with error.
 */
DexFuture *
foundry_git_vcs_stage_entry (FoundryGitVcs         *self,
                             FoundryGitStatusEntry *entry,
                             GBytes                *contents)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self));
  dex_return_error_if_fail (FOUNDRY_IS_GIT_STATUS_ENTRY (entry));

  return _foundry_git_repository_stage_entry (self->repository, entry, contents);
}

/**
 * foundry_git_vcs_unstage_entry:
 * @self: a [class@Foundry.GitVcs]
 * @entry: a [class@Foundry.GitStatusEntry]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any value
 *   or rejects with error.
 */
DexFuture *
foundry_git_vcs_unstage_entry (FoundryGitVcs         *self,
                               FoundryGitStatusEntry *entry)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self));
  dex_return_error_if_fail (FOUNDRY_IS_GIT_STATUS_ENTRY (entry));

  return _foundry_git_repository_unstage_entry (self->repository, entry);
}

/**
 * foundry_git_vcs_commit:
 * @self: a [class@Foundry.GitVcs]
 * @message:
 * @author_name: (nullable):
 * @author_email: (nullable):
 *
 * Simple API to create a new commit from the index.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.GitCommit] or rejects with error.
 */
DexFuture *
foundry_git_vcs_commit (FoundryGitVcs *self,
                        const char    *message,
                        const char    *author_name,
                        const char    *author_email)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self));
  dex_return_error_if_fail (message != NULL);

  return _foundry_git_repository_commit (self->repository, message, author_name, author_email);
}

/**
 * foundry_git_vcs_load_head:
 * @self: a [class@Foundry.GitVcs]
 *
 * Loads the HEAD commit from the repository.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.GitCommit] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_git_vcs_load_head (FoundryGitVcs *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self));

  return _foundry_git_repository_load_head (self->repository);
}

/**
 * foundry_git_vcs_query_config:
 * @self: a [class@Foundry.GitVcs]
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
foundry_git_vcs_query_config (FoundryGitVcs *self,
                              const char    *key)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self));
  dex_return_error_if_fail (key != NULL);

  return _foundry_git_repository_query_config (self->repository, key);
}

char *
_foundry_git_vcs_dup_git_dir (FoundryGitVcs *self)
{
  g_autoptr(FoundryGitRepositoryPaths) paths = NULL;

  g_return_val_if_fail (FOUNDRY_IS_GIT_VCS (self), NULL);

  paths = _foundry_git_repository_dup_paths (self->repository);

  return foundry_git_repository_paths_dup_git_dir (paths);
}

GFile *
_foundry_git_vcs_dup_workdir (FoundryGitVcs *self)
{
  g_autoptr(FoundryGitRepositoryPaths) paths = NULL;
  g_autofree char *workdir_path = NULL;

  g_return_val_if_fail (FOUNDRY_IS_GIT_VCS (self), NULL);

  paths = _foundry_git_repository_dup_paths (self->repository);
  workdir_path = foundry_git_repository_paths_dup_workdir (paths);

  return g_file_new_for_path (workdir_path);
}

FoundryGitRepositoryPaths *
_foundry_git_vcs_dup_paths (FoundryGitVcs *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_VCS (self), NULL);

  return _foundry_git_repository_dup_paths (self->repository);
}

static char *
armor_sshsig (GBytes *sshsig_bin)
{
  gsize len = 0;
  const guint8 *data = g_bytes_get_data (sshsig_bin, &len);
  g_autofree char *b64 = g_base64_encode (data, len);
  GString *s = g_string_new("-----BEGIN SSH SIGNATURE-----\n");
  gsize blen = strlen (b64);
  const char *iter = b64;
  const char *endptr = iter + blen;

  while (iter < endptr)
    {
      g_string_append_len (s, iter, MIN (70, endptr - iter));
      g_string_append_c (s, '\n');
      iter += 70;
    }

  g_string_append (s, "-----END SSH SIGNATURE-----");

  return g_string_free (s, FALSE);
}

static inline void
put_u32 (GByteArray *b,
         guint32     v)
{
  guint32 be = GINT32_TO_BE(v);
  g_byte_array_append(b, (guint8 *)&be, 4);
}

static inline void
put_string (GByteArray *b,
            const void *data,
            gsize       len)
{
  put_u32 (b, (guint32)len);

  if (len)
    g_byte_array_append (b, (const guint8 *)data, len);
}

static GBytes *
build_sshsig (const guint8 *pubkey_blob,
              gsize         pubkey_blob_len,
              const guint8 *sha512_hash,
              gsize         sha512_hash_len,
              const guint8 *agent_sig_blob,
              gsize         agent_sig_blob_len)
{
  g_autoptr(GByteArray) arr = g_byte_array_new();
  const gchar *namespace = "git";
  const gchar *hash_alg = "sha512";
  const gchar *magic = "SSHSIG";

  g_assert (sha512_hash_len == 64);

  /* magic is first bytes */
  g_byte_array_append (arr, (const guint8 *)magic, strlen (magic));

  /* uint32 version = 1 */
  put_u32 (arr, 1);

  /* string pubkey_blob */
  put_string (arr, pubkey_blob, pubkey_blob_len);

  /* string namespace "git" */
  put_string (arr, namespace, strlen (namespace));

  /* string reserved "" */
  put_string (arr, "", 0);

  /* string hash algorithm "sha512" */
  put_string (arr, hash_alg, strlen (hash_alg));

  /* string signature (raw agent signature blob) */
  put_string (arr, agent_sig_blob, agent_sig_blob_len);

  return g_byte_array_free_to_bytes (g_steal_pointer (&arr));
}

static char *
sign_bytes_ssh (GBytes      *data,
                const char  *signing_key,
                GError     **error)
{
  g_autoptr(GByteArray) preimage = NULL;
  g_autoptr(GBytes) sig_bytes = NULL;
  g_autoptr(GBytes) sshsig_bytes = NULL;
  g_autoptr(GChecksum) checksum = NULL;
  g_auto(GStrv) tokens = NULL;
  g_autofree guint8 *hash = NULL;
  g_autofree guint8 *pubkey_blob = NULL;
  const char *b64;
  const guint8 *agent_sig_data = NULL;
  gsize agent_sig_len = 0;
  gsize blob_len = 0;
  gsize digest_len;

  /* Parse public key line: "algo base64 [comment]" */
  tokens = g_strsplit_set (signing_key, " \t", 3);
  if (!tokens[0] || !tokens[1])
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Invalid SSH public key line: '%s'",
                   signing_key);
      return NULL;
    }

  b64 = tokens[1];
  pubkey_blob = g_base64_decode (b64, &blob_len);
  if (!pubkey_blob || blob_len == 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Failed to base64-decode SSH public key blob");
      return NULL;
    }

  checksum = g_checksum_new (G_CHECKSUM_SHA512);
  g_checksum_update (checksum,
                     g_bytes_get_data (data, NULL),
                     g_bytes_get_size (data));

  digest_len = g_checksum_type_get_length (G_CHECKSUM_SHA512);
  hash = g_malloc (digest_len);
  g_checksum_get_digest (checksum, hash, &digest_len);

  preimage = g_byte_array_new ();
  g_byte_array_append (preimage, (guint8 *)"SSHSIG", strlen ("SSHSIG"));
  put_string (preimage, "git", strlen ("git"));
  put_string (preimage, "", 0);
  put_string (preimage, "sha512", strlen ("sha512"));
  put_string (preimage, hash, digest_len);

  /* Sign the structure (not the raw commit content) */
  if (!ssh_agent_sign_data_for_pubkey (signing_key, preimage->data, preimage->len, &sig_bytes, error))
    return NULL;

  agent_sig_data = g_bytes_get_data (sig_bytes, &agent_sig_len);

  /* Build the complete SSH signature format */
  sshsig_bytes = build_sshsig (pubkey_blob,
                               blob_len,
                               hash,
                               digest_len,
                               agent_sig_data,
                               agent_sig_len);

  /* Armor the SSH signature for git */
  return armor_sshsig (sshsig_bytes);
}

static char *
sign_bytes_gpg (GBytes      *data,
                const char  *signing_key,
                GError     **error)
{
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GBytes) stdout_bytes = NULL;
  GOutputStream *stdin_stream = NULL;
  GInputStream *stdout_stream = NULL;
  const guint8 *stdout_data;
  gsize stdout_size;

  g_assert (data != NULL);
  g_assert (signing_key != NULL);

  if (!(subprocess = g_subprocess_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                       G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                       G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                                       error,
                                       "gpg",
                                       "--detach-sign",
                                       "--armor",
                                       "--local-user",
                                       signing_key,
                                       NULL)))
    return NULL;

  stdin_stream = g_subprocess_get_stdin_pipe (subprocess);
  stdout_stream = g_subprocess_get_stdout_pipe (subprocess);

  if (!g_output_stream_write_all (stdin_stream,
                                  g_bytes_get_data (data, NULL),
                                  g_bytes_get_size (data),
                                  NULL, NULL, error))
    return NULL;

  g_output_stream_close (stdin_stream, NULL, NULL);

  if (!g_subprocess_wait_check (subprocess, NULL, error))
    return NULL;

  if (!(stdout_bytes = g_input_stream_read_bytes (stdout_stream, G_MAXSSIZE, NULL, error)))
    return NULL;

  stdout_data = g_bytes_get_data (stdout_bytes, &stdout_size);

  if (!g_utf8_validate ((char *)stdout_data, stdout_size, NULL))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Invalid UTF-8 received from gpg");
      return NULL;
    }

  return g_strndup ((const char *)stdout_data, stdout_size);
}

typedef struct _SignBytes
{
  GBytes *bytes;
  char *signing_format;
  char *signing_key;
} SignBytes;

static void
sign_bytes_free (SignBytes *state)
{
  g_clear_pointer (&state->bytes, g_bytes_unref);
  g_clear_pointer (&state->signing_format, g_free);
  g_clear_pointer (&state->signing_key, g_free);
  g_free (state);
}

static DexFuture *
foundry_git_vcs_sign_bytes_thread (gpointer user_data)
{
  SignBytes *state = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *ret = NULL;

  g_assert (state != NULL);
  g_assert (state->bytes != NULL);
  g_assert (state->signing_format != NULL);
  g_assert (state->signing_key != NULL);

  if (foundry_str_equal0 (state->signing_format, "gpg"))
    {
      if (!(ret = sign_bytes_gpg (state->bytes, state->signing_key, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      return dex_future_new_take_string (g_steal_pointer (&ret));
    }

  if (foundry_str_equal0 (state->signing_format, "ssh"))
    {
      if (!(ret = sign_bytes_ssh (state->bytes, state->signing_key, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      return dex_future_new_take_string (g_steal_pointer (&ret));
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Signing format `%s` is not supported",
                                state->signing_format);
}

char *
_foundry_git_vcs_sign_bytes (const char  *signing_format,
                             const char  *signing_key,
                             GBytes      *bytes,
                             GError     **error)
{
  g_autoptr(DexFuture) future = NULL;
  const GValue *value;
  SignBytes *state;
  char *ret = NULL;

  g_return_val_if_fail (signing_format != NULL, NULL);
  g_return_val_if_fail (signing_key != NULL, NULL);
  g_return_val_if_fail (bytes != NULL, NULL);

  state = g_new0 (SignBytes, 1);
  state->signing_format = g_strdup (signing_format);
  state->signing_key = g_strdup (signing_key);
  state->bytes = g_bytes_ref (bytes);

  future = foundry_git_vcs_sign_bytes_thread (state);

  if ((value = dex_future_get_value (future, error)))
    ret = g_value_dup_string (value);

  sign_bytes_free (state);

  return ret;
}

/**
 * foundry_git_vcs_sign_bytes:
 * @self: a [class@Foundry.GitVcs]
 * @signing_format: the format such as "gpg" or "ssh" such as from the
 *   config value for "gpg.format".
 * @signing_key: the signing key to use such as from the config
 *   value for "user.signingKey"
 * @bytes: the bytes to sign
 *
 * Signs @bytes using the format and key specified.
 *
 * Use [method@Foundry.GitVcs.query_config] to get the
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   armor contained string.
 *
 * Since: 1.1
 */
DexFuture *
foundry_git_vcs_sign_bytes (FoundryGitVcs *self,
                            const char    *signing_format,
                            const char    *signing_key,
                            GBytes        *bytes)
{
  SignBytes *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self));
  dex_return_error_if_fail (signing_format != NULL);
  dex_return_error_if_fail (signing_key != NULL);
  dex_return_error_if_fail (bytes != NULL);

  state = g_new0 (SignBytes, 1);
  state->signing_format = g_strdup (signing_format);
  state->signing_key = g_strdup (signing_key);
  state->bytes = g_bytes_ref (bytes);

  return dex_thread_spawn ("[foundry-git-vcs-sign-bytes]",
                           foundry_git_vcs_sign_bytes_thread,
                           state,
                           (GDestroyNotify) sign_bytes_free);
}

/**
 * foundry_git_vcs_stash:
 * @self: a [class@Foundry.GitVcs]
 *
 * Stashes the current working directory changes.
 *
 * This method saves the current changes in the working directory to the stash,
 * similar to running "git stash". The changes are saved with the default stash
 * options.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.GitCommit] representing the stash commit or
 *   rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_git_vcs_stash (FoundryGitVcs *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (self));

  return _foundry_git_repository_stash (self->repository);
}
