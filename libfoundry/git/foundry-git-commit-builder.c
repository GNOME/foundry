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
#include <git2.h>

#include "foundry-git-autocleanups.h"
#include "foundry-git-commit-builder.h"
#include "foundry-git-commit-private.h"
#include "foundry-git-delta-private.h"
#include "foundry-git-diff-private.h"
#include "foundry-git-error.h"
#include "foundry-git-vcs-private.h"
#include "foundry-util.h"

#define MAX_UNTRACKED_FILES 25000

struct _FoundryGitCommitBuilder
{
  GObject           parent_instance;

  FoundryGitCommit *parent;

  char             *author_name;
  char             *author_email;
  char             *signing_key;
  char             *git_dir;
  char             *message;
  GDateTime        *when;

  GListStore       *staged;
  GListStore       *unstaged;
  GListStore       *untracked;

  GFile            *workdir;

  GMutex            mutex;
  FoundryGitDiff   *staged_diff;
  FoundryGitDiff   *unstaged_diff;

  guint             context_lines;
};

enum {
  PROP_0,
  PROP_AUTHOR_EMAIL,
  PROP_AUTHOR_NAME,
  PROP_SIGNING_KEY,
  PROP_WHEN,
  PROP_MESSAGE,
  PROP_CAN_COMMIT,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryGitCommitBuilder, foundry_git_commit_builder, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_git_commit_builder_finalize (GObject *object)
{
  FoundryGitCommitBuilder *self = (FoundryGitCommitBuilder *)object;

  g_clear_object (&self->parent);

  g_clear_object (&self->staged);
  g_clear_object (&self->unstaged);
  g_clear_object (&self->untracked);

  g_clear_pointer (&self->git_dir, g_free);
  g_clear_pointer (&self->author_name, g_free);
  g_clear_pointer (&self->author_email, g_free);
  g_clear_pointer (&self->signing_key, g_free);
  g_clear_pointer (&self->message, g_free);

  g_clear_pointer (&self->when, g_date_time_unref);
  g_clear_object (&self->staged_diff);
  g_clear_object (&self->unstaged_diff);
  g_clear_object (&self->workdir);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (foundry_git_commit_builder_parent_class)->finalize (object);
}

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

    case PROP_MESSAGE:
      g_value_take_string (value, foundry_git_commit_builder_dup_message (self));
      break;

    case PROP_CAN_COMMIT:
      g_value_set_boolean (value, foundry_git_commit_builder_get_can_commit (self));
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

  properties[PROP_AUTHOR_EMAIL] =
    g_param_spec_string ("author-email", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_AUTHOR_NAME] =
    g_param_spec_string ("author-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_WHEN] =
    g_param_spec_boxed ("when", NULL, NULL,
                        G_TYPE_DATE_TIME,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_SIGNING_KEY] =
    g_param_spec_string ("signing-key", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_MESSAGE] =
    g_param_spec_string ("message", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_CAN_COMMIT] =
    g_param_spec_boolean ("can-commit", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_git_commit_builder_init (FoundryGitCommitBuilder *self)
{
  self->unstaged = g_list_store_new (G_TYPE_FILE);
  self->untracked = g_list_store_new (G_TYPE_FILE);
  self->staged = g_list_store_new (G_TYPE_FILE);
  self->context_lines = 3;
  g_mutex_init (&self->mutex);
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
  g_assert (G_IS_FILE (self->workdir));

  if (git_repository_open (&repository, self->git_dir) != 0)
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
  new_staged_diff = _foundry_git_diff_new_with_dir (g_steal_pointer (&staged_diff), self->git_dir);
  new_unstaged_diff = _foundry_git_diff_new_with_dir (g_steal_pointer (&unstaged_diff), self->git_dir);

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
      g_autoptr(GFile) file = NULL;
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

      file = g_file_resolve_relative_path (self->workdir, path);

      /* Check for staged changes */
      if (status & (GIT_STATUS_INDEX_NEW |
                    GIT_STATUS_INDEX_MODIFIED |
                    GIT_STATUS_INDEX_DELETED |
                    GIT_STATUS_INDEX_RENAMED |
                    GIT_STATUS_INDEX_TYPECHANGE))
        g_list_store_append (self->staged, g_object_ref (file));

      /* Check for unstaged changes (but not untracked) */
      if (status & (GIT_STATUS_WT_MODIFIED |
                    GIT_STATUS_WT_DELETED |
                    GIT_STATUS_WT_RENAMED |
                    GIT_STATUS_WT_TYPECHANGE))
        g_list_store_append (self->unstaged, g_object_ref (file));

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
                  g_list_store_append (self->untracked, g_object_ref (file));
                  untracked_count++;
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
  self->author_name = dex_await_string (foundry_git_vcs_query_config (vcs, "user.name"), NULL);
  self->author_email = dex_await_string (foundry_git_vcs_query_config (vcs, "user.email"), NULL);
  self->signing_key = dex_await_string (foundry_git_vcs_query_config (vcs, "user.signingKey"), NULL);
  self->git_dir = _foundry_git_vcs_dup_git_dir (vcs);
  self->workdir = _foundry_git_vcs_dup_workdir (vcs);
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

/**
 * foundry_git_commit_builder_dup_author_name:
 * @self: a [class@Foundry.GitCommitBuilder]
 *
 * Returns: (transfer full) (nullable):
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
 * Returns: (transfer full) (nullable):
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
 * Returns: (transfer full) (nullable):
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
 * @author_name: (nullable):
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
 * @author_email: (nullable):
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
 * @signing_key: (nullable):
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
 * foundry_git_commit_builder_set_when:
 * @self: a [class@Foundry.GitCommitBuilder]
 * @when: (nullable):
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
 * Returns: (transfer full) (nullable):
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
 * @message: (nullable):
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
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
GDateTime *
foundry_git_commit_builder_dup_when (FoundryGitCommitBuilder *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self), NULL);

  return self->when ? g_date_time_ref (self->when) : NULL;
}

typedef struct _BuilderCommit
{
  char *git_dir;
  char *message;
  char *author_name;
  char *author_email;
  GDateTime *when;
  git_oid parent_id;
  guint has_parent : 1;
} BuilderCommit;

static void
builder_commit_free (BuilderCommit *state)
{
  g_clear_pointer (&state->git_dir, g_free);
  g_clear_pointer (&state->message, g_free);
  g_clear_pointer (&state->author_name, g_free);
  g_clear_pointer (&state->author_email, g_free);
  g_clear_pointer (&state->when, g_date_time_unref);
  g_free (state);
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
  g_assert (state->git_dir != NULL);
  g_assert (state->message != NULL);

  if (git_repository_open (&repository, state->git_dir) != 0)
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

      if (git_commit_create_v (&commit_oid, repository, "HEAD", author, committer, NULL, state->message, tree, 1, parent) != 0)
        return foundry_git_reject_last_error ();
    }
  else
    {
      g_autoptr(git_object) parent_obj = NULL;

      if ((err = git_revparse_single (&parent_obj, repository, "HEAD^{commit}")) != 0)
        {
          if (err != GIT_ENOTFOUND)
            return foundry_git_reject_last_error ();

          if (git_commit_create_v (&commit_oid, repository, "HEAD", author, committer, NULL, state->message, tree, 0) != 0)
            return foundry_git_reject_last_error ();
        }
      else
        {
          if (git_object_peel ((git_object **)&parent, parent_obj, GIT_OBJECT_COMMIT) != 0)
            return foundry_git_reject_last_error ();

          if (git_commit_create_v (&commit_oid, repository, "HEAD", author, committer, NULL, state->message, tree, 1, parent) != 0)
            return foundry_git_reject_last_error ();
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

  state = g_new0 (BuilderCommit, 1);
  state->git_dir = g_strdup (self->git_dir);
  state->message = g_strdup (self->message);
  state->author_name = g_strdup (self->author_name);
  state->author_email = g_strdup (self->author_email);
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
    new_staged_diff = _foundry_git_diff_new_with_dir (g_steal_pointer (&staged_diff), self->git_dir);

  /* Refresh unstaged diff (index to workdir) */
  ret = git_diff_index_to_workdir (&unstaged_diff, repository, index, &diff_opts);
  if (ret == 0)
    new_unstaged_diff = _foundry_git_diff_new_with_dir (g_steal_pointer (&unstaged_diff), self->git_dir);

  /* Lock mutex and update diffs atomically */
  g_mutex_lock (&self->mutex);
  g_set_object (&self->staged_diff, new_staged_diff);
  g_set_object (&self->unstaged_diff, new_unstaged_diff);
  g_mutex_unlock (&self->mutex);
}

static void
update_list_store_remove (GListStore *store,
                          GFile      *file)
{
  guint n_items;

  g_return_if_fail (G_IS_LIST_STORE (store));
  g_return_if_fail (G_IS_FILE (file));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (store));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GFile) item = g_list_model_get_item (G_LIST_MODEL (store), i);

      if (g_file_equal (item, file))
        {
          g_list_store_remove (store, i);
          break;
        }
    }
}

static gboolean
update_list_store_contains (GListStore *store,
                           GFile      *file)
{
  guint n_items;

  g_return_val_if_fail (G_IS_LIST_STORE (store), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (store));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GFile) item = g_list_model_get_item (G_LIST_MODEL (store), i);

      if (g_file_equal (item, file))
        return TRUE;
    }

  return FALSE;
}

static void
update_list_store_add (GListStore *store,
                      GFile      *file)
{
  g_return_if_fail (G_IS_LIST_STORE (store));
  g_return_if_fail (G_IS_FILE (file));

  if (!update_list_store_contains (store, file))
    g_list_store_append (store, g_object_ref (file));
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

  if (!(relative_path = g_file_get_relative_path (data->self->workdir, data->file)))
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
    update_list_store_remove (data->self->unstaged, data->file);

  /* Add to staged if it's in staged diff */
  if (in_staged)
    update_list_store_add (data->self->staged, data->file);
  else
    update_list_store_remove (data->self->staged, data->file);

  /* For untracked: remove if it's now staged, otherwise keep it */
  if (in_staged)
    update_list_store_remove (data->self->untracked, data->file);

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

  if (!(relative_path = g_file_get_relative_path (data->self->workdir, data->file)))
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
    update_list_store_remove (data->self->staged, data->file);

  /* Add to unstaged if it's in unstaged diff */
  if (in_unstaged)
    update_list_store_add (data->self->unstaged, data->file);
  else
    update_list_store_remove (data->self->unstaged, data->file);

  /* For untracked: if it's not in staged, it might be untracked */
  if (!in_staged)
    {
      /* If it's not in unstaged either, it's untracked */
      if (!in_unstaged)
        update_list_store_add (data->self->untracked, data->file);
      else
        update_list_store_remove (data->self->untracked, data->file);
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
  char *git_dir;
  FoundryGitDiff *unstaged_diff;
} StageFile;

static void
stage_file_free (StageFile *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->file);
  g_clear_pointer (&state->git_dir, g_free);
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

  relative_path = g_file_get_relative_path (state->self->workdir, state->file);
  if (relative_path == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "File is not in working tree");

  if (state->unstaged_diff == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_INITIALIZED,
                                  "Commit builder not initialized");

  if (git_repository_open (&repository, state->git_dir) != 0)
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
  state->git_dir = g_strdup (self->git_dir);

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
  char *git_dir;
  FoundryGitDiff *staged_diff;
} UnstageFile;

static void
unstage_file_free (UnstageFile *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->file);
  g_clear_pointer (&state->git_dir, g_free);
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

  relative_path = g_file_get_relative_path (state->self->workdir, state->file);
  if (relative_path == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "File is not in working tree");

  if (state->staged_diff == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_INITIALIZED,
                                  "Commit builder not initialized");

  if (git_repository_open (&repository, state->git_dir) != 0)
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
  state->git_dir = g_strdup (self->git_dir);

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
 * Returns: (transfer full): a [class@Gio.ListModel] of [iface@Gio.File]
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
 * Returns: (transfer full): a [class@Gio.ListModel] of [iface@Gio.File]
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
 * Returns: (transfer full): a [class@Gio.ListModel] of [iface@Gio.File]
 *
 * Since: 1.1
 */
GListModel *
foundry_git_commit_builder_list_untracked (FoundryGitCommitBuilder *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_COMMIT_BUILDER (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->untracked));
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

  relative_path = g_file_get_relative_path (state->self->workdir, state->file);
  if (relative_path == NULL)
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

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_FOUND,
                                "Delta not found for file");
}

static DexFuture *
foundry_git_commit_builder_load_unstaged_delta_thread (gpointer user_data)
{
  LoadDelta *state = user_data;
  g_autofree char *relative_path = NULL;
  gsize n_deltas;

  g_assert (FOUNDRY_IS_GIT_COMMIT_BUILDER (state->self));
  g_assert (G_IS_FILE (state->file));

  relative_path = g_file_get_relative_path (state->self->workdir, state->file);
  if (relative_path == NULL)
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
