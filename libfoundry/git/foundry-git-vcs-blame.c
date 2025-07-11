/* foundry-git-vcs-blame.c
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

#include "foundry-git-autocleanups.h"
#include "foundry-git-vcs-blame-private.h"
#include "foundry-git-vcs-signature-private.h"
#include "foundry-vcs-file.h"

struct _FoundryGitVcsBlame
{
  FoundryVcsBlame parent_instance;
  git_blame *base_blame;
  git_blame *bytes_blame;
};

G_DEFINE_FINAL_TYPE (FoundryGitVcsBlame, foundry_git_vcs_blame, FOUNDRY_TYPE_VCS_BLAME)

static git_blame *
get_blame (FoundryGitVcsBlame *self)
{
  return self->bytes_blame ? self->bytes_blame : self->base_blame;
}

static DexFuture *
foundry_git_vcs_blame_update (FoundryVcsBlame *vcs_blame,
                              GBytes          *contents)
{
  FoundryGitVcsBlame *self = (FoundryGitVcsBlame *)vcs_blame;
  g_autoptr(git_blame) blame = NULL;
  gconstpointer data = NULL;
  gsize size;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_VCS_BLAME (self));
  dex_return_error_if_fail (contents != NULL);

  if (contents != NULL)
    data = g_bytes_get_data (contents, &size);

  g_clear_pointer (&self->bytes_blame, git_blame_free);

  if (git_blame_buffer (&blame, self->base_blame, data, size) == 0)
    self->bytes_blame = g_steal_pointer (&blame);

  return dex_future_new_true ();
}

static FoundryVcsSignature *
foundry_git_vcs_blame_query_line (FoundryVcsBlame *blame,
                                  guint            line)
{
  FoundryGitVcsBlame *self = (FoundryGitVcsBlame *)blame;
  const git_blame_hunk *hunk;
  git_blame *gblame;

  g_assert (FOUNDRY_IS_GIT_VCS_BLAME (self));
  g_assert (self->base_blame != NULL);

  gblame = get_blame (self);

  if ((hunk = git_blame_get_hunk_byline (gblame, line + 1)))
    return foundry_git_vcs_signature_new (&hunk->final_commit_id, hunk->final_signature);

  return NULL;
}

static guint
foundry_git_vcs_blame_get_n_lines (FoundryVcsBlame *blame)
{
  FoundryGitVcsBlame *self = (FoundryGitVcsBlame *)blame;
  git_blame *gblame;
  gsize hunk_count;
  guint n_lines = 0;

  g_assert (FOUNDRY_IS_GIT_VCS_BLAME (self));
  g_assert (self->base_blame != NULL);

  gblame = get_blame (self);

  hunk_count = git_blame_get_hunk_count (gblame);

  for (gsize i = 0; i < hunk_count; i++)
    {
      const git_blame_hunk *hunk = git_blame_get_hunk_byindex (gblame, i);

      if (hunk != NULL)
        n_lines += hunk->lines_in_hunk;
    }

  return n_lines;
}

static void
foundry_git_vcs_blame_finalize (GObject *object)
{
  FoundryGitVcsBlame *self = (FoundryGitVcsBlame *)object;

  g_clear_pointer (&self->bytes_blame, git_blame_free);
  g_clear_pointer (&self->base_blame, git_blame_free);

  G_OBJECT_CLASS (foundry_git_vcs_blame_parent_class)->finalize (object);
}

static void
foundry_git_vcs_blame_class_init (FoundryGitVcsBlameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsBlameClass *vcs_blame_class = FOUNDRY_VCS_BLAME_CLASS (klass);

  object_class->finalize = foundry_git_vcs_blame_finalize;

  vcs_blame_class->update = foundry_git_vcs_blame_update;
  vcs_blame_class->query_line = foundry_git_vcs_blame_query_line;
  vcs_blame_class->get_n_lines = foundry_git_vcs_blame_get_n_lines;
}

static void
foundry_git_vcs_blame_init (FoundryGitVcsBlame *self)
{
}

FoundryGitVcsBlame *
foundry_git_vcs_blame_new (git_blame *base_blame,
                           git_blame *bytes_blame)
{
  FoundryGitVcsBlame *self;

  g_return_val_if_fail (base_blame != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_VCS_BLAME, NULL);
  self->base_blame = g_steal_pointer (&base_blame);
  self->bytes_blame = g_steal_pointer (&bytes_blame);

  return self;
}
