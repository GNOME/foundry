/* foundry-git-commit-builder.c
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

#include <gio/gio.h>
#include <glib.h>
#include <git2.h>

#include "foundry-git-autocleanups.h"
#include "foundry-git-commit-builder.h"
#include "foundry-git-commit-private.h"
#include "foundry-git-delta-private.h"
#include "foundry-git-diff-hunk-private.h"
#include "foundry-git-diff-line-private.h"
#include "foundry-git-diff-private.h"
#include "foundry-git-error.h"
#include "foundry-git-patch-private.h"
#include "foundry-git-repository-paths-private.h"
#include "foundry-git-status-entry-private.h"
#include "foundry-git-vcs-private.h"
#include "foundry-util.h"

#define MAX_UNTRACKED_FILES 25000

struct _FoundryGitCommitBuilder
{
  GObject                    parent_instance;

  FoundryGitVcs             *vcs;
  FoundryGitCommit          *parent;

  char                      *author_name;
  char                      *author_email;
  char                      *signing_key;
  char                      *signing_format;
  FoundryGitRepositoryPaths *paths;
  char                      *message;
  GDateTime                 *when;

  GListStore                *staged;
  GListStore                *unstaged;
  GListStore                *untracked;

  GHashTable                *initially_untracked;

  GMutex                     mutex;
  FoundryGitDiff            *staged_diff;
  FoundryGitDiff            *unstaged_diff;

  guint                      context_lines;
};

enum {
  PROP_0,
  PROP_AUTHOR_EMAIL,
  PROP_AUTHOR_NAME,
  PROP_SIGNING_KEY,
  PROP_SIGNING_FORMAT,
  PROP_WHEN,
  PROP_MESSAGE,
  PROP_CAN_COMMIT,
  PROP_STAGED,
  PROP_UNSTAGED,
  PROP_UNTRACKED,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryGitCommitBuilder, foundry_git_commit_builder, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_git_commit_builder_finalize (GObject *object)
{
  FoundryGitCommitBuilder *self = (FoundryGitCommitBuilder *)object;

  g_clear_object (&self->vcs);
  g_clear_object (&self->parent);

  g_clear_object (&self->staged);
  g_clear_object (&self->unstaged);
  g_clear_object (&self->untracked);

  g_clear_pointer (&self->initially_untracked, g_hash_table_unref);

  g_clear_pointer (&self->paths, foundry_git_repository_paths_unref);
  g_clear_pointer (&self->author_name, g_free);
  g_clear_pointer (&self->author_email, g_free);
  g_clear_pointer (&self->signing_key, g_free);
  g_clear_pointer (&self->signing_format, g_free);
  g_clear_pointer (&self->message, g_free);

  g_clear_pointer (&self->when, g_date_time_unref);
  g_clear_object (&self->staged_diff);
  g_clear_object (&self->unstaged_diff);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (foundry_git_commit_builder_parent_class)->finalize (object);
}

/**
 * foundry_git_commit_builder_get_can_commit:
 * @self: a [class@Foundry.GitCommitBuilder]
 *
 * Checks whether the builder has sufficient information to create a commit.
 *
 * Returns %TRUE if both a non-empty commit message and at least one staged
 * file are present. Returns %FALSE otherwise.
 *
 * Returns: %TRUE if a commit can be created, %FALSE otherwise
 *
 * Since: 1.1
 */
gboolean
foundry_git_commit_builder_get_can_commit (FoundryGitCommitBuilder *self)
{
  guint staged_count;

  g_return_val_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self), FALSE);

  /* Must have a non-empty commit message */
  if (self->message == NULL || self->message[0] == '\0')
    return FALSE;

  /* Must have at least one staged file */
  staged_count = g_list_model_get_n_items (G_LIST_MODEL (self->staged));
  if (staged_count == 0)
    return FALSE;

  return TRUE;
}

static void
foundry_git_commit_builder_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  FoundryGitCommitBuilder *self = FOUNDRY_GIT_COMMIT_BUILDER (object);

  switch (prop_id)
    {
    case PROP_AUTHOR_EMAIL:
      g_value_take_string (value, foundry_git_commit_builder_dup_author_email (self));
      break;

    case PROP_AUTHOR_NAME:
      g_value_take_string (value, foundry_git_commit_builder_dup_author_name (self));
      break;

    case PROP_WHEN:
      g_value_take_boxed (value, foundry_git_commit_builder_dup_when (self));
      break;

    case PROP_SIGNING_KEY:
      g_value_take_string (value, foundry_git_commit_builder_dup_signing_key (self));
      break;

    case PROP_SIGNING_FORMAT:
      g_value_take_string (value, foundry_git_commit_builder_dup_signing_format (self));
      break;

    case PROP_MESSAGE:
      g_value_take_string (value, foundry_git_commit_builder_dup_message (self));
      break;

    case PROP_CAN_COMMIT:
      g_value_set_boolean (value, foundry_git_commit_builder_get_can_commit (self));
      break;

    case PROP_STAGED:
      g_value_take_object (value, foundry_git_commit_builder_list_staged (self));
      break;

    case PROP_UNSTAGED:
      g_value_take_object (value, foundry_git_commit_builder_list_unstaged (self));
      break;

    case PROP_UNTRACKED:
      g_value_take_object (value, foundry_git_commit_builder_list_untracked (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_git_commit_builder_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  FoundryGitCommitBuilder *self = FOUNDRY_GIT_COMMIT_BUILDER (object);

  switch (prop_id)
    {
    case PROP_AUTHOR_EMAIL:
      foundry_git_commit_builder_set_author_email (self, g_value_get_string (value));
      break;

    case PROP_AUTHOR_NAME:
      foundry_git_commit_builder_set_author_name (self, g_value_get_string (value));
      break;

    case PROP_WHEN:
      foundry_git_commit_builder_set_when (self, g_value_get_boxed (value));
      break;

    case PROP_SIGNING_KEY:
      foundry_git_commit_builder_set_signing_key (self, g_value_get_string (value));
      break;

    case PROP_SIGNING_FORMAT:
      foundry_git_commit_builder_set_signing_format (self, g_value_get_string (value));
      break;

    case PROP_MESSAGE:
      foundry_git_commit_builder_set_message (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_git_commit_builder_class_init (FoundryGitCommitBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_git_commit_builder_finalize;
  object_class->get_property = foundry_git_commit_builder_get_property;
  object_class->set_property = foundry_git_commit_builder_set_property;

  /**
   * FoundryGitCommitBuilder:author-email:
   *
   * The email address of the commit author.
   *
   * If not set, the value from git config "user.email" will be used when
   * creating the commit.
   */
  properties[PROP_AUTHOR_EMAIL] =
    g_param_spec_string ("author-email", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryGitCommitBuilder:author-name:
   *
   * The name of the commit author.
   *
   * If not set, the value from git config "user.name" will be used when
   * creating the commit.
   */
  properties[PROP_AUTHOR_NAME] =
    g_param_spec_string ("author-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryGitCommitBuilder:when:
   *
   * The timestamp for the commit.
   *
   * If not set, the current time will be used when creating the commit.
   */
  properties[PROP_WHEN] =
    g_param_spec_boxed ("when", NULL, NULL,
                        G_TYPE_DATE_TIME,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * FoundryGitCommitBuilder:signing-key:
   *
   * The key identifier to use for signing the commit.
   *
   * If set, the commit will be signed using the specified key and the
   * signing format. If not set, the commit will not be signed.
   */
  properties[PROP_SIGNING_KEY] =
    g_param_spec_string ("signing-key", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryGitCommitBuilder:signing-format:
   *
   * The format to use for signing commits.
   *
   * Common values are "gpg" for GPG signatures or "ssh" for SSH signatures.
   * Defaults to "gpg" if not set.
   */
  properties[PROP_SIGNING_FORMAT] =
    g_param_spec_string ("signing-format", NULL, NULL,
                         "gpg",
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryGitCommitBuilder:message:
   *
   * The commit message.
   *
   * This must be set to a non-empty string before a commit can be created.
   */
  properties[PROP_MESSAGE] =
    g_param_spec_string ("message", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryGitCommitBuilder:can-commit:
   *
   * Whether the builder has sufficient information to create a commit.
   *
   * This property is %TRUE when both a non-empty commit message and at least
   * one staged file are present. It is read-only and will be updated
   * automatically as files are staged or unstaged and as the message changes.
   */
  properties[PROP_CAN_COMMIT] =
    g_param_spec_boolean ("can-commit", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  /**
   * FoundryGitCommitBuilder:staged:
   *
   * A list model containing all files that are currently staged for commit.
   *
   * The list model contains [class@Foundry.GitStatusEntry] objects representing
   * files in the working tree that have been staged. The list is updated
   * automatically as files are staged or unstaged.
   */
  properties[PROP_STAGED] =
    g_param_spec_object ("staged", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryGitCommitBuilder:unstaged:
   *
   * A list model containing all files that have unstaged changes.
   *
   * The list model contains [class@Foundry.GitStatusEntry] objects representing
   * files in the working tree that have been modified but not staged. The list
   * is updated automatically as files are staged or unstaged.
   */
  properties[PROP_UNSTAGED] =
    g_param_spec_object ("unstaged", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryGitCommitBuilder:untracked:
   *
   * A list model containing all untracked files in the working tree.
   *
   * The list model contains [class@Foundry.GitStatusEntry] objects representing
   * files in the working tree that are not tracked by git. The list is updated
   * automatically as files are staged or untracked files are added.
   */
  properties[PROP_UNTRACKED] =
    g_param_spec_object ("untracked", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_git_commit_builder_init (FoundryGitCommitBuilder *self)
{
  g_mutex_init (&self->mutex);

  self->unstaged = g_list_store_new (FOUNDRY_TYPE_GIT_STATUS_ENTRY);
  self->untracked = g_list_store_new (FOUNDRY_TYPE_GIT_STATUS_ENTRY);
  self->staged = g_list_store_new (FOUNDRY_TYPE_GIT_STATUS_ENTRY);
  self->initially_untracked = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->context_lines = 3;
}

static DexFuture *
foundry_git_commit_builder_new_thread (gpointer user_data)
{
  FoundryGitCommitBuilder *self = user_data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_status_list) status_list = NULL;
  g_autoptr(git_index) index = NULL;
  g_autoptr(git_tree) tree = NULL;
  g_autoptr(git_diff) staged_diff = NULL;
  g_autoptr(git_diff) unstaged_diff = NULL;
  g_autoptr(FoundryGitDiff) new_staged_diff = NULL;
  g_autoptr(FoundryGitDiff) new_unstaged_diff = NULL;
  git_status_options opts = GIT_STATUS_OPTIONS_INIT;
  git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;
  gsize entry_count;
  guint untracked_count = 0;

  g_assert (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));
  g_assert (self->paths != NULL);

  if (!foundry_git_repository_paths_open (self->paths, &repository, NULL))
    return foundry_git_reject_last_error ();

  if (git_repository_index (&index, repository) != 0)
    return foundry_git_reject_last_error ();

  opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
  opts.flags = (GIT_STATUS_OPT_INCLUDE_UNTRACKED |
                GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
                GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
                GIT_STATUS_OPT_SORT_CASE_SENSITIVELY);

  if (self->parent != NULL)
    {
      git_oid tree_id;

      if (_foundry_git_commit_get_tree_id (self->parent, &tree_id))
        {
          if (git_tree_lookup (&tree, repository, &tree_id) != 0)
            return foundry_git_reject_last_error ();

          opts.baseline = tree;
        }
    }

  if (git_status_list_new (&status_list, repository, &opts) != 0)
    return foundry_git_reject_last_error ();

  diff_opts.context_lines = self->context_lines;

  /* Create diff from parent tree to index (staged changes) */
  if (git_diff_tree_to_index (&staged_diff, repository, tree, index, &diff_opts) != 0)
    return foundry_git_reject_last_error ();

  /* Create diff from index to working directory (unstaged changes) */
  if (git_diff_index_to_workdir (&unstaged_diff, repository, index, &diff_opts) != 0)
    return foundry_git_reject_last_error ();

  /* Create new diff objects */
  new_staged_diff = _foundry_git_diff_new_with_paths (g_steal_pointer (&staged_diff), self->paths);
  new_unstaged_diff = _foundry_git_diff_new_with_paths (g_steal_pointer (&unstaged_diff), self->paths);

  /* Lock mutex and set diffs atomically */
  g_mutex_lock (&self->mutex);
  g_set_object (&self->staged_diff, new_staged_diff);
  g_set_object (&self->unstaged_diff, new_unstaged_diff);
  g_mutex_unlock (&self->mutex);

  entry_count = git_status_list_entrycount (status_list);

  for (gsize i = 0; i < entry_count; i++)
    {
      const git_status_entry *entry = git_status_byindex (status_list, i);
      const char *path = NULL;
      g_autoptr(FoundryGitStatusEntry) status_entry = NULL;
      guint status;

      if (entry == NULL)
        continue;

      status = entry->status;

      if (entry->head_to_index)
        path = entry->head_to_index->new_file.path;
      else if (entry->index_to_workdir)
        path = entry->index_to_workdir->new_file.path;

      if (path == NULL)
        continue;

      if (!(status_entry = _foundry_git_status_entry_new (entry)))
        continue;

      /* Check for staged changes */
      if (status & (GIT_STATUS_INDEX_NEW |
                    GIT_STATUS_INDEX_MODIFIED |
                    GIT_STATUS_INDEX_DELETED |
                    GIT_STATUS_INDEX_RENAMED |
                    GIT_STATUS_INDEX_TYPECHANGE))
        g_list_store_append (self->staged, status_entry);

      /* Check for unstaged changes (but not untracked) */
      if (status & (GIT_STATUS_WT_MODIFIED |
                    GIT_STATUS_WT_DELETED |
                    GIT_STATUS_WT_RENAMED |
                    GIT_STATUS_WT_TYPECHANGE))
        g_list_store_append (self->unstaged, status_entry);

      /* Check for untracked files */
      if (status & GIT_STATUS_WT_NEW)
        {
          if (!(status & (GIT_STATUS_INDEX_NEW |
                          GIT_STATUS_INDEX_MODIFIED |
                          GIT_STATUS_INDEX_DELETED |
                          GIT_STATUS_INDEX_RENAMED |
                          GIT_STATUS_INDEX_TYPECHANGE)))
            {
              if (untracked_count < MAX_UNTRACKED_FILES)
                {
                  g_list_store_append (self->untracked, status_entry);
                  g_hash_table_add (self->initially_untracked, g_strdup (path));
                  untracked_count++;
                }
            }
        }

      /* Check for staged files that were never in HEAD (initially untracked) */
      if (status & GIT_STATUS_INDEX_NEW)
        {
          gboolean was_in_head = FALSE;

          if (entry->head_to_index != NULL)
            was_in_head = (entry->head_to_index->old_file.path != NULL &&
                           entry->head_to_index->old_file.mode != 0);

          if (!was_in_head)
            {
              if (!g_hash_table_contains (self->initially_untracked, path))
                {
                  if (untracked_count < MAX_UNTRACKED_FILES)
                    {
                      g_hash_table_add (self->initially_untracked, g_strdup (path));
                      untracked_count++;
                    }
                }
            }
        }
    }

  return dex_future_new_take_object (g_object_ref (self));
}

static DexFuture *
foundry_git_commit_builder_new_fiber (FoundryGitVcs    *vcs,
                                      FoundryGitCommit *parent,
                                      guint             context_lines)
{
  g_autoptr(FoundryGitCommitBuilder) self = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_GIT_VCS (vcs));
  g_assert (!parent || FOUNDRY_IS_GIT_COMMIT (parent));

  if (parent == NULL)
    {
      if (!(parent = dex_await_object (foundry_git_vcs_load_head (vcs), &error)))
        {
          if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
            return dex_future_new_for_error (g_steal_pointer (&error));

          g_clear_error (&error);
        }
    }

  self = g_object_new (FOUNDRY_TYPE_GIT_COMMIT_BUILDER, NULL);
  g_set_object (&self->vcs, vcs);
  self->author_name = dex_await_string (foundry_git_vcs_query_config (vcs, "user.name"), NULL);
  self->author_email = dex_await_string (foundry_git_vcs_query_config (vcs, "user.email"), NULL);
  self->signing_key = dex_await_string (foundry_git_vcs_query_config (vcs, "user.signingKey"), NULL);
  self->signing_format = dex_await_string (foundry_git_vcs_query_config (vcs, "gpg.format"), NULL);
  self->paths = _foundry_git_vcs_dup_paths (vcs);
  g_set_object (&self->parent, parent);

  if (context_lines)
    self->context_lines = context_lines;

  return dex_thread_spawn ("[git-commit-builder]",
                           foundry_git_commit_builder_new_thread,
                           g_object_ref (self),
                           g_object_unref);
}

/**
 * foundry_git_commit_builder_new:
 * @vcs: a [class@Foundry.GitVcs]
 * @parent: (nullable): a [class@Foundry.GitCommit]
 * @context_lines: the number of context lines to use for diffs or 0 for default
 *
 * Creates a new builder using @parent as the parent commit.
 *
 * If parent is NULL, then the last commit on the current branch will
 * be used as the parent.
 *
 * The @context_lines parameter controls how many lines of context are
 * included around each change in the diff. The default is 3 lines.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.GitCommitBuilder].
 *
 * Since: 1.1
 */
DexFuture *
foundry_git_commit_builder_new (FoundryGitVcs    *vcs,
                                FoundryGitCommit *parent,
                                guint             context_lines)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS (vcs));
  dex_return_error_if_fail (!parent || FOUNDRY_IS_GIT_COMMIT (parent));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (foundry_git_commit_builder_new_fiber),
                                  3,
                                  FOUNDRY_TYPE_GIT_VCS, vcs,
                                  FOUNDRY_TYPE_GIT_COMMIT, parent,
                                  G_TYPE_UINT, context_lines);
}

static DexFuture *
foundry_git_commit_builder_new_similar_fiber (gpointer user_data)
{
  FoundryGitCommitBuilder *self = user_data;
  g_autoptr(FoundryGitCommitBuilder) new_builder = NULL;
  g_autoptr(FoundryGitCommit) parent = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));
  g_assert (FOUNDRY_IS_GIT_VCS (self->vcs));

  if (self->parent == NULL)
    {
      if (!(parent = dex_await_object (foundry_git_vcs_load_head (self->vcs), &error)))
        {
          if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
            return dex_future_new_for_error (g_steal_pointer (&error));

          g_clear_error (&error);
        }
    }
  else
    {
      parent = g_object_ref (self->parent);
    }

  new_builder = g_object_new (FOUNDRY_TYPE_GIT_COMMIT_BUILDER, NULL);
  g_set_object (&new_builder->vcs, self->vcs);
  new_builder->author_name = g_strdup (self->author_name);
  new_builder->author_email = g_strdup (self->author_email);
  new_builder->signing_key = g_strdup (self->signing_key);
  new_builder->signing_format = g_strdup (self->signing_format);
  new_builder->paths = foundry_git_repository_paths_ref (self->paths);
  g_set_object (&new_builder->parent, parent);
  new_builder->context_lines = self->context_lines;

  if (self->when != NULL)
    new_builder->when = g_date_time_ref (self->when);

  return dex_thread_spawn ("[git-commit-builder]",
                           foundry_git_commit_builder_new_thread,
                           g_object_ref (new_builder),
                           g_object_unref);
}

/**
 * foundry_git_commit_builder_new_similar:
 * @self: a [class@Foundry.GitCommitBuilder]
 *
 * Creates a new builder similar to @self, copying all string and GDateTime
 * properties from the existing builder.
 *
 * The new builder will use the same VCS instance, parent commit (or HEAD if
 * no parent was set), context lines, author name, author email, signing key,
 * signing format, and timestamp as @self.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.GitCommitBuilder].
 *
 * Since: 1.1
 */
DexFuture *
foundry_git_commit_builder_new_similar (FoundryGitCommitBuilder *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));

  return dex_scheduler_spawn (NULL, 0,
                              foundry_git_commit_builder_new_similar_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

/**
 * foundry_git_commit_builder_dup_author_name:
 * @self: a [class@Foundry.GitCommitBuilder]
 *
 * Gets the author name that will be used for the commit.
 *
 * Returns: (transfer full) (nullable): the author name, or %NULL if not set.
 *   The caller should free the returned string with g_free() when done.
 *
 * Since: 1.1
 */
char *
foundry_git_commit_builder_dup_author_name (FoundryGitCommitBuilder *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self), NULL);

  return g_strdup (self->author_name);
}

/**
 * foundry_git_commit_builder_dup_author_email:
 * @self: a [class@Foundry.GitCommitBuilder]
 *
 * Gets the author email address that will be used for the commit.
 *
 * Returns: (transfer full) (nullable): the author email, or %NULL if not set.
 *   The caller should free the returned string with g_free() when done.
 *
 * Since: 1.1
 */
char *
foundry_git_commit_builder_dup_author_email (FoundryGitCommitBuilder *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self), NULL);

  return g_strdup (self->author_email);
}

/**
 * foundry_git_commit_builder_dup_signing_key:
 * @self: a [class@Foundry.GitCommitBuilder]
 *
 * Gets the signing key identifier that will be used for signing the commit.
 *
 * Returns: (transfer full) (nullable): the signing key identifier, or %NULL
 *   if not set. The caller should free the returned string with g_free()
 *   when done.
 *
 * Since: 1.1
 */
char *
foundry_git_commit_builder_dup_signing_key (FoundryGitCommitBuilder *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self), NULL);

  return g_strdup (self->signing_key);
}

/**
 * foundry_git_commit_builder_set_author_name:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @author_name: (nullable): the author name to use, or %NULL to unset
 *
 * Sets the author name that will be used for the commit.
 *
 * If set to %NULL or not set, the value from git config "user.name" will be
 * used when creating the commit.
 *
 * Since: 1.1
 */
void
foundry_git_commit_builder_set_author_name (FoundryGitCommitBuilder *self,
                                            const char              *author_name)
{
  g_return_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));

  if (g_set_str (&self->author_name, author_name))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUTHOR_NAME]);
}

/**
 * foundry_git_commit_builder_set_author_email:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @author_email: (nullable): the author email to use, or %NULL to unset
 *
 * Sets the author email address that will be used for the commit.
 *
 * If set to %NULL or not set, the value from git config "user.email" will be
 * used when creating the commit.
 *
 * Since: 1.1
 */
void
foundry_git_commit_builder_set_author_email (FoundryGitCommitBuilder *self,
                                            const char              *author_email)
{
  g_return_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));

  if (g_set_str (&self->author_email, author_email))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUTHOR_EMAIL]);
}

/**
 * foundry_git_commit_builder_set_signing_key:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @signing_key: (nullable): the signing key identifier to use, or %NULL to disable signing
 *
 * Sets the signing key identifier that will be used for signing the commit.
 *
 * If set, the commit will be signed using the specified key and the signing
 * format. If set to %NULL, the commit will not be signed.
 *
 * Since: 1.1
 */
void
foundry_git_commit_builder_set_signing_key (FoundryGitCommitBuilder *self,
                                            const char              *signing_key)
{
  g_return_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));

  if (g_set_str (&self->signing_key, signing_key))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SIGNING_KEY]);
}

/**
 * foundry_git_commit_builder_dup_signing_format:
 * @self: a [class@Foundry.GitCommitBuilder]
 *
 * Gets the signing format that will be used for signing the commit.
 *
 * Returns: (transfer full) (nullable): the signing format (e.g. "gpg" or
 *   "ssh"), or %NULL if not set. The caller should free the returned string
 *   with g_free() when done.
 *
 * Since: 1.1
 */
char *
foundry_git_commit_builder_dup_signing_format (FoundryGitCommitBuilder *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self), NULL);

  return g_strdup (self->signing_format);
}

/**
 * foundry_git_commit_builder_set_signing_format:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @signing_format: (nullable): the signing format to use (e.g. "gpg" or "ssh"), or %NULL for default
 *
 * Sets the signing format that will be used for signing the commit.
 *
 * Common values are "gpg" for GPG signatures or "ssh" for SSH signatures.
 * If set to %NULL, defaults to "gpg".
 *
 * Since: 1.1
 */
void
foundry_git_commit_builder_set_signing_format (FoundryGitCommitBuilder *self,
                                               const char              *signing_format)
{
  g_return_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));

  if (g_set_str (&self->signing_format, signing_format))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SIGNING_FORMAT]);
}

/**
 * foundry_git_commit_builder_set_when:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @when: (nullable): the timestamp to use for the commit, or %NULL to use current time
 *
 * Sets the timestamp that will be used for the commit.
 *
 * If set to %NULL or not set, the current time will be used when creating
 * the commit. The builder takes ownership of @when.
 *
 * Since: 1.1
 */
void
foundry_git_commit_builder_set_when (FoundryGitCommitBuilder *self,
                                     GDateTime               *when)
{
  g_return_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));

  if (when == self->when)
    return;

  if (when != NULL)
    g_date_time_ref (when);

  g_clear_pointer (&self->when, g_date_time_unref);
  self->when = when;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WHEN]);
}

/**
 * foundry_git_commit_builder_dup_message:
 * @self: a [class@Foundry.GitCommitBuilder]
 *
 * Gets the commit message that will be used for the commit.
 *
 * Returns: (transfer full) (nullable): the commit message, or %NULL if not set.
 *   The caller should free the returned string with g_free() when done.
 *
 * Since: 1.1
 */
char *
foundry_git_commit_builder_dup_message (FoundryGitCommitBuilder *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self), NULL);

  return g_strdup (self->message);
}

/**
 * foundry_git_commit_builder_set_message:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @message: (nullable): the commit message to use, or %NULL to unset
 *
 * Sets the commit message that will be used for the commit.
 *
 * This must be set to a non-empty string before a commit can be created.
 * Setting this will automatically update the [property@Foundry.GitCommitBuilder:can-commit]
 * property.
 *
 * Since: 1.1
 */
void
foundry_git_commit_builder_set_message (FoundryGitCommitBuilder *self,
                                        const char              *message)
{
  gboolean old_can_commit;
  gboolean new_can_commit;

  g_return_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));

  old_can_commit = foundry_git_commit_builder_get_can_commit (self);

  if (g_set_str (&self->message, message))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MESSAGE]);

      new_can_commit = foundry_git_commit_builder_get_can_commit (self);
      if (old_can_commit != new_can_commit)
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CAN_COMMIT]);
    }
}

/**
 * foundry_git_commit_builder_dup_when:
 * @self: a [class@Foundry.GitCommitBuilder]
 *
 * Gets the timestamp that will be used for the commit.
 *
 * Returns: (transfer full) (nullable): a [class@GLib.DateTime] representing
 *   the commit timestamp, or %NULL if not set. The caller should free the
 *   returned object with g_date_time_unref() when done.
 *
 * Since: 1.1
 */
GDateTime *
foundry_git_commit_builder_dup_when (FoundryGitCommitBuilder *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self), NULL);

  return self->when ? g_date_time_ref (self->when) : NULL;
}

static char *
ensure_message_trailing_newline (const char *message)
{
  gsize len;

  if (message == NULL)
    return NULL;

  len = strlen (message);

  if (len > 0 && message[len - 1] == '\n')
    return g_strdup (message);

  return g_strconcat (message, "\n", NULL);
}

typedef struct _BuilderCommit
{
  FoundryGitRepositoryPaths *paths;
  char *message;
  char *author_name;
  char *author_email;
  char *signing_key;
  char *signing_format;
  GDateTime *when;
  git_oid parent_id;
  guint has_parent : 1;
} BuilderCommit;

static void
builder_commit_free (BuilderCommit *state)
{
  g_clear_pointer (&state->paths, foundry_git_repository_paths_unref);
  g_clear_pointer (&state->message, g_free);
  g_clear_pointer (&state->author_name, g_free);
  g_clear_pointer (&state->author_email, g_free);
  g_clear_pointer (&state->signing_key, g_free);
  g_clear_pointer (&state->signing_format, g_free);
  g_clear_pointer (&state->when, g_date_time_unref);
  g_free (state);
}

static char *
sign_commit_content (const char  *commit_content,
                     const char  *signing_key,
                     const char  *signing_format,
                     GError     **error)
{
  g_autoptr(GBytes) to_sign = NULL;

  if (signing_key == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVAL,
                   "No signing key provided to sign content");
      return NULL;
    }

  if (signing_format == NULL)
    signing_format = "gpg";

  g_assert (commit_content != NULL);
  g_assert (signing_key != NULL);
  g_assert (signing_format != NULL);

  to_sign = g_bytes_new (commit_content, strlen (commit_content));

  return _foundry_git_vcs_sign_bytes (signing_format, signing_key, to_sign, error);
}

static DexFuture *
foundry_git_commit_builder_commit_thread (gpointer data)
{
  BuilderCommit *state = data;
  g_autofree char *author_name = NULL;
  g_autofree char *author_email = NULL;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_config) config = NULL;
  g_autoptr(git_index) index = NULL;
  g_autoptr(git_tree) tree = NULL;
  g_autoptr(git_signature) author = NULL;
  g_autoptr(git_signature) committer = NULL;
  g_autoptr(git_commit) parent = NULL;
  g_autoptr(git_commit) commit = NULL;
  git_oid tree_oid;
  git_oid commit_oid;
  int err;
  time_t commit_time;
  int offset;

  g_assert (state != NULL);
  g_assert (state->paths != NULL);
  g_assert (state->message != NULL);

  if (!foundry_git_repository_paths_open (state->paths, &repository, NULL))
    return foundry_git_reject_last_error ();

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

  if (state->when != NULL)
    {
      commit_time = g_date_time_to_unix (state->when);
      offset = g_date_time_get_utc_offset (state->when) / G_TIME_SPAN_SECOND;

      if (git_signature_new (&author, author_name, author_email, commit_time, offset) != 0)
        return foundry_git_reject_last_error ();
    }
  else
    {
      if (git_signature_now (&author, author_name, author_email) != 0)
        return foundry_git_reject_last_error ();
    }

  if (git_signature_dup (&committer, author) != 0)
    return foundry_git_reject_last_error ();

  if (state->has_parent)
    {
      if (git_commit_lookup (&parent, repository, &state->parent_id) != 0)
        return foundry_git_reject_last_error ();
    }
  else
    {
      g_autoptr(git_object) parent_obj = NULL;

      if ((err = git_revparse_single (&parent_obj, repository, "HEAD^{commit}")) != 0)
        {
          if (err != GIT_ENOTFOUND)
            return foundry_git_reject_last_error ();
        }
      else
        {
          if (git_object_peel ((git_object **)&parent, parent_obj, GIT_OBJECT_COMMIT) != 0)
            return foundry_git_reject_last_error ();
        }
    }

  if (!foundry_str_empty0 (state->signing_key))
    {
      git_buf commit_buffer = GIT_BUF_INIT;
      g_autofree char *signature = NULL;
      g_autofree char *message = NULL;
      g_autoptr(GError) error = NULL;
      g_autoptr(git_reference) head_ref = NULL;
      g_autoptr(git_reference) resolved_ref = NULL;
      int parent_count = parent != NULL ? 1 : 0;

      /* Ensure message has trailing newline like git does */
      message = ensure_message_trailing_newline (state->message);

      /* Step 1: Build the unsigned commit buffer */
      if (git_commit_create_buffer (&commit_buffer, repository, author, committer, NULL, message, tree, parent_count, (const git_commit **)&parent) != 0)
        return foundry_git_reject_last_error ();

      /* Step 2: Sign the buffer */
      signature = sign_commit_content ((const char *)commit_buffer.ptr, state->signing_key, state->signing_format, &error);
      if (signature == NULL)
        {
          git_buf_dispose (&commit_buffer);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }

      /* Step 3: Create the signed commit object */
      /* Use "gpgsig" for both GPG and SSH signatures (Git uses gpgsig field for all signature types) */
      if (git_commit_create_with_signature (&commit_oid, repository, commit_buffer.ptr, signature, "gpgsig") != 0)
        {
          git_buf_dispose (&commit_buffer);
          return foundry_git_reject_last_error ();
        }

      git_buf_dispose (&commit_buffer);

      /* Step 4: Update HEAD / branch ref */
      if ((err = git_repository_head (&head_ref, repository)) == 0)
        {
          /* Resolve symbolic reference to get the actual branch reference */
          if (git_reference_resolve (&resolved_ref, head_ref) == 0)
            {
              /* Move it to the new commit */
              if (git_reference_set_target (&resolved_ref, resolved_ref, &commit_oid, NULL) != 0)
                return foundry_git_reject_last_error ();
            }
          else
            {
              return foundry_git_reject_last_error ();
            }
        }
      else if (err == GIT_ENOTFOUND)
        {
          /* No HEAD exists, create default branch and HEAD */
          const char *default_branch = "refs/heads/main";

          if (git_reference_create (&head_ref, repository, default_branch, &commit_oid, 0, NULL) != 0)
            {
              /* Try master if main doesn't work */
              default_branch = "refs/heads/master";
              if (git_reference_create (&head_ref, repository, default_branch, &commit_oid, 0, NULL) != 0)
                return foundry_git_reject_last_error ();
            }

          g_clear_pointer (&head_ref, git_reference_free);

          /* Create symbolic HEAD pointing to the branch */
          if (git_reference_symbolic_create (&head_ref, repository, "HEAD", default_branch, 0, NULL) != 0)
            return foundry_git_reject_last_error ();
        }
      else
        {
          return foundry_git_reject_last_error ();
        }
    }
  else
    {
      g_autofree char *message = NULL;

      /* Ensure message has trailing newline like git does */
      message = ensure_message_trailing_newline (state->message);

      if (state->has_parent)
        {
          if (git_commit_create_v (&commit_oid, repository, "HEAD", author, committer, NULL, message, tree, 1, parent) != 0)
            return foundry_git_reject_last_error ();
        }
      else
        {
          if (parent != NULL)
            {
              if (git_commit_create_v (&commit_oid, repository, "HEAD", author, committer, NULL, message, tree, 1, parent) != 0)
                return foundry_git_reject_last_error ();
            }
          else
            {
              if (git_commit_create_v (&commit_oid, repository, "HEAD", author, committer, NULL, message, tree, 0) != 0)
                return foundry_git_reject_last_error ();
            }
        }
    }

  if (git_commit_lookup (&commit, repository, &commit_oid) != 0)
    return foundry_git_reject_last_error ();

  return dex_future_new_take_object (_foundry_git_commit_new (g_steal_pointer (&commit),
                                                              (GDestroyNotify) git_commit_free));
}

/**
 * foundry_git_commit_builder_commit:
 * @self: a [class@Foundry.GitCommitBuilder]
 *
 * Creates a commit using the fields from the builder.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.GitCommit] or rejects with error
 *
 * Since: 1.1
 */
DexFuture *
foundry_git_commit_builder_commit (FoundryGitCommitBuilder *self)
{
  BuilderCommit *state;
  git_oid parent_id;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));
  dex_return_error_if_fail (self->message != NULL);

  if (!foundry_git_commit_builder_get_can_commit (self))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Not enough information to commit");

  state = g_new0 (BuilderCommit, 1);
  state->paths = foundry_git_repository_paths_ref (self->paths);
  state->message = g_strdup (self->message);
  state->author_name = g_strdup (self->author_name);
  state->author_email = g_strdup (self->author_email);
  state->signing_key = g_strdup (self->signing_key);
  state->signing_format = g_strdup (self->signing_format);
  state->when = self->when ? g_date_time_ref (self->when) : NULL;

  if (self->parent != NULL)
    {
      _foundry_git_commit_get_oid (self->parent, &parent_id);
      state->parent_id = parent_id;
      state->has_parent = TRUE;
    }
  else
    {
      state->has_parent = FALSE;
    }

  return dex_thread_spawn ("[git-commit-builder-commit]",
                           foundry_git_commit_builder_commit_thread,
                           state,
                           (GDestroyNotify) builder_commit_free);
}

static void
foundry_git_commit_builder_refresh_diffs (FoundryGitCommitBuilder *self,
                                          git_repository          *repository,
                                          git_index               *index,
                                          git_tree                *tree)
{
  g_autoptr(git_diff) staged_diff = NULL;
  g_autoptr(git_diff) unstaged_diff = NULL;
  g_autoptr(FoundryGitDiff) new_staged_diff = NULL;
  g_autoptr(FoundryGitDiff) new_unstaged_diff = NULL;
  git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;
  int ret;

  diff_opts.context_lines = self->context_lines;

  /* Refresh staged diff (tree to index) */
  ret = git_diff_tree_to_index (&staged_diff, repository, tree, index, &diff_opts);
  if (ret == 0)
    new_staged_diff = _foundry_git_diff_new_with_paths (g_steal_pointer (&staged_diff), self->paths);

  /* Refresh unstaged diff (index to workdir) */
  ret = git_diff_index_to_workdir (&unstaged_diff, repository, index, &diff_opts);
  if (ret == 0)
    new_unstaged_diff = _foundry_git_diff_new_with_paths (g_steal_pointer (&unstaged_diff), self->paths);

  /* Lock mutex and update diffs atomically */
  g_mutex_lock (&self->mutex);
  g_set_object (&self->staged_diff, new_staged_diff);
  g_set_object (&self->unstaged_diff, new_unstaged_diff);
  g_mutex_unlock (&self->mutex);
}

static FoundryGitStatusEntry *
create_status_entry_from_diffs (const char     *relative_path,
                                FoundryGitDiff *staged_diff,
                                FoundryGitDiff *unstaged_diff)
{
  g_autoptr(FoundryGitStatusEntry) entry = NULL;
  gboolean has_staged = FALSE;
  gboolean has_unstaged = FALSE;
  git_status_t status = 0;
  git_status_entry status_entry = {0};
  git_diff_delta dummy_delta = {0};
  gsize n_deltas;
  gsize i;

  if (relative_path == NULL)
    return NULL;

  dummy_delta.new_file.path = relative_path;
  dummy_delta.old_file.path = relative_path;

  /* Check staged diff */
  if (staged_diff != NULL)
    {
      n_deltas = _foundry_git_diff_get_num_deltas (staged_diff);

      for (i = 0; i < n_deltas; i++)
        {
          const git_diff_delta *delta = _foundry_git_diff_get_delta (staged_diff, i);

          if (delta != NULL &&
              (g_strcmp0 (delta->new_file.path, relative_path) == 0 ||
               g_strcmp0 (delta->old_file.path, relative_path) == 0))
            {
              has_staged = TRUE;

              if (delta->status == GIT_DELTA_ADDED)
                status |= GIT_STATUS_INDEX_NEW;
              else if (delta->status == GIT_DELTA_MODIFIED)
                status |= GIT_STATUS_INDEX_MODIFIED;
              else if (delta->status == GIT_DELTA_DELETED)
                status |= GIT_STATUS_INDEX_DELETED;
              else if (delta->status == GIT_DELTA_RENAMED)
                status |= GIT_STATUS_INDEX_RENAMED;
              else if (delta->status == GIT_DELTA_TYPECHANGE)
                status |= GIT_STATUS_INDEX_TYPECHANGE;

              break;
            }
        }
    }

  /* Check unstaged diff */
  if (unstaged_diff != NULL)
    {
      n_deltas = _foundry_git_diff_get_num_deltas (unstaged_diff);

      for (i = 0; i < n_deltas; i++)
        {
          const git_diff_delta *delta = _foundry_git_diff_get_delta (unstaged_diff, i);

          if (delta != NULL &&
              (g_strcmp0 (delta->new_file.path, relative_path) == 0 ||
               g_strcmp0 (delta->old_file.path, relative_path) == 0))
            {
              has_unstaged = TRUE;

              if (delta->status == GIT_DELTA_ADDED)
                status |= GIT_STATUS_WT_NEW;
              else if (delta->status == GIT_DELTA_MODIFIED)
                status |= GIT_STATUS_WT_MODIFIED;
              else if (delta->status == GIT_DELTA_DELETED)
                status |= GIT_STATUS_WT_DELETED;
              else if (delta->status == GIT_DELTA_RENAMED)
                status |= GIT_STATUS_WT_RENAMED;
              else if (delta->status == GIT_DELTA_TYPECHANGE)
                status |= GIT_STATUS_WT_TYPECHANGE;

              break;
            }
        }
    }

  /* Create a minimal git_status_entry structure */
  if (has_staged)
    status_entry.head_to_index = &dummy_delta;
  if (has_unstaged)
    status_entry.index_to_workdir = &dummy_delta;

  status_entry.status = status;

  if (!(entry = _foundry_git_status_entry_new (&status_entry)))
    return NULL;

  return g_steal_pointer (&entry);
}

static void
update_list_store_remove (GListStore              *store,
                          FoundryGitCommitBuilder *self,
                          GFile                   *file)
{
  guint n_items;
  g_autofree char *file_relative_path = NULL;

  g_return_if_fail (G_IS_LIST_STORE (store));
  g_return_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));
  g_return_if_fail (G_IS_FILE (file));

  if (!(file_relative_path = foundry_git_repository_paths_get_workdir_relative_path (self->paths, file)))
    return;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (store));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryGitStatusEntry) item = g_list_model_get_item (G_LIST_MODEL (store), i);
      g_autofree char *item_path = NULL;

      if (item == NULL)
        continue;

      if ((item_path = foundry_git_status_entry_dup_path (item)) &&
          g_str_equal (item_path, file_relative_path))
        {
          g_list_store_remove (store, i);
          break;
        }
    }
}

static gboolean
update_list_store_contains_path (GListStore *store,
                                 const char *relative_path)
{
  guint n_items;

  g_return_val_if_fail (G_IS_LIST_STORE (store), FALSE);
  g_return_val_if_fail (relative_path != NULL, FALSE);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (store));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryGitStatusEntry) item = g_list_model_get_item (G_LIST_MODEL (store), i);
      g_autofree char *item_path = NULL;

      if (item == NULL)
        continue;

      if ((item_path = foundry_git_status_entry_dup_path (item)) &&
          g_str_equal (item_path, relative_path))
        return TRUE;
    }

  return FALSE;
}

static void
update_list_store_add (GListStore              *store,
                       FoundryGitCommitBuilder *self,
                       FoundryGitStatusEntry   *entry)
{
  g_autofree char *entry_path = NULL;

  g_return_if_fail (G_IS_LIST_STORE (store));
  g_return_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));
  g_return_if_fail (FOUNDRY_IS_GIT_STATUS_ENTRY (entry));

  if (!(entry_path = foundry_git_status_entry_dup_path (entry)))
    return;

  /* Check if entry already exists before adding */
  if (!update_list_store_contains_path (store, entry_path))
    g_list_store_append (store, entry);
}

typedef struct _UpdateStoresData
{
  FoundryGitCommitBuilder *self;
  GFile *file;
} UpdateStoresData;

static void
update_stores_data_free (UpdateStoresData *data)
{
  g_clear_object (&data->self);
  g_clear_object (&data->file);
  g_free (data);
}

static DexFuture *
update_list_stores_after_stage (DexFuture *completed,
                                gpointer   user_data)
{
  UpdateStoresData *data = user_data;
  g_autofree char *relative_path = NULL;
  g_autoptr(FoundryGitDiff) staged_diff = NULL;
  g_autoptr(FoundryGitDiff) unstaged_diff = NULL;
  gboolean in_staged = FALSE;
  gboolean in_unstaged = FALSE;
  gboolean old_can_commit;
  gboolean new_can_commit;

  g_assert (FOUNDRY_IS_GIT_COMMIT_BUILDER (data->self));
  g_assert (G_IS_FILE (data->file));

  if (!(relative_path = foundry_git_repository_paths_get_workdir_relative_path (data->self->paths, data->file)))
    return NULL;

  old_can_commit = foundry_git_commit_builder_get_can_commit (data->self);

  /* Get current diffs */
  g_mutex_lock (&data->self->mutex);
  if (data->self->staged_diff != NULL)
    staged_diff = g_object_ref (data->self->staged_diff);
  if (data->self->unstaged_diff != NULL)
    unstaged_diff = g_object_ref (data->self->unstaged_diff);
  g_mutex_unlock (&data->self->mutex);

  /* Check if file is in staged diff */
  if (staged_diff != NULL)
    in_staged = _foundry_git_diff_contains_file (staged_diff, relative_path);

  /* Check if file is in unstaged diff */
  if (unstaged_diff != NULL)
    in_unstaged = _foundry_git_diff_contains_file (unstaged_diff, relative_path);

  /* Remove from unstaged if it was there */
  if (!in_unstaged)
    update_list_store_remove (data->self->unstaged, data->self, data->file);

  /* Add to staged if it's in staged diff */
  if (in_staged)
    {
      g_autoptr(FoundryGitStatusEntry) entry = NULL;

      if ((entry = create_status_entry_from_diffs (relative_path, staged_diff, unstaged_diff)))
        update_list_store_add (data->self->staged, data->self, entry);
    }
  else
    update_list_store_remove (data->self->staged, data->self, data->file);

  /* For untracked: remove if it's now staged, otherwise keep it */
  if (in_staged)
    update_list_store_remove (data->self->untracked, data->self, data->file);

  new_can_commit = foundry_git_commit_builder_get_can_commit (data->self);
  if (old_can_commit != new_can_commit)
    g_object_notify_by_pspec (G_OBJECT (data->self), properties[PROP_CAN_COMMIT]);

  return NULL;
}

static DexFuture *
update_list_stores_after_unstage (DexFuture *completed,
                                  gpointer   user_data)
{
  UpdateStoresData *data = user_data;
  g_autofree char *relative_path = NULL;
  g_autoptr(FoundryGitDiff) staged_diff = NULL;
  g_autoptr(FoundryGitDiff) unstaged_diff = NULL;
  gboolean in_staged = FALSE;
  gboolean in_unstaged = FALSE;
  gboolean old_can_commit;
  gboolean new_can_commit;

  g_assert (FOUNDRY_IS_GIT_COMMIT_BUILDER (data->self));
  g_assert (G_IS_FILE (data->file));

  if (!(relative_path = foundry_git_repository_paths_get_workdir_relative_path (data->self->paths, data->file)))
    return NULL;

  old_can_commit = foundry_git_commit_builder_get_can_commit (data->self);

  /* Get current diffs */
  g_mutex_lock (&data->self->mutex);
  if (data->self->staged_diff != NULL)
    staged_diff = g_object_ref (data->self->staged_diff);
  if (data->self->unstaged_diff != NULL)
    unstaged_diff = g_object_ref (data->self->unstaged_diff);
  g_mutex_unlock (&data->self->mutex);

  /* Check if file is in staged diff */
  if (staged_diff != NULL)
    in_staged = _foundry_git_diff_contains_file (staged_diff, relative_path);

  /* Check if file is in unstaged diff */
  if (unstaged_diff != NULL)
    in_unstaged = _foundry_git_diff_contains_file (unstaged_diff, relative_path);

  /* Remove from staged if it was there */
  if (!in_staged)
    update_list_store_remove (data->self->staged, data->self, data->file);

  /* Add to unstaged if it's in unstaged diff */
  if (in_unstaged)
    {
      g_autoptr(FoundryGitStatusEntry) entry = NULL;

      if ((entry = create_status_entry_from_diffs (relative_path, staged_diff, unstaged_diff)))
        update_list_store_add (data->self->unstaged, data->self, entry);
    }
  else
    update_list_store_remove (data->self->unstaged, data->self, data->file);

  /* For untracked: if it's not in staged, it might be untracked */
  if (!in_staged)
    {
      /* If it's not in unstaged either, check if it was originally untracked */
      if (!in_unstaged)
        {
          g_autoptr(FoundryGitStatusEntry) entry = NULL;

          /* Check if file was originally untracked */
          if (foundry_git_commit_builder_is_untracked (data->self, data->file))
            {
              /* Create a status entry for untracked file */
              git_status_entry status_entry = {0};
              git_diff_delta dummy_delta = {0};

              dummy_delta.new_file.path = relative_path;
              status_entry.index_to_workdir = &dummy_delta;
              status_entry.status = GIT_STATUS_WT_NEW;

              entry = _foundry_git_status_entry_new (&status_entry);
              if (entry != NULL)
                update_list_store_add (data->self->untracked, data->self, entry);
            }
          else
            {
              /* Try to create from diffs (for files that were modified, not new) */
              if ((entry = create_status_entry_from_diffs (relative_path, staged_diff, unstaged_diff)))
                update_list_store_add (data->self->untracked, data->self, entry);
            }
        }
      else
        update_list_store_remove (data->self->untracked, data->self, data->file);
    }

  new_can_commit = foundry_git_commit_builder_get_can_commit (data->self);
  if (old_can_commit != new_can_commit)
    g_object_notify_by_pspec (G_OBJECT (data->self), properties[PROP_CAN_COMMIT]);

  return NULL;
}

typedef struct _StageFile
{
  FoundryGitCommitBuilder *self;
  GFile *file;
  FoundryGitRepositoryPaths *paths;
  FoundryGitDiff *unstaged_diff;
} StageFile;

static void
stage_file_free (StageFile *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->file);
  g_clear_pointer (&state->paths, foundry_git_repository_paths_unref);
  g_clear_object (&state->unstaged_diff);
  g_free (state);
}

static DexFuture *
foundry_git_commit_builder_stage_file_thread (gpointer user_data)
{
  StageFile *state = user_data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_index) index = NULL;
  g_autoptr(git_tree) tree = NULL;
  g_autoptr(git_blob) blob = NULL;
  g_autofree char *relative_path = NULL;
  gsize n_deltas;
  const git_diff_delta *delta = NULL;
  const char *buf = NULL;
  gsize buf_len = 0;
  git_index_entry entry;

  g_assert (FOUNDRY_IS_GIT_COMMIT_BUILDER (state->self));
  g_assert (G_IS_FILE (state->file));

  if (!(relative_path = foundry_git_repository_paths_get_workdir_relative_path (state->self->paths, state->file)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "File is not in working tree");

  if (state->unstaged_diff == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_INITIALIZED,
                                  "Commit builder not initialized");

  if (!foundry_git_repository_paths_open (state->paths, &repository, NULL))
    return foundry_git_reject_last_error ();

  if (git_repository_index (&index, repository) != 0)
    return foundry_git_reject_last_error ();

  /* Get parent tree for refreshing diffs */
  if (state->self->parent != NULL)
    {
      git_oid tree_id;

      if (_foundry_git_commit_get_tree_id (state->self->parent, &tree_id))
        {
          if (git_tree_lookup (&tree, repository, &tree_id) != 0)
            return foundry_git_reject_last_error ();
        }
    }

  n_deltas = _foundry_git_diff_get_num_deltas (state->unstaged_diff);

  /* Find the delta for this file in unstaged diff */
  for (gsize i = 0; i < n_deltas; i++)
    {
      const git_diff_delta *d = _foundry_git_diff_get_delta (state->unstaged_diff, i);

      if (d != NULL &&
          (g_strcmp0 (d->new_file.path, relative_path) == 0 ||
           g_strcmp0 (d->old_file.path, relative_path) == 0))
        {
          delta = d;
          break;
        }
    }

  /* If delta not found, file is probably untracked */
  if (delta == NULL)
    {
      if (!g_file_query_exists (state->file, NULL))
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_NOT_FOUND,
                                      "File does not exist");

      if (git_index_get_bypath (index, relative_path, 0) == NULL)
        {
          if (git_index_add_bypath (index, relative_path) != 0)
            return foundry_git_reject_last_error ();

          if (git_index_write (index) != 0)
            return foundry_git_reject_last_error ();

          foundry_git_commit_builder_refresh_diffs (state->self, repository, index, tree);

          return dex_future_new_true ();
        }

      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_NOT_FOUND,
                                    "Delta not found for file");
    }

  /* For staging, we want the "new" version (workdir state) */
  /* Try to get the blob for the new file if OID is set */
  if (!git_oid_is_zero (&delta->new_file.id))
    {
      if (git_blob_lookup (&blob, repository, &delta->new_file.id) == 0)
        {
          buf = git_blob_rawcontent (blob);
          buf_len = git_blob_rawsize (blob);
        }
    }

  /* If blob not available, the new file is in workdir - use git_index_add_bypath */
  if (buf == NULL)
    {
      /* Check if file was deleted */
      if (delta->status == GIT_DELTA_DELETED)
        {
          if (git_index_remove_bypath (index, relative_path) != 0)
            return foundry_git_reject_last_error ();

          if (git_index_write (index) != 0)
            return foundry_git_reject_last_error ();

          foundry_git_commit_builder_refresh_diffs (state->self, repository, index, tree);
          return dex_future_new_true ();
        }

      /* Use git_index_add_bypath which reads from workdir */
      /* This is the "new" version from the delta (workdir state) */
      if (git_index_add_bypath (index, relative_path) != 0)
        return foundry_git_reject_last_error ();

      if (git_index_write (index) != 0)
        return foundry_git_reject_last_error ();

      foundry_git_commit_builder_refresh_diffs (state->self, repository, index, tree);
      return dex_future_new_true ();
    }

  /* Stage the file with the new content */
  entry = (git_index_entry) {
    .mode = delta->new_file.mode != 0 ? delta->new_file.mode : GIT_FILEMODE_BLOB,
    .id = delta->new_file.id,
    .path = relative_path,
  };

  if (git_index_add_frombuffer (index, &entry, buf, buf_len) != 0)
    return foundry_git_reject_last_error ();

  if (git_index_write (index) != 0)
    return foundry_git_reject_last_error ();

  foundry_git_commit_builder_refresh_diffs (state->self, repository, index, tree);

  return dex_future_new_true ();
}

/**
 * foundry_git_commit_builder_stage_file:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @file: a [iface@Gio.File] in the working tree
 *
 * Stages the file using the version from the stored diff/delta.
 * This stages the fully applied version (all changes from the delta).
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any value
 *   or rejects with error
 *
 * Since: 1.1
 */
DexFuture *
foundry_git_commit_builder_stage_file (FoundryGitCommitBuilder *self,
                                       GFile                   *file)
{
  StageFile *state;
  UpdateStoresData *update_data;
  DexFuture *future;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));
  dex_return_error_if_fail (G_IS_FILE (file));

  state = g_new0 (StageFile, 1);
  state->self = g_object_ref (self);
  state->file = g_object_ref (file);
  state->paths = foundry_git_repository_paths_ref (self->paths);

  g_mutex_lock (&self->mutex);
  if (self->unstaged_diff != NULL)
    state->unstaged_diff = g_object_ref (self->unstaged_diff);
  g_mutex_unlock (&self->mutex);

  future = dex_thread_spawn ("[git-commit-builder-stage-file]",
                              foundry_git_commit_builder_stage_file_thread,
                              state,
                              (GDestroyNotify) stage_file_free);

  /* Chain callback to update list stores on main thread */
  update_data = g_new0 (UpdateStoresData, 1);
  update_data->self = g_object_ref (self);
  update_data->file = g_object_ref (file);

  return dex_future_then (future,
                          update_list_stores_after_stage,
                          update_data,
                          (GDestroyNotify) update_stores_data_free);
}

typedef struct _UnstageFile
{
  FoundryGitCommitBuilder *self;
  GFile *file;
  FoundryGitRepositoryPaths *paths;
  FoundryGitDiff *staged_diff;
} UnstageFile;

static void
unstage_file_free (UnstageFile *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->file);
  g_clear_pointer (&state->paths, foundry_git_repository_paths_unref);
  g_clear_object (&state->staged_diff);
  g_free (state);
}

static DexFuture *
foundry_git_commit_builder_unstage_file_thread (gpointer user_data)
{
  UnstageFile *state = user_data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_index) index = NULL;
  g_autoptr(git_tree) parent_tree = NULL;
  g_autoptr(git_tree_entry) tree_entry = NULL;
  g_autoptr(git_blob) blob = NULL;
  g_autofree char *relative_path = NULL;
  gsize n_deltas;
  const git_diff_delta *delta = NULL;
  const char *buf = NULL;
  gsize buf_len = 0;
  git_index_entry entry;
  int err;

  g_assert (FOUNDRY_IS_GIT_COMMIT_BUILDER (state->self));
  g_assert (G_IS_FILE (state->file));

  if (!(relative_path = foundry_git_repository_paths_get_workdir_relative_path (state->self->paths, state->file)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "File is not in working tree");

  if (state->staged_diff == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_INITIALIZED,
                                  "Commit builder not initialized");

  if (!foundry_git_repository_paths_open (state->paths, &repository, NULL))
    return foundry_git_reject_last_error ();

  if (git_repository_index (&index, repository) != 0)
    return foundry_git_reject_last_error ();

  /* Get parent tree for unstaging and refreshing diffs */
  if (state->self->parent != NULL)
    {
      git_oid tree_id;

      if (_foundry_git_commit_get_tree_id (state->self->parent, &tree_id))
        {
          if (git_tree_lookup (&parent_tree, repository, &tree_id) != 0)
            return foundry_git_reject_last_error ();
        }
    }

  n_deltas = _foundry_git_diff_get_num_deltas (state->staged_diff);

  /* Find the delta for this file in staged diff */
  for (gsize i = 0; i < n_deltas; i++)
    {
      const git_diff_delta *d = _foundry_git_diff_get_delta (state->staged_diff, i);

      if (d != NULL &&
          (g_strcmp0 (d->new_file.path, relative_path) == 0 ||
           g_strcmp0 (d->old_file.path, relative_path) == 0))
        {
          delta = d;
          break;
        }
    }

  if (delta == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "Delta not found for file");

  /* For unstaging, we want the "old" version (parent tree version) */
  if (parent_tree == NULL)
    {
      /* No parent, remove from index */
      if (git_index_remove_bypath (index, relative_path) != 0)
        return foundry_git_reject_last_error ();
    }
  else
    {
      /* Find the file in the parent tree */
      if ((err = git_tree_entry_bypath (&tree_entry, parent_tree, relative_path)))
        {
          if (err != GIT_ENOTFOUND)
            return foundry_git_reject_last_error ();

          /* File didn't exist in parent, remove from index */
          if (git_index_remove_bypath (index, relative_path) != 0)
            return foundry_git_reject_last_error ();
        }
      else
        {
          /* Get the blob content from the old file */
          if (git_blob_lookup (&blob, repository, git_tree_entry_id (tree_entry)) == 0)
            {
              buf = git_blob_rawcontent (blob);
              buf_len = git_blob_rawsize (blob);
            }

          entry = (git_index_entry) {
            .mode = git_tree_entry_filemode (tree_entry),
            .id = *git_tree_entry_id (tree_entry),
            .path = relative_path,
          };

          if (git_index_add_frombuffer (index, &entry, buf, buf_len) != 0)
            return foundry_git_reject_last_error ();
        }
    }

  if (git_index_write (index) != 0)
    return foundry_git_reject_last_error ();

  foundry_git_commit_builder_refresh_diffs (state->self, repository, index, parent_tree);

  return dex_future_new_true ();
}

/**
 * foundry_git_commit_builder_unstage_file:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @file: a [iface@Gio.File] in the working tree
 *
 * Unstages the file using the version from the stored diff/delta.
 * This restores the file to the fully unapplied version (HEAD version).
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any value
 *   or rejects with error
 *
 * Since: 1.1
 */
DexFuture *
foundry_git_commit_builder_unstage_file (FoundryGitCommitBuilder *self,
                                         GFile                   *file)
{
  UnstageFile *state;
  UpdateStoresData *update_data;
  DexFuture *future;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));
  dex_return_error_if_fail (G_IS_FILE (file));

  state = g_new0 (UnstageFile, 1);
  state->self = g_object_ref (self);
  state->file = g_object_ref (file);
  state->paths = foundry_git_repository_paths_ref (self->paths);

  g_mutex_lock (&self->mutex);
  if (self->staged_diff != NULL)
    state->staged_diff = g_object_ref (self->staged_diff);
  g_mutex_unlock (&self->mutex);

  future = dex_thread_spawn ("[git-commit-builder-unstage-file]",
                              foundry_git_commit_builder_unstage_file_thread,
                              state,
                              (GDestroyNotify) unstage_file_free);

  /* Chain callback to update list stores on main thread */
  update_data = g_new0 (UpdateStoresData, 1);
  update_data->self = g_object_ref (self);
  update_data->file = g_object_ref (file);

  return dex_future_then (future,
                          update_list_stores_after_unstage,
                          update_data,
                          (GDestroyNotify) update_stores_data_free);
}

/**
 * foundry_git_commit_builder_list_staged:
 * @self: a [class@Foundry.GitCommitBuilder]
 *
 * Gets a list model containing all files that are currently staged for commit.
 *
 * The list model contains [class@Foundry.GitStatusEntry] objects representing
 * files in the working tree that have been staged. The list is updated
 * automatically as files are staged or unstaged.
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of
 *   [class@Foundry.GitStatusEntry] objects representing staged files
 *
 * Since: 1.1
 */
GListModel *
foundry_git_commit_builder_list_staged (FoundryGitCommitBuilder *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->staged));
}

/**
 * foundry_git_commit_builder_list_unstaged:
 * @self: a [class@Foundry.GitCommitBuilder]
 *
 * Gets a list model containing all files that have unstaged changes.
 *
 * The list model contains [class@Foundry.GitStatusEntry] objects representing
 * files in the working tree that have been modified but not staged. The list
 * is updated automatically as files are staged or unstaged.
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of
 *   [class@Foundry.GitStatusEntry] objects representing files with unstaged
 *   changes
 *
 * Since: 1.1
 */
GListModel *
foundry_git_commit_builder_list_unstaged (FoundryGitCommitBuilder *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->unstaged));
}

/**
 * foundry_git_commit_builder_list_untracked:
 * @self: a [class@Foundry.GitCommitBuilder]
 *
 * Gets a list model containing all untracked files in the working tree.
 *
 * The list model contains [class@Foundry.GitStatusEntry] objects representing
 * files in the working tree that are not tracked by git. The list is updated
 * automatically as files are staged or untracked files are added.
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of
 *   [class@Foundry.GitStatusEntry] objects representing untracked files
 *
 * Since: 1.1
 */
GListModel *
foundry_git_commit_builder_list_untracked (FoundryGitCommitBuilder *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->untracked));
}

/**
 * foundry_git_commit_builder_is_untracked:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @file: a [iface@Gio.File] to check
 *
 * Checks whether @file was untracked when the commit builder was created.
 *
 * This method checks if the file was in the untracked files list at the
 * time the commit builder was initialized. Note that this reflects the
 * state when the builder was created, not the current git status.
 *
 * Returns: %TRUE if @file was untracked when the builder was created,
 *   %FALSE otherwise
 *
 * Since: 1.1
 */
gboolean
foundry_git_commit_builder_is_untracked (FoundryGitCommitBuilder *self,
                                         GFile                   *file)
{
  g_autofree char *relative_path = NULL;

  g_return_val_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (!(relative_path = foundry_git_repository_paths_get_workdir_relative_path (self->paths, file)))
    return FALSE;

  return g_hash_table_contains (self->initially_untracked, relative_path);
}

typedef struct _LoadDelta
{
  FoundryGitCommitBuilder *self;
  GFile *file;
  FoundryGitDiff *diff;
  guint is_staged : 1;
} LoadDelta;

static void
load_delta_free (LoadDelta *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->file);
  g_clear_object (&state->diff);
  g_free (state);
}

static DexFuture *
foundry_git_commit_builder_load_staged_delta_thread (gpointer user_data)
{
  LoadDelta *state = user_data;
  g_autofree char *relative_path = NULL;
  gsize n_deltas;

  g_assert (FOUNDRY_IS_GIT_COMMIT_BUILDER (state->self));
  g_assert (G_IS_FILE (state->file));

  if (!(relative_path = foundry_git_repository_paths_get_workdir_relative_path (state->self->paths, state->file)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "File is not in working tree");

  if (state->diff == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_INITIALIZED,
                                  "Commit builder not initialized");

  n_deltas = _foundry_git_diff_get_num_deltas (state->diff);

  /* Find the delta for this file in staged diff */
  for (gsize i = 0; i < n_deltas; i++)
    {
      const git_diff_delta *delta = _foundry_git_diff_get_delta (state->diff, i);

      if (delta != NULL &&
          (g_strcmp0 (delta->new_file.path, relative_path) == 0 ||
           g_strcmp0 (delta->old_file.path, relative_path) == 0))
        {
          g_autoptr(FoundryGitDelta) git_delta = _foundry_git_delta_new (state->diff, i);
          _foundry_git_delta_set_context_lines (git_delta, state->self->context_lines);
          return dex_future_new_take_object (g_steal_pointer (&git_delta));
        }
    }

  /* Delta not found in staged diff - might be an untracked file that was staged */
  /* Create a diff from NULL tree to the index blob for this file */
  {
    g_autoptr(git_repository) repository = NULL;
    g_autoptr(git_index) index = NULL;
    g_autoptr(git_diff) temp_diff = NULL;
    g_autoptr(FoundryGitDiff) temp_foundry_diff = NULL;
    const git_index_entry *index_entry = NULL;
    git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;
    int ret;

    if (!foundry_git_repository_paths_open (state->self->paths, &repository, NULL))
      return foundry_git_reject_last_error ();

    if (git_repository_index (&index, repository) != 0)
      return foundry_git_reject_last_error ();

    /* Check if file is in index */
    index_entry = git_index_get_bypath (index, relative_path, 0);
    if (index_entry == NULL || git_oid_is_zero (&index_entry->id))
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_NOT_FOUND,
                                    "Delta not found for file");

    diff_opts.context_lines = state->self->context_lines;
    {
      char *pathspec_array[] = { relative_path };
      diff_opts.pathspec.count = 1;
      diff_opts.pathspec.strings = pathspec_array;

      /* Create diff from NULL tree (empty) to index for this file */
      ret = git_diff_tree_to_index (&temp_diff, repository, NULL, index, &diff_opts);
    }
    if (ret != 0)
      return foundry_git_reject_last_error ();

    temp_foundry_diff = _foundry_git_diff_new_with_paths (g_steal_pointer (&temp_diff), state->self->paths);

    /* Find the delta in the temporary diff */
    n_deltas = _foundry_git_diff_get_num_deltas (temp_foundry_diff);
    for (gsize i = 0; i < n_deltas; i++)
      {
        const git_diff_delta *delta = _foundry_git_diff_get_delta (temp_foundry_diff, i);

        if (delta != NULL &&
            (g_strcmp0 (delta->new_file.path, relative_path) == 0 ||
             g_strcmp0 (delta->old_file.path, relative_path) == 0))
          {
            g_autoptr(FoundryGitDelta) git_delta = _foundry_git_delta_new (temp_foundry_diff, i);
            _foundry_git_delta_set_context_lines (git_delta, state->self->context_lines);
            return dex_future_new_take_object (g_steal_pointer (&git_delta));
          }
      }

    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "Delta not found for file");
  }
}

static DexFuture *
foundry_git_commit_builder_load_unstaged_delta_thread (gpointer user_data)
{
  LoadDelta *state = user_data;
  g_autofree char *relative_path = NULL;
  gsize n_deltas;

  g_assert (FOUNDRY_IS_GIT_COMMIT_BUILDER (state->self));
  g_assert (G_IS_FILE (state->file));

  if (!(relative_path = foundry_git_repository_paths_get_workdir_relative_path (state->self->paths, state->file)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "File is not in working tree");

  if (state->diff == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_INITIALIZED,
                                  "Commit builder not initialized");

  n_deltas = _foundry_git_diff_get_num_deltas (state->diff);

  /* Find the delta for this file in unstaged diff */
  for (gsize i = 0; i < n_deltas; i++)
    {
      const git_diff_delta *delta = _foundry_git_diff_get_delta (state->diff, i);

      if (delta != NULL &&
          (g_strcmp0 (delta->new_file.path, relative_path) == 0 ||
           g_strcmp0 (delta->old_file.path, relative_path) == 0))
        {
          g_autoptr(FoundryGitDelta) git_delta = _foundry_git_delta_new (state->diff, i);
          _foundry_git_delta_set_context_lines (git_delta, state->self->context_lines);
          return dex_future_new_take_object (g_steal_pointer (&git_delta));
        }
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_FOUND,
                                "Delta not found for file");
}

static DexFuture *
foundry_git_commit_builder_load_untracked_delta_thread (gpointer user_data)
{
  LoadDelta *state = user_data;
  g_autofree char *relative_path = NULL;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_index) index = NULL;
  g_autoptr(git_diff) temp_diff = NULL;
  g_autoptr(FoundryGitDiff) temp_foundry_diff = NULL;
  git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;
  int ret;

  g_assert (FOUNDRY_IS_GIT_COMMIT_BUILDER (state->self));
  g_assert (G_IS_FILE (state->file));

  if (!(relative_path = foundry_git_repository_paths_get_workdir_relative_path (state->self->paths, state->file)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "File is not in working tree");

  /* Verify file is untracked */
  if (!foundry_git_commit_builder_is_untracked (state->self, state->file))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "File is not untracked");

  if (!foundry_git_repository_paths_open (state->self->paths, &repository, NULL))
    return foundry_git_reject_last_error ();

  if (git_repository_index (&index, repository) != 0)
    return foundry_git_reject_last_error ();

  /* Create a diff from index to working directory for this untracked file */
  /* Since the file is untracked, it's not in the index, so the diff will show it as added */
  diff_opts.context_lines = state->self->context_lines;
  diff_opts.flags |= GIT_DIFF_INCLUDE_UNTRACKED;
  {
    char *pathspec_array[] = { relative_path };
    diff_opts.pathspec.count = 1;
    diff_opts.pathspec.strings = pathspec_array;

    /* Create diff from index to working directory - untracked files will appear as added */
    ret = git_diff_index_to_workdir (&temp_diff, repository, index, &diff_opts);
  }

  if (ret != 0)
    return foundry_git_reject_last_error ();

  temp_foundry_diff = _foundry_git_diff_new_with_paths (g_steal_pointer (&temp_diff), state->self->paths);

  /* Find the delta in the temporary diff */
  {
    gsize n_deltas = _foundry_git_diff_get_num_deltas (temp_foundry_diff);

    for (gsize i = 0; i < n_deltas; i++)
      {
        const git_diff_delta *delta = _foundry_git_diff_get_delta (temp_foundry_diff, i);

        if (delta != NULL &&
            (g_strcmp0 (delta->new_file.path, relative_path) == 0 ||
             g_strcmp0 (delta->old_file.path, relative_path) == 0))
          {
            g_autoptr(FoundryGitDelta) git_delta = _foundry_git_delta_new (temp_foundry_diff, i);
            _foundry_git_delta_set_context_lines (git_delta, state->self->context_lines);
            return dex_future_new_take_object (g_steal_pointer (&git_delta));
          }
      }
  }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_FOUND,
                                "Delta not found for file");
}

/**
 * foundry_git_commit_builder_load_staged_delta:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @file: a [iface@Gio.File] in the working tree
 *
 * Loads the delta for @file comparing the index against the parent commit.
 * This delta represents staged changes and can be used to toggle individual
 * lines on/off for staging in the background.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.GitDelta] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_git_commit_builder_load_staged_delta (FoundryGitCommitBuilder *self,
                                              GFile                   *file)
{
  LoadDelta *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));
  dex_return_error_if_fail (G_IS_FILE (file));

  state = g_new0 (LoadDelta, 1);
  state->self = g_object_ref (self);
  state->file = g_object_ref (file);
  state->is_staged = TRUE;

  g_mutex_lock (&self->mutex);
  if (self->staged_diff != NULL)
    state->diff = g_object_ref (self->staged_diff);
  g_mutex_unlock (&self->mutex);

  return dex_thread_spawn ("[git-commit-builder-load-staged-delta]",
                           foundry_git_commit_builder_load_staged_delta_thread,
                           state,
                           (GDestroyNotify) load_delta_free);
}

/**
 * foundry_git_commit_builder_load_unstaged_delta:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @file: a [iface@Gio.File] in the working tree
 *
 * Loads the delta for @file comparing the working directory against the index.
 * This delta represents unstaged changes and can be used to toggle individual
 * lines on/off for staging in the background.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.GitDelta] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_git_commit_builder_load_unstaged_delta (FoundryGitCommitBuilder *self,
                                                GFile                   *file)
{
  LoadDelta *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));
  dex_return_error_if_fail (G_IS_FILE (file));

  state = g_new0 (LoadDelta, 1);
  state->self = g_object_ref (self);
  state->file = g_object_ref (file);
  state->is_staged = FALSE;

  g_mutex_lock (&self->mutex);
  if (self->unstaged_diff != NULL)
    state->diff = g_object_ref (self->unstaged_diff);
  g_mutex_unlock (&self->mutex);

  return dex_thread_spawn ("[git-commit-builder-load-unstaged-delta]",
                           foundry_git_commit_builder_load_unstaged_delta_thread,
                           state,
                           (GDestroyNotify) load_delta_free);
}

/**
 * foundry_git_commit_builder_load_untracked_delta:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @file: a [iface@Gio.File] in the working tree
 *
 * Loads the delta for an untracked @file. This creates a synthetic delta
 * that represents adding the entire file, allowing the same API semantics
 * for new files as for modified files. The delta can be used to toggle
 * individual lines on/off for staging in the background.
 *
 * The file must be untracked (not in git's index) when the commit builder
 * was created. Since untracked files are new files, the delta will contain
 * a single hunk with all lines marked as additions.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.GitDelta] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_git_commit_builder_load_untracked_delta (FoundryGitCommitBuilder *self,
                                                 GFile                   *file)
{
  LoadDelta *state;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));
  dex_return_error_if_fail (G_IS_FILE (file));

  state = g_new0 (LoadDelta, 1);
  state->self = g_object_ref (self);
  state->file = g_object_ref (file);
  state->is_staged = FALSE;

  return dex_thread_spawn ("[git-commit-builder-load-untracked-delta]",
                           foundry_git_commit_builder_load_untracked_delta_thread,
                           state,
                           (GDestroyNotify) load_delta_free);
}

typedef struct _StageHunks
{
  FoundryGitCommitBuilder *self;
  GFile *file;
  FoundryGitRepositoryPaths *paths;
  FoundryGitDiff *unstaged_diff;
  GListModel *hunks;
} StageHunks;

static void
stage_hunks_free (StageHunks *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->file);
  g_clear_pointer (&state->paths, foundry_git_repository_paths_unref);
  g_clear_object (&state->unstaged_diff);
  g_clear_object (&state->hunks);
  g_free (state);
}

static gboolean
is_hunk_selected (GListModel      *selected_hunks,
                  FoundryGitPatch *patch,
                  gsize            hunk_idx)
{
  guint n_items = g_list_model_get_n_items (selected_hunks);
  const git_diff_delta *patch_delta = _foundry_git_patch_get_delta (patch);

  if (patch_delta == NULL)
    return FALSE;

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryGitDiffHunk) hunk = g_list_model_get_item (selected_hunks, i);
      g_autoptr(FoundryGitPatch) hunk_patch = NULL;
      const git_diff_delta *hunk_delta = NULL;
      gsize hunk_hunk_idx;

      if (hunk == NULL)
        continue;

      hunk_patch = _foundry_git_diff_hunk_get_patch (hunk);
      hunk_hunk_idx = _foundry_git_diff_hunk_get_hunk_idx (hunk);

      if (hunk_patch != NULL)
        hunk_delta = _foundry_git_patch_get_delta (hunk_patch);

      if (hunk_delta != NULL &&
          patch_delta != NULL &&
          hunk_hunk_idx == hunk_idx &&
          (g_strcmp0 (hunk_delta->new_file.path, patch_delta->new_file.path) == 0 ||
           g_strcmp0 (hunk_delta->old_file.path, patch_delta->old_file.path) == 0))
        return TRUE;
    }

  return FALSE;
}

static gboolean
is_line_selected (GListModel      *selected_lines,
                  FoundryGitPatch *patch,
                  gsize            hunk_idx,
                  gsize            line_idx)
{
  guint n_items = g_list_model_get_n_items (selected_lines);
  const git_diff_delta *patch_delta = _foundry_git_patch_get_delta (patch);

  if (patch_delta == NULL)
    return FALSE;

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryGitDiffLine) line = g_list_model_get_item (selected_lines, i);
      g_autoptr(FoundryGitPatch) line_patch = NULL;
      const git_diff_delta *line_delta = NULL;
      gsize line_hunk_idx;
      gsize line_line_idx;

      if (line == NULL)
        continue;

      line_patch = _foundry_git_diff_line_get_patch (line);
      line_hunk_idx = _foundry_git_diff_line_get_hunk_idx (line);
      line_line_idx = _foundry_git_diff_line_get_line_idx (line);

      if (line_patch != NULL)
        line_delta = _foundry_git_patch_get_delta (line_patch);

      if (line_delta != NULL &&
          patch_delta != NULL &&
          line_hunk_idx == hunk_idx &&
          line_line_idx == line_idx &&
          (g_strcmp0 (line_delta->new_file.path, patch_delta->new_file.path) == 0 ||
           g_strcmp0 (line_delta->old_file.path, patch_delta->old_file.path) == 0))
        return TRUE;
    }

  return FALSE;
}

static char *
apply_selected_hunks_to_content (const char      *old_content,
                                 gsize            old_len,
                                 FoundryGitPatch *patch,
                                 GListModel      *selected_hunks,
                                 gboolean         invert,
                                 gboolean         target_ends_with_newline)
{
  g_autoptr(GString) result = NULL;
  g_auto(GStrv) old_lines = NULL;
  gsize num_hunks;
  gsize old_line = 1;
  gsize old_line_count = 0;
  gboolean original_ends_with_newline = FALSE;

  g_assert (patch != NULL);
  g_assert (selected_hunks != NULL);

  result = g_string_new (NULL);
  num_hunks = _foundry_git_patch_get_num_hunks (patch);

  /* Check if original content ends with newline */
  if (old_content != NULL && old_len > 0)
    original_ends_with_newline = (old_content[old_len - 1] == '\n');

  /* Split old content into lines */
  if (old_content != NULL && old_len > 0)
    {
      g_autofree char *old_str = g_strndup (old_content, old_len);
      old_lines = g_strsplit (old_str, "\n", -1);
    }
  else
    {
      old_lines = g_new0 (char *, 1);
    }
  if (old_lines != NULL)
    {
      for (guint i = 0; old_lines[i] != NULL; i++)
        old_line_count++;
    }

  for (gsize hunk_idx = 0; hunk_idx < num_hunks; hunk_idx++)
    {
      const git_diff_hunk *hunk = NULL;
      gsize num_lines_in_hunk;
      gboolean hunk_selected;

      hunk = _foundry_git_patch_get_hunk (patch, hunk_idx);
      if (hunk == NULL)
        continue;

      hunk_selected = is_hunk_selected (selected_hunks, patch, hunk_idx);
      if (invert)
        hunk_selected = !hunk_selected;
      num_lines_in_hunk = _foundry_git_patch_get_num_lines_in_hunk (patch, hunk_idx);

      /* Add lines before this hunk */
      while (old_line < (gsize)hunk->old_start && old_line <= old_line_count)
        {
          if (old_lines != NULL && old_lines[old_line - 1] != NULL)
            {
              /* Check if this will be the last line (last hunk, no lines in hunk, no remaining old lines) */
              gboolean is_last_line = (hunk_idx == num_hunks - 1 &&
                                       num_lines_in_hunk == 0 &&
                                       old_line == old_line_count);
              g_string_append (result, old_lines[old_line - 1]);
              /* Only add newline if not the last line, or if target should end with newline */
              if (!is_last_line || target_ends_with_newline)
                g_string_append_c (result, '\n');
            }
          old_line++;
        }

      if (hunk_selected)
        {
          /* Apply the hunk - add new lines */
          for (gsize line_idx = 0; line_idx < num_lines_in_hunk; line_idx++)
            {
              const git_diff_line *line = _foundry_git_patch_get_line (patch, hunk_idx, line_idx);

              if (line == NULL)
                continue;

              if (line->origin == '+' || line->origin == ' ' || line->origin == '>')
                {
                  /* Check if this will be the last line in the result */
                  gboolean is_last_line = (hunk_idx == num_hunks - 1 &&
                                           line_idx == num_lines_in_hunk - 1 &&
                                           old_line > old_line_count);
                  gboolean line_has_newline = (line->content_len > 0 && line->content[line->content_len - 1] == '\n');

                  g_string_append_len (result, (const char *)line->content, line->content_len);
                  /* Add newline if:
                   * - Line doesn't already have one AND
                   * - (It's not the last line OR target should end with newline)
                   */
                  if (!line_has_newline && (!is_last_line || target_ends_with_newline))
                    g_string_append_c (result, '\n');
                }
              else if (line->origin == '-')
                {
                  /* Skip deleted lines */
                }
            }

          /* Skip old lines that were in this hunk */
          for (gsize i = 0; i < (gsize)hunk->old_lines; i++)
            {
              if (old_line <= old_line_count)
                old_line++;
            }
        }
      else
        {
          /* Don't apply hunk - keep old lines */
          for (gsize i = 0; i < (gsize)hunk->old_lines; i++)
            {
              if (old_line <= old_line_count && old_lines != NULL && old_lines[old_line - 1] != NULL)
                {
                  /* Check if this is the last real line before empty string */
                  gboolean is_last_real_line = (hunk_idx == num_hunks - 1 &&
                                               i == hunk->old_lines - 1 &&
                                               old_line == old_line_count - 1);
                  gboolean has_empty_string_after = (original_ends_with_newline &&
                                                     old_lines[old_line_count] != NULL &&
                                                     strlen (old_lines[old_line_count]) == 0);
                  gboolean will_add_newline = TRUE;

                  if (is_last_real_line && has_empty_string_after)
                    {
                      /* Last real line with empty string after - skip newline here */
                      will_add_newline = FALSE;
                    }
                  else if (is_last_real_line && old_line == old_line_count)
                    {
                      /* Last line and no empty string - add newline if target should have one */
                      will_add_newline = target_ends_with_newline;
                    }

                  g_string_append (result, old_lines[old_line - 1]);
                  if (will_add_newline)
                    g_string_append_c (result, '\n');
                }
              old_line++;
            }
        }
    }

  /* Add remaining old lines */
  while (old_line <= old_line_count)
    {
      if (old_lines != NULL && old_lines[old_line - 1] != NULL)
        {
          gboolean is_last_line = (old_line == old_line_count);
          const char *line_content = old_lines[old_line - 1];
          gsize line_len = strlen (line_content);
          /* Check if this is the empty string from g_strsplit when original ended with newline */
          gboolean is_empty_from_trailing_newline = (is_last_line && line_len == 0 && original_ends_with_newline);

          if (is_empty_from_trailing_newline)
            {
              /* This is the empty string from trailing newline in the original content.
               * The empty string represents the trailing newline that was already in the original.
               * We need to ensure the result ends correctly based on target_ends_with_newline.
               */
              gboolean result_ends_with_newline = (result->len > 0 && result->str[result->len - 1] == '\n');

              if (result_ends_with_newline && !target_ends_with_newline)
                {
                  /* Result has newline but target shouldn't - remove it */
                  g_string_truncate (result, result->len - 1);
                }
              else if (!result_ends_with_newline && target_ends_with_newline)
                {
                  /* Result doesn't have newline but target should - add it */
                  g_string_append_c (result, '\n');
                }
            }
          else
            {
              /* Regular line - add it with appropriate newline.
               * Check if this is the last real line (before the potential empty string).
               */
              gboolean is_last_real_line = (old_line == old_line_count - 1);
              /* Check if there's an empty string after this line */
              gboolean has_empty_string_after = (original_ends_with_newline &&
                                                 old_lines[old_line_count] != NULL &&
                                                 strlen (old_lines[old_line_count]) == 0);
              gboolean will_add_newline;

              if (is_last_real_line && has_empty_string_after)
                {
                  /* This is the last real line and there's an empty string after it */
                  /* Don't add newline here - the empty string handling will add it if needed */
                  will_add_newline = FALSE;
                }
              else if (is_last_real_line)
                {
                  /* This is the last real line and there's no empty string after */
                  /* Add newline if target should have one */
                  will_add_newline = target_ends_with_newline;
                }
              else
                {
                  /* Not the last line - always add newline */
                  will_add_newline = TRUE;
                }

              g_string_append (result, line_content);
              if (will_add_newline)
                g_string_append_c (result, '\n');
            }
        }
      old_line++;
    }
  return g_string_free (g_steal_pointer (&result), FALSE);
}

static char *
apply_selected_lines_to_content (const char      *old_content,
                                 gsize            old_len,
                                 FoundryGitPatch *patch,
                                 GListModel      *selected_lines,
                                 gboolean         invert,
                                 gboolean         target_ends_with_newline)
{
  g_autoptr(GString) result = NULL;
  gsize num_hunks;
  gsize old_line = 1;
  g_auto(GStrv) old_lines = NULL;
  gsize old_line_count = 0;
  gboolean original_ends_with_newline = FALSE;

  g_assert (patch != NULL);
  g_assert (selected_lines != NULL);

  result = g_string_new (NULL);
  num_hunks = _foundry_git_patch_get_num_hunks (patch);

  /* Check if original content ends with newline */
  if (old_content != NULL && old_len > 0)
    original_ends_with_newline = (old_content[old_len - 1] == '\n');

  /* Split old content into lines */
  if (old_content != NULL && old_len > 0)
    {
      g_autofree char *old_str = g_strndup (old_content, old_len);
      old_lines = g_strsplit (old_str, "\n", -1);
    }
  else
    {
      old_lines = g_new0 (char *, 1);
    }

  if (old_lines != NULL)
    old_line_count += g_strv_length (old_lines);

  for (gsize hunk_idx = 0; hunk_idx < num_hunks; hunk_idx++)
    {
      const git_diff_hunk *hunk = NULL;
      gsize num_lines_in_hunk;

      hunk = _foundry_git_patch_get_hunk (patch, hunk_idx);
      if (hunk == NULL)
        continue;

      num_lines_in_hunk = _foundry_git_patch_get_num_lines_in_hunk (patch, hunk_idx);

      /* Add lines before this hunk */
      while (old_line < (gsize)hunk->old_start && old_line <= old_line_count)
        {
          if (old_lines != NULL && old_lines[old_line - 1] != NULL)
            {
              /* Check if this will be the last line (last hunk, no lines in hunk, no remaining old lines) */
              gboolean is_last_line = (hunk_idx == num_hunks - 1 &&
                                       num_lines_in_hunk == 0 &&
                                       old_line == old_line_count);
              g_string_append (result, old_lines[old_line - 1]);
              /* Only add newline if not the last line, or if target should end with newline */
              if (!is_last_line || target_ends_with_newline)
                g_string_append_c (result, '\n');
            }
          old_line++;
        }

      /* Process lines in hunk */
      for (gsize line_idx = 0; line_idx < num_lines_in_hunk; line_idx++)
        {
          const git_diff_line *line = NULL;
          gboolean line_selected;
          gsize old_line_after_this_line = old_line;

          line = _foundry_git_patch_get_line (patch, hunk_idx, line_idx);
          if (line == NULL)
            continue;

          line_selected = is_line_selected (selected_lines, patch, hunk_idx, line_idx);
          if (invert)
            line_selected = !line_selected;

          /* Calculate what old_line will be after processing this line */
          if (line->origin == '-' && line_selected)
            old_line_after_this_line++;
          else if (line->origin == ' ' || line->origin == '=')
            old_line_after_this_line++;
          else if (line->origin == '-' && !line_selected)
            old_line_after_this_line++;

          if (line->origin == '+' && line_selected)
            {
              /* Check if this will be the last line in the result.
               *
               * It's the last line if:
               * - We're in the last hunk
               * - This is the last line in that hunk
               * - There are no more old lines after processing this line
               * - There are no more lines after this in the hunk that will be added
               */
              gboolean is_last_line = (hunk_idx == num_hunks - 1 &&
                                       line_idx == num_lines_in_hunk - 1 &&
                                       old_line_after_this_line > old_line_count);
              gboolean line_has_newline = (line->content_len > 0 && line->content[line->content_len - 1] == '\n');

              /* Add new line */
              g_string_append_len (result, (const char *)line->content, line->content_len);

              /* Add newline if:
               * - Line doesn't already have one AND
               * - (It's not the last line OR target should end with newline)
               */
              if (!line_has_newline && (!is_last_line || target_ends_with_newline))
                g_string_append_c (result, '\n');
            }
          else if (line->origin == '-' && line_selected)
            {
              /* Remove old line - skip it */
              if (old_line <= old_line_count)
                old_line++;
            }
          else if (line->origin == ' ' || line->origin == '=')
            {
              /* Context line - keep it */
              if (old_line <= old_line_count && old_lines != NULL && old_lines[old_line - 1] != NULL)
                {
                  gboolean is_last_line = (hunk_idx == num_hunks - 1 &&
                                           line_idx == num_lines_in_hunk - 1 &&
                                           old_line_after_this_line > old_line_count);

                  g_string_append (result, old_lines[old_line - 1]);

                  /* Only add newline if not the last line, or if target should end with newline */
                  if (!is_last_line || target_ends_with_newline)
                    g_string_append_c (result, '\n');
                }

              old_line++;
            }
          else if (line->origin == '+' && !line_selected)
            {
              /* New line not selected - skip it */
            }
          else if (line->origin == '-' && !line_selected)
            {
              /* Old line not selected - keep it */
              if (old_line <= old_line_count && old_lines != NULL && old_lines[old_line - 1] != NULL)
                {
                  gboolean is_last_line = (hunk_idx == num_hunks - 1 &&
                                           line_idx == num_lines_in_hunk - 1 &&
                                           old_line_after_this_line > old_line_count);
                  g_string_append (result, old_lines[old_line - 1]);
                  /* Only add newline if not the last line, or if target should end with newline */
                  if (!is_last_line || target_ends_with_newline)
                    g_string_append_c (result, '\n');
                }

              old_line++;
            }
        }
    }

  /* Add remaining old lines */
  while (old_line <= old_line_count)
    {
      if (old_lines != NULL && old_lines[old_line - 1] != NULL)
        {
          gboolean is_last_line = (old_line == old_line_count);
          const char *line_content = old_lines[old_line - 1];
          gsize line_len = strlen (line_content);
          /* Check if this is the empty string from g_strsplit when original ended with newline */
          gboolean is_empty_from_trailing_newline = (is_last_line && line_len == 0 && original_ends_with_newline);

          if (is_empty_from_trailing_newline)
            {
              /* This is the empty string from trailing newline in the original content */
              /* The empty string represents the trailing newline that was already in the original */
              /* We need to ensure the result ends correctly based on target_ends_with_newline */
              /* Check current state of result */
              gboolean result_ends_with_newline = (result->len > 0 && result->str[result->len - 1] == '\n');

              if (result_ends_with_newline && !target_ends_with_newline)
                {
                  /* Result has newline but target shouldn't - remove it */
                  g_string_truncate (result, result->len - 1);
                }
              else if (!result_ends_with_newline && target_ends_with_newline)
                {
                  /* Result doesn't have newline but target should - add it */
                  g_string_append_c (result, '\n');
                }
            }
          else
            {
              /* Regular line - add it with appropriate newline */
              /* Check if this is the last real line (before the potential empty string) */
              gboolean is_last_real_line = (old_line == old_line_count - 1);
              /* Check if there's an empty string after this line */
              gboolean has_empty_string_after = (original_ends_with_newline &&
                                                 old_lines[old_line_count] != NULL &&
                                                 strlen (old_lines[old_line_count]) == 0);
              gboolean will_add_newline;

              if (is_last_real_line && has_empty_string_after)
                {
                  /* This is the last real line and there's an empty string after it */
                  /* Don't add newline here - the empty string handling will add it if needed */
                  will_add_newline = FALSE;
                }
              else if (is_last_real_line)
                {
                  /* This is the last real line and there's no empty string after */
                  /* Add newline if target should have one */
                  will_add_newline = target_ends_with_newline;
                }
              else
                {
                  /* Not the last line - always add newline */
                  will_add_newline = TRUE;
                }

              g_string_append (result, line_content);
              if (will_add_newline)
                g_string_append_c (result, '\n');
            }
        }

      old_line++;
    }

  return g_string_free (g_steal_pointer (&result), FALSE);
}

typedef enum
{
  STAGE_OPERATION_STAGE,
  STAGE_OPERATION_UNSTAGE
} StageOperation;

static gboolean
find_delta_for_file (FoundryGitDiff        *diff,
                     const char            *relative_path,
                     const git_diff_delta **delta_out,
                     gsize                 *delta_idx_out,
                     GError               **error)
{
  gsize n_deltas;
  const git_diff_delta *delta = NULL;
  gsize delta_idx = G_MAXSIZE;

  g_assert (diff != NULL);
  g_assert (relative_path != NULL);
  g_assert (delta_out != NULL);
  g_assert (delta_idx_out != NULL);

  n_deltas = _foundry_git_diff_get_num_deltas (diff);
  for (gsize i = 0; i < n_deltas; i++)
    {
      const git_diff_delta *d = _foundry_git_diff_get_delta (diff, i);

      if (d != NULL &&
          (g_strcmp0 (d->new_file.path, relative_path) == 0 ||
           g_strcmp0 (d->old_file.path, relative_path) == 0))
        {
          delta = d;
          delta_idx = i;
          break;
        }
    }

  if (delta == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "Delta not found for file");
      return FALSE;
    }

  *delta_out = delta;
  *delta_idx_out = delta_idx;

  return TRUE;
}

static gboolean
setup_repository_context (FoundryGitCommitBuilder  *self,
                          FoundryGitRepositoryPaths *paths,
                          git_repository          **repository_out,
                          git_index               **index_out,
                          git_tree                **tree_out,
                          GError                  **error)
{
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_index) index = NULL;
  g_autoptr(git_tree) tree = NULL;
  const git_error *git_err;

  g_assert (self != NULL);
  g_assert (paths != NULL);
  g_assert (repository_out != NULL);
  g_assert (index_out != NULL);
  g_assert (tree_out != NULL);

  if (!foundry_git_repository_paths_open (paths, &repository, error))
    return FALSE;

  if (git_repository_index (&index, repository) != 0)
    {
      git_err = git_error_last ();
      g_set_error (error,
                   FOUNDRY_GIT_ERROR,
                   git_err->klass,
                   "%s", git_err->message);
      return FALSE;
    }

  /* Get parent tree for reading old content */
  if (self->parent != NULL)
    {
      git_oid tree_id;

      if (_foundry_git_commit_get_tree_id (self->parent, &tree_id))
        {
          if (git_tree_lookup (&tree, repository, &tree_id) != 0)
            {
              git_err = git_error_last ();
              g_set_error (error,
                           FOUNDRY_GIT_ERROR,
                           git_err->klass,
                           "%s", git_err->message);
              return FALSE;
            }
        }
    }

  *repository_out = g_steal_pointer (&repository);
  *index_out = g_steal_pointer (&index);
  *tree_out = g_steal_pointer (&tree);

  return TRUE;
}

static void
get_old_content_for_stage (git_repository  *repository,
                           git_index       *index,
                           git_tree        *tree,
                           const char      *relative_path,
                           const char     **old_buf_out,
                           gsize           *old_buf_len_out,
                           git_blob       **old_blob_out,
                           GBytes         **workdir_contents_for_untracked_out)
{
  g_autoptr(git_blob) old_blob = NULL;
  g_autoptr(git_tree_entry) tree_entry = NULL;
  g_autoptr(GBytes) workdir_contents_for_untracked = NULL;
  const char *old_buf = NULL;
  gsize old_buf_len = 0;

  g_assert (repository != NULL);
  g_assert (index != NULL);
  g_assert (relative_path != NULL);
  g_assert (old_buf_out != NULL);
  g_assert (old_buf_len_out != NULL);
  g_assert (old_blob_out != NULL);
  g_assert (workdir_contents_for_untracked_out != NULL);

  /* Get old content from index */
  {
    const git_index_entry *index_entry = git_index_get_bypath (index, relative_path, 0);

    if (index_entry != NULL && !git_oid_is_zero (&index_entry->id))
      {
        if (git_blob_lookup (&old_blob, repository, &index_entry->id) == 0)
          {
            old_buf = git_blob_rawcontent (old_blob);
            old_buf_len = git_blob_rawsize (old_blob);
          }
      }
    else if (tree != NULL)
      {
        /* Try parent tree */
        if (git_tree_entry_bypath (&tree_entry, tree, relative_path) == 0)
          {
            if (git_blob_lookup (&old_blob, repository, git_tree_entry_id (tree_entry)) == 0)
              {
                old_buf = git_blob_rawcontent (old_blob);
                old_buf_len = git_blob_rawsize (old_blob);
              }
          }
      }
  }

  *old_buf_out = old_buf;
  *old_buf_len_out = old_buf_len;
  *old_blob_out = g_steal_pointer (&old_blob);
  *workdir_contents_for_untracked_out = g_steal_pointer (&workdir_contents_for_untracked);
}

static void
get_old_content_for_unstage (git_repository  *repository,
                             git_tree        *parent_tree,
                             const char      *relative_path,
                             const char     **old_buf_out,
                             gsize           *old_buf_len_out,
                             git_blob       **old_blob_out)
{
  g_autoptr(git_blob) old_blob = NULL;
  g_autoptr(git_tree_entry) tree_entry = NULL;
  const char *old_buf = NULL;
  gsize old_buf_len = 0;

  g_assert (repository != NULL);
  g_assert (relative_path != NULL);
  g_assert (old_buf_out != NULL);
  g_assert (old_buf_len_out != NULL);
  g_assert (old_blob_out != NULL);

  /* Get old content from parent tree */
  if (parent_tree != NULL)
    {
      if (git_tree_entry_bypath (&tree_entry, parent_tree, relative_path) == 0)
        {
          if (git_blob_lookup (&old_blob, repository, git_tree_entry_id (tree_entry)) == 0)
            {
              old_buf = git_blob_rawcontent (old_blob);
              old_buf_len = git_blob_rawsize (old_blob);
            }
        }
    }

  *old_buf_out = old_buf;
  *old_buf_len_out = old_buf_len;
  *old_blob_out = g_steal_pointer (&old_blob);
}

static gboolean
refresh_diff_and_refind_delta (FoundryGitCommitBuilder  *self,
                               FoundryGitDiff          **diff_inout,
                               StageOperation            operation,
                               git_repository           *repository,
                               git_index                *index,
                               git_tree                 *tree,
                               const char               *relative_path,
                               const git_diff_delta    **delta_out,
                               gsize                    *delta_idx_out,
                               GError                  **error)
{
  const git_diff_delta *delta = NULL;
  gsize delta_idx = G_MAXSIZE;

  g_assert (self != NULL);
  g_assert (diff_inout != NULL);
  g_assert (*diff_inout != NULL);
  g_assert (repository != NULL);
  g_assert (index != NULL);
  g_assert (relative_path != NULL);
  g_assert (delta_out != NULL);
  g_assert (delta_idx_out != NULL);

  /* Refresh the diff before creating patch to ensure hash algorithm matches */
  foundry_git_commit_builder_refresh_diffs (self, repository, index, tree);

  /* Re-acquire the refreshed diff */
  g_mutex_lock (&self->mutex);
  g_clear_object (diff_inout);
  if (operation == STAGE_OPERATION_STAGE)
    {
      if (self->unstaged_diff != NULL)
        *diff_inout = g_object_ref (self->unstaged_diff);
    }
  else
    {
      if (self->staged_diff != NULL)
        *diff_inout = g_object_ref (self->staged_diff);
    }
  g_mutex_unlock (&self->mutex);

  if (*diff_inout == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_INITIALIZED,
                   "Diff not available after refresh");
      return FALSE;
    }

  /* Re-find the delta after refresh */
  if (!find_delta_for_file (*diff_inout, relative_path, &delta, &delta_idx, error))
    return FALSE;

  *delta_out = delta;
  *delta_idx_out = delta_idx;

  return TRUE;
}

static gboolean
create_patch_and_determine_newline (FoundryGitDiff           *diff,
                                    gsize                     delta_idx,
                                    FoundryGitCommitBuilder  *self,
                                    const char               *relative_path,
                                    const char               *old_buf,
                                    gsize                     old_buf_len,
                                    StageOperation            operation,
                                    gboolean                 *target_ends_with_newline_out,
                                    FoundryGitPatch         **git_patch_out,
                                    const char              **old_buf_out,
                                    gsize                    *old_buf_len_out,
                                    GBytes                  **workdir_contents_for_untracked_out,
                                    GError                  **error)
{
  g_autoptr(git_patch) patch = NULL;
  g_autoptr(FoundryGitPatch) git_patch = NULL;
  g_autoptr(GBytes) workdir_contents_for_untracked = NULL;
  gboolean target_ends_with_newline = FALSE;
  const char *effective_old_buf = old_buf;
  gsize effective_old_buf_len = old_buf_len;
  const git_error *git_err;

  g_assert (diff != NULL);
  g_assert (self != NULL);
  g_assert (relative_path != NULL);
  g_assert (target_ends_with_newline_out != NULL);
  g_assert (git_patch_out != NULL);
  g_assert (old_buf_out != NULL);
  g_assert (old_buf_len_out != NULL);

  /* workdir_contents_for_untracked_out can be NULL for unstage operations */

  if (_foundry_git_diff_patch_from_diff (diff, &patch, delta_idx) != 0)
    {
      git_err = git_error_last ();
      g_set_error (error,
                   FOUNDRY_GIT_ERROR,
                   git_err->klass,
                   "%s", git_err->message);
      return FALSE;
    }

  git_patch = _foundry_git_patch_new (g_steal_pointer (&patch));

  if (operation == STAGE_OPERATION_STAGE)
    {
      /* Read working directory file to determine trailing newline behavior */
      /* For untracked files, use workdir content as the base */
      g_autoptr(GFile) workdir_file = NULL;
      g_autoptr(GBytes) workdir_contents = NULL;
      const char *workdir_buf = NULL;
      gsize workdir_len = 0;

      workdir_file = foundry_git_repository_paths_get_workdir_file (self->paths, relative_path);
      workdir_contents = g_file_load_bytes (workdir_file, NULL, NULL, NULL);

      if (workdir_contents != NULL)
        {
          workdir_buf = g_bytes_get_data (workdir_contents, &workdir_len);

          if (workdir_len > 0)
            target_ends_with_newline = (workdir_buf[workdir_len - 1] == '\n');
        }

      /* For untracked files (old_buf is NULL), use workdir content as base */
      if (effective_old_buf == NULL && workdir_buf != NULL)
        {
          effective_old_buf = workdir_buf;
          effective_old_buf_len = workdir_len;

          /* Keep workdir_contents alive to prevent freeing the buffer */
          workdir_contents_for_untracked = g_steal_pointer (&workdir_contents);
        }
    }
  else
    {
      /* Determine trailing newline from parent tree (what we're unstaging to) */
      if (effective_old_buf != NULL && effective_old_buf_len > 0)
        target_ends_with_newline = (effective_old_buf[effective_old_buf_len - 1] == '\n');
    }

  *target_ends_with_newline_out = target_ends_with_newline;
  *git_patch_out = g_steal_pointer (&git_patch);
  *old_buf_out = effective_old_buf;
  *old_buf_len_out = effective_old_buf_len;

  if (workdir_contents_for_untracked_out != NULL)
    *workdir_contents_for_untracked_out = g_steal_pointer (&workdir_contents_for_untracked);

  return TRUE;
}

static gboolean
write_merged_content_to_index (git_repository           *repository,
                               git_index                *index,
                               git_tree                 *tree,
                               const char               *relative_path,
                               const git_diff_delta     *delta,
                               const char               *merged_content,
                               StageOperation            operation,
                               FoundryGitCommitBuilder  *self,
                               GError                  **error)
{
  git_index_entry entry;
  const git_error *git_err;

  g_assert (repository != NULL);
  g_assert (index != NULL);
  g_assert (relative_path != NULL);
  g_assert (delta != NULL);
  g_assert (self != NULL);

  if (merged_content == NULL)
    {
      /* If no old content, remove from index */
      if (git_index_remove_bypath (index, relative_path) != 0)
        {
          git_err = git_error_last ();
          g_set_error (error,
                       FOUNDRY_GIT_ERROR,
                       git_err->klass,
                       "%s", git_err->message);
          return FALSE;
        }

      if (git_index_write (index) != 0)
        {
          git_err = git_error_last ();
          g_set_error (error,
                       FOUNDRY_GIT_ERROR,
                       git_err->klass,
                       "%s", git_err->message);
          return FALSE;
        }

      foundry_git_commit_builder_refresh_diffs (self, repository, index, tree);
      return TRUE;
    }

  /* Stage the merged content */
  entry = (git_index_entry) {
    .mode = (operation == STAGE_OPERATION_STAGE) ?
            (delta->new_file.mode != 0 ? delta->new_file.mode : GIT_FILEMODE_BLOB) :
            (delta->old_file.mode != 0 ? delta->old_file.mode : GIT_FILEMODE_BLOB),
    .path = relative_path,
  };

  if (git_index_add_frombuffer (index, &entry, merged_content, strlen (merged_content)) != 0)
    {
      git_err = git_error_last ();
      g_set_error (error,
                   FOUNDRY_GIT_ERROR,
                   git_err->klass,
                   "%s", git_err->message);
      return FALSE;
    }

  if (git_index_write (index) != 0)
    {
      git_err = git_error_last ();
      g_set_error (error,
                   FOUNDRY_GIT_ERROR,
                   git_err->klass,
                   "%s", git_err->message);
      return FALSE;
    }

  foundry_git_commit_builder_refresh_diffs (self, repository, index, tree);

  return TRUE;
}

static DexFuture *
foundry_git_commit_builder_stage_hunks_thread (gpointer user_data)
{
  StageHunks *state = user_data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_index) index = NULL;
  g_autoptr(git_tree) tree = NULL;
  g_autoptr(git_blob) old_blob = NULL;
  g_autofree char *relative_path = NULL;
  g_autofree char *merged_content = NULL;
  g_autoptr(GBytes) workdir_contents_for_untracked = NULL;
  g_autoptr(FoundryGitPatch) git_patch = NULL;
  const git_diff_delta *delta = NULL;
  const char *old_buf = NULL;
  gsize old_buf_len = 0;
  gsize delta_idx = G_MAXSIZE;
  gboolean target_ends_with_newline = FALSE;

  g_assert (FOUNDRY_IS_GIT_COMMIT_BUILDER (state->self));
  g_assert (G_IS_FILE (state->file));

  if (!(relative_path = foundry_git_repository_paths_get_workdir_relative_path (state->self->paths, state->file)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "File is not in working tree");

  if (state->unstaged_diff == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_INITIALIZED,
                                  "Commit builder not initialized");

  {
    g_autoptr(GError) error = NULL;

    if (!setup_repository_context (state->self, state->paths, &repository, &index, &tree, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));

    if (!find_delta_for_file (state->unstaged_diff, relative_path, &delta, &delta_idx, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));

    get_old_content_for_stage (repository, index, tree, relative_path,
                               &old_buf, &old_buf_len, &old_blob,
                               &workdir_contents_for_untracked);

    if (!refresh_diff_and_refind_delta (state->self, &state->unstaged_diff,
                                        STAGE_OPERATION_STAGE, repository, index, tree,
                                        relative_path, &delta, &delta_idx, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));

    if (!create_patch_and_determine_newline (state->unstaged_diff, delta_idx,
                                             state->self, relative_path,
                                             old_buf, old_buf_len, STAGE_OPERATION_STAGE,
                                             &target_ends_with_newline, &git_patch,
                                             &old_buf, &old_buf_len,
                                             &workdir_contents_for_untracked, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));
  }

  merged_content = apply_selected_hunks_to_content (old_buf, old_buf_len, git_patch,
                                                     state->hunks, FALSE, target_ends_with_newline);

  if (merged_content == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Failed to apply hunks");

  {
    g_autoptr(GError) error = NULL;

    if (!write_merged_content_to_index (repository, index, tree, relative_path,
                                        delta, merged_content, STAGE_OPERATION_STAGE,
                                        state->self, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));
  }

  return dex_future_new_true ();
}

/**
 * foundry_git_commit_builder_stage_hunks:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @file: a [iface@Gio.File] in the working tree
 * @hunks: a [iface@Gio.ListModel] of [class@Foundry.GitDiffHunk]
 *
 * Stages the selected hunks from the file.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any value
 *   or rejects with error
 *
 * Since: 1.1
 */
DexFuture *
foundry_git_commit_builder_stage_hunks (FoundryGitCommitBuilder *self,
                                        GFile                   *file,
                                        GListModel              *hunks)
{
  StageHunks *state;
  UpdateStoresData *update_data;
  DexFuture *future;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));
  dex_return_error_if_fail (G_IS_FILE (file));
  dex_return_error_if_fail (G_IS_LIST_MODEL (hunks));

  state = g_new0 (StageHunks, 1);
  state->self = g_object_ref (self);
  state->file = g_object_ref (file);
  state->paths = foundry_git_repository_paths_ref (self->paths);
  state->hunks = g_object_ref (hunks);

  g_mutex_lock (&self->mutex);
  if (self->unstaged_diff != NULL)
    state->unstaged_diff = g_object_ref (self->unstaged_diff);
  g_mutex_unlock (&self->mutex);

  future = dex_thread_spawn ("[git-commit-builder-stage-hunks]",
                             foundry_git_commit_builder_stage_hunks_thread,
                             state,
                             (GDestroyNotify) stage_hunks_free);

  /* Chain callback to update list stores on main thread */
  update_data = g_new0 (UpdateStoresData, 1);
  update_data->self = g_object_ref (self);
  update_data->file = g_object_ref (file);

  return dex_future_then (future,
                          update_list_stores_after_stage,
                          update_data,
                          (GDestroyNotify) update_stores_data_free);
}

typedef struct _StageLines
{
  FoundryGitCommitBuilder *self;
  GFile *file;
  FoundryGitRepositoryPaths *paths;
  FoundryGitDiff *unstaged_diff;
  GListModel *lines;
} StageLines;

static void
stage_lines_free (StageLines *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->file);
  g_clear_pointer (&state->paths, foundry_git_repository_paths_unref);
  g_clear_object (&state->unstaged_diff);
  g_clear_object (&state->lines);
  g_free (state);
}

static DexFuture *
foundry_git_commit_builder_stage_lines_thread (gpointer user_data)
{
  StageLines *state = user_data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_index) index = NULL;
  g_autoptr(git_tree) tree = NULL;
  g_autoptr(git_blob) old_blob = NULL;
  g_autofree char *relative_path = NULL;
  g_autofree char *merged_content = NULL;
  g_autoptr(GBytes) workdir_contents_for_untracked = NULL;
  g_autoptr(FoundryGitPatch) git_patch = NULL;
  const git_diff_delta *delta = NULL;
  const char *old_buf = NULL;
  gsize old_buf_len = 0;
  gsize delta_idx = G_MAXSIZE;
  gboolean target_ends_with_newline = FALSE;

  g_assert (FOUNDRY_IS_GIT_COMMIT_BUILDER (state->self));
  g_assert (G_IS_FILE (state->file));

  if (!(relative_path = foundry_git_repository_paths_get_workdir_relative_path (state->self->paths, state->file)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "File is not in working tree");

  if (state->unstaged_diff == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_INITIALIZED,
                                  "Commit builder not initialized");

  {
    g_autoptr(GError) error = NULL;

    if (!setup_repository_context (state->self, state->paths, &repository, &index, &tree, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));

    if (!find_delta_for_file (state->unstaged_diff, relative_path, &delta, &delta_idx, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));

    get_old_content_for_stage (repository, index, tree, relative_path,
                               &old_buf, &old_buf_len, &old_blob,
                               &workdir_contents_for_untracked);

    if (!refresh_diff_and_refind_delta (state->self, &state->unstaged_diff,
                                        STAGE_OPERATION_STAGE, repository, index, tree,
                                        relative_path, &delta, &delta_idx, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));

    if (!create_patch_and_determine_newline (state->unstaged_diff, delta_idx,
                                             state->self, relative_path,
                                             old_buf, old_buf_len, STAGE_OPERATION_STAGE,
                                             &target_ends_with_newline, &git_patch,
                                             &old_buf, &old_buf_len,
                                             &workdir_contents_for_untracked, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));
  }

  merged_content = apply_selected_lines_to_content (old_buf, old_buf_len, git_patch,
                                                    state->lines, FALSE, target_ends_with_newline);

  if (merged_content == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Failed to apply lines");

  {
    g_autoptr(GError) error = NULL;

    if (!write_merged_content_to_index (repository, index, tree, relative_path,
                                        delta, merged_content, STAGE_OPERATION_STAGE,
                                        state->self, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));
  }

  return dex_future_new_true ();
}

/**
 * foundry_git_commit_builder_stage_lines:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @file: a [iface@Gio.File] in the working tree
 * @lines: a [iface@Gio.ListModel] of [class@Foundry.GitDiffLine]
 *
 * Stages the selected lines from the file.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any value
 *   or rejects with error
 *
 * Since: 1.1
 */
DexFuture *
foundry_git_commit_builder_stage_lines (FoundryGitCommitBuilder *self,
                                        GFile                   *file,
                                        GListModel              *lines)
{
  StageLines *state;
  UpdateStoresData *update_data;
  DexFuture *future;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));
  dex_return_error_if_fail (G_IS_FILE (file));
  dex_return_error_if_fail (G_IS_LIST_MODEL (lines));

  state = g_new0 (StageLines, 1);
  state->self = g_object_ref (self);
  state->file = g_object_ref (file);
  state->paths = foundry_git_repository_paths_ref (self->paths);
  state->lines = g_object_ref (lines);

  g_mutex_lock (&self->mutex);
  if (self->unstaged_diff != NULL)
    state->unstaged_diff = g_object_ref (self->unstaged_diff);
  g_mutex_unlock (&self->mutex);

  future = dex_thread_spawn ("[git-commit-builder-stage-lines]",
                             foundry_git_commit_builder_stage_lines_thread,
                             state,
                             (GDestroyNotify) stage_lines_free);

  /* Chain callback to update list stores on main thread */
  update_data = g_new0 (UpdateStoresData, 1);
  update_data->self = g_object_ref (self);
  update_data->file = g_object_ref (file);

  return dex_future_then (future,
                          update_list_stores_after_stage,
                          update_data,
                          (GDestroyNotify) update_stores_data_free);
}

typedef struct _UnstageHunks
{
  FoundryGitCommitBuilder *self;
  GFile *file;
  FoundryGitRepositoryPaths *paths;
  FoundryGitDiff *staged_diff;
  GListModel *hunks;
} UnstageHunks;

static void
unstage_hunks_free (UnstageHunks *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->file);
  g_clear_pointer (&state->paths, foundry_git_repository_paths_unref);
  g_clear_object (&state->staged_diff);
  g_clear_object (&state->hunks);
  g_free (state);
}

static DexFuture *
foundry_git_commit_builder_unstage_hunks_thread (gpointer user_data)
{
  UnstageHunks *state = user_data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_index) index = NULL;
  g_autoptr(git_tree) tree = NULL;
  g_autoptr(git_blob) old_blob = NULL;
  g_autofree char *relative_path = NULL;
  g_autofree char *merged_content = NULL;
  g_autoptr(FoundryGitPatch) git_patch = NULL;
  const git_diff_delta *delta = NULL;
  const char *old_buf = NULL;
  gsize old_buf_len = 0;
  gsize delta_idx = G_MAXSIZE;
  gboolean target_ends_with_newline = FALSE;

  g_assert (FOUNDRY_IS_GIT_COMMIT_BUILDER (state->self));
  g_assert (G_IS_FILE (state->file));

  if (!(relative_path = foundry_git_repository_paths_get_workdir_relative_path (state->self->paths, state->file)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "File is not in working tree");

  if (state->staged_diff == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_INITIALIZED,
                                  "Commit builder not initialized");

  {
    g_autoptr(GError) error = NULL;

    if (!setup_repository_context (state->self, state->paths, &repository, &index, &tree, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));

    if (!find_delta_for_file (state->staged_diff, relative_path, &delta, &delta_idx, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));

    get_old_content_for_unstage (repository, tree, relative_path,
                                 &old_buf, &old_buf_len, &old_blob);

    if (!refresh_diff_and_refind_delta (state->self, &state->staged_diff,
                                        STAGE_OPERATION_UNSTAGE, repository, index, tree,
                                        relative_path, &delta, &delta_idx, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));

    if (!create_patch_and_determine_newline (state->staged_diff, delta_idx,
                                             state->self, relative_path,
                                             old_buf, old_buf_len, STAGE_OPERATION_UNSTAGE,
                                             &target_ends_with_newline, &git_patch,
                                             &old_buf, &old_buf_len,
                                             NULL, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));
  }

  merged_content = apply_selected_hunks_to_content (old_buf, old_buf_len, git_patch,
                                                    state->hunks, TRUE, target_ends_with_newline);

  {
    g_autoptr(GError) error = NULL;

    if (!write_merged_content_to_index (repository, index, tree, relative_path,
                                        delta, merged_content, STAGE_OPERATION_UNSTAGE,
                                        state->self, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));
  }

  return dex_future_new_true ();
}

/**
 * foundry_git_commit_builder_unstage_hunks:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @file: a [iface@Gio.File] in the working tree
 * @hunks: a [iface@Gio.ListModel] of [class@Foundry.GitDiffHunk]
 *
 * Unstages the selected hunks from the file.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any value
 *   or rejects with error
 *
 * Since: 1.1
 */
DexFuture *
foundry_git_commit_builder_unstage_hunks (FoundryGitCommitBuilder *self,
                                          GFile                   *file,
                                          GListModel              *hunks)
{
  UnstageHunks *state;
  UpdateStoresData *update_data;
  DexFuture *future;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));
  dex_return_error_if_fail (G_IS_FILE (file));
  dex_return_error_if_fail (G_IS_LIST_MODEL (hunks));

  state = g_new0 (UnstageHunks, 1);
  state->self = g_object_ref (self);
  state->file = g_object_ref (file);
  state->paths = foundry_git_repository_paths_ref (self->paths);
  state->hunks = g_object_ref (hunks);

  g_mutex_lock (&self->mutex);
  if (self->staged_diff != NULL)
    state->staged_diff = g_object_ref (self->staged_diff);
  g_mutex_unlock (&self->mutex);

  future = dex_thread_spawn ("[git-commit-builder-unstage-hunks]",
                             foundry_git_commit_builder_unstage_hunks_thread,
                             state,
                             (GDestroyNotify) unstage_hunks_free);

  /* Chain callback to update list stores on main thread */
  update_data = g_new0 (UpdateStoresData, 1);
  update_data->self = g_object_ref (self);
  update_data->file = g_object_ref (file);

  return dex_future_then (future,
                          update_list_stores_after_unstage,
                          update_data,
                          (GDestroyNotify) update_stores_data_free);
}

typedef struct _UnstageLines
{
  FoundryGitCommitBuilder *self;
  GFile *file;
  FoundryGitRepositoryPaths *paths;
  FoundryGitDiff *staged_diff;
  GListModel *lines;
} UnstageLines;

static void
unstage_lines_free (UnstageLines *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->file);
  g_clear_pointer (&state->paths, foundry_git_repository_paths_unref);
  g_clear_object (&state->staged_diff);
  g_clear_object (&state->lines);
  g_free (state);
}

static DexFuture *
foundry_git_commit_builder_unstage_lines_thread (gpointer user_data)
{
  UnstageLines *state = user_data;
  g_autoptr(git_repository) repository = NULL;
  g_autoptr(git_index) index = NULL;
  g_autoptr(git_tree) tree = NULL;
  g_autoptr(git_blob) old_blob = NULL;
  g_autofree char *relative_path = NULL;
  g_autofree char *merged_content = NULL;
  g_autoptr(FoundryGitPatch) git_patch = NULL;
  const git_diff_delta *delta = NULL;
  const char *old_buf = NULL;
  gsize old_buf_len = 0;
  gsize delta_idx = G_MAXSIZE;
  gboolean target_ends_with_newline = FALSE;

  g_assert (FOUNDRY_IS_GIT_COMMIT_BUILDER (state->self));
  g_assert (G_IS_FILE (state->file));

  if (!(relative_path = foundry_git_repository_paths_get_workdir_relative_path (state->self->paths, state->file)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "File is not in working tree");

  if (state->staged_diff == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_INITIALIZED,
                                  "Commit builder not initialized");

  {
    g_autoptr(GError) error = NULL;

    if (!setup_repository_context (state->self, state->paths, &repository, &index, &tree, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));

    if (!find_delta_for_file (state->staged_diff, relative_path, &delta, &delta_idx, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));

    get_old_content_for_unstage (repository, tree, relative_path,
                                 &old_buf, &old_buf_len, &old_blob);

    if (!refresh_diff_and_refind_delta (state->self, &state->staged_diff,
                                        STAGE_OPERATION_UNSTAGE, repository, index, tree,
                                        relative_path, &delta, &delta_idx, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));

    if (!create_patch_and_determine_newline (state->staged_diff, delta_idx,
                                             state->self, relative_path,
                                             old_buf, old_buf_len, STAGE_OPERATION_UNSTAGE,
                                             &target_ends_with_newline, &git_patch,
                                             &old_buf, &old_buf_len,
                                             NULL, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));
  }

  merged_content = apply_selected_lines_to_content (old_buf, old_buf_len, git_patch,
                                                    state->lines, TRUE, target_ends_with_newline);

  {
    g_autoptr(GError) error = NULL;

    if (!write_merged_content_to_index (repository, index, tree, relative_path,
                                        delta, merged_content, STAGE_OPERATION_UNSTAGE,
                                        state->self, &error))
      return dex_future_new_for_error (g_steal_pointer (&error));
  }

  return dex_future_new_true ();
}

/**
 * foundry_git_commit_builder_unstage_lines:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @file: a [iface@Gio.File] in the working tree
 * @lines: a [iface@Gio.ListModel] of [class@Foundry.GitDiffLine]
 *
 * Unstages the selected lines from the file.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any value
 *   or rejects with error
 *
 * Since: 1.1
 */
DexFuture *
foundry_git_commit_builder_unstage_lines (FoundryGitCommitBuilder *self,
                                          GFile                   *file,
                                          GListModel              *lines)
{
  UnstageLines *state;
  UpdateStoresData *update_data;
  DexFuture *future;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self));
  dex_return_error_if_fail (G_IS_FILE (file));
  dex_return_error_if_fail (G_IS_LIST_MODEL (lines));

  state = g_new0 (UnstageLines, 1);
  state->self = g_object_ref (self);
  state->file = g_object_ref (file);
  state->paths = foundry_git_repository_paths_ref (self->paths);
  state->lines = g_object_ref (lines);

  g_mutex_lock (&self->mutex);
  if (self->staged_diff != NULL)
    state->staged_diff = g_object_ref (self->staged_diff);
  g_mutex_unlock (&self->mutex);

  future = dex_thread_spawn ("[git-commit-builder-unstage-lines]",
                             foundry_git_commit_builder_unstage_lines_thread,
                             state,
                             (GDestroyNotify) unstage_lines_free);

  /* Chain callback to update list stores on main thread */
  update_data = g_new0 (UpdateStoresData, 1);
  update_data->self = g_object_ref (self);
  update_data->file = g_object_ref (file);

  return dex_future_then (future,
                          update_list_stores_after_unstage,
                          update_data,
                          (GDestroyNotify) update_stores_data_free);
}
