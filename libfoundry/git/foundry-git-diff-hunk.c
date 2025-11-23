/* foundry-git-diff-hunk.c
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

#include "foundry-git-diff-hunk-private.h"
#include "foundry-git-diff-line-private.h"
#include "foundry-git-patch-private.h"
#include "foundry-util.h"

struct _FoundryGitDiffHunk
{
  FoundryVcsDiffHunk parent_instance;
  FoundryGitPatch *patch;
  gsize hunk_idx;
};

G_DEFINE_FINAL_TYPE (FoundryGitDiffHunk, foundry_git_diff_hunk, FOUNDRY_TYPE_VCS_DIFF_HUNK)

static void
foundry_git_diff_hunk_finalize (GObject *object)
{
  FoundryGitDiffHunk *self = (FoundryGitDiffHunk *)object;

  g_clear_pointer (&self->patch, _foundry_git_patch_unref);

  G_OBJECT_CLASS (foundry_git_diff_hunk_parent_class)->finalize (object);
}

static DexFuture *
foundry_git_diff_hunk_list_lines (FoundryVcsDiffHunk *hunk)
{
  FoundryGitDiffHunk *self = FOUNDRY_GIT_DIFF_HUNK (hunk);
  g_autoptr(GListStore) store = NULL;
  gsize num_lines;

  g_assert (FOUNDRY_IS_GIT_DIFF_HUNK (self));
  g_assert (self->patch != NULL);

  store = g_list_store_new (FOUNDRY_TYPE_GIT_DIFF_LINE);
  num_lines = _foundry_git_patch_get_num_lines_in_hunk (self->patch, self->hunk_idx);

  for (gsize i = 0; i < num_lines; i++)
    {
      g_autoptr(FoundryGitDiffLine) line = NULL;

      line = _foundry_git_diff_line_new (self->patch, self->hunk_idx, i);
      g_list_store_append (store, line);
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static char *
foundry_git_diff_hunk_dup_header (FoundryVcsDiffHunk *hunk)
{
  FoundryGitDiffHunk *self = FOUNDRY_GIT_DIFF_HUNK (hunk);
  const git_diff_hunk *ghunk;

  g_assert (FOUNDRY_IS_GIT_DIFF_HUNK (self));
  g_assert (self->patch != NULL);

  ghunk = _foundry_git_patch_get_hunk (self->patch, self->hunk_idx);

  if (ghunk == NULL)
    return NULL;

  return g_strdup (ghunk->header);
}

static guint
foundry_git_diff_hunk_get_old_start (FoundryVcsDiffHunk *hunk)
{
  FoundryGitDiffHunk *self = FOUNDRY_GIT_DIFF_HUNK (hunk);
  const git_diff_hunk *ghunk;

  g_assert (FOUNDRY_IS_GIT_DIFF_HUNK (self));
  g_assert (self->patch != NULL);

  ghunk = _foundry_git_patch_get_hunk (self->patch, self->hunk_idx);

  if (ghunk == NULL)
    return 0;

  return ghunk->old_start;
}

static guint
foundry_git_diff_hunk_get_old_lines (FoundryVcsDiffHunk *hunk)
{
  FoundryGitDiffHunk *self = FOUNDRY_GIT_DIFF_HUNK (hunk);
  const git_diff_hunk *ghunk;

  g_assert (FOUNDRY_IS_GIT_DIFF_HUNK (self));
  g_assert (self->patch != NULL);

  ghunk = _foundry_git_patch_get_hunk (self->patch, self->hunk_idx);

  if (ghunk == NULL)
    return 0;

  return ghunk->old_lines;
}

static guint
foundry_git_diff_hunk_get_new_start (FoundryVcsDiffHunk *hunk)
{
  FoundryGitDiffHunk *self = FOUNDRY_GIT_DIFF_HUNK (hunk);
  const git_diff_hunk *ghunk;

  g_assert (FOUNDRY_IS_GIT_DIFF_HUNK (self));
  g_assert (self->patch != NULL);

  ghunk = _foundry_git_patch_get_hunk (self->patch, self->hunk_idx);

  if (ghunk == NULL)
    return 0;

  return ghunk->new_start;
}

static guint
foundry_git_diff_hunk_get_new_lines (FoundryVcsDiffHunk *hunk)
{
  FoundryGitDiffHunk *self = FOUNDRY_GIT_DIFF_HUNK (hunk);
  const git_diff_hunk *ghunk;

  g_assert (FOUNDRY_IS_GIT_DIFF_HUNK (self));
  g_assert (self->patch != NULL);

  ghunk = _foundry_git_patch_get_hunk (self->patch, self->hunk_idx);

  if (ghunk == NULL)
    return 0;

  return ghunk->new_lines;
}

static void
foundry_git_diff_hunk_class_init (FoundryGitDiffHunkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsDiffHunkClass *hunk_class = FOUNDRY_VCS_DIFF_HUNK_CLASS (klass);

  object_class->finalize = foundry_git_diff_hunk_finalize;

  hunk_class->list_lines = foundry_git_diff_hunk_list_lines;
  hunk_class->dup_header = foundry_git_diff_hunk_dup_header;
  hunk_class->get_old_start = foundry_git_diff_hunk_get_old_start;
  hunk_class->get_old_lines = foundry_git_diff_hunk_get_old_lines;
  hunk_class->get_new_start = foundry_git_diff_hunk_get_new_start;
  hunk_class->get_new_lines = foundry_git_diff_hunk_get_new_lines;
}

static void
foundry_git_diff_hunk_init (FoundryGitDiffHunk *self)
{
}

FoundryGitDiffHunk *
_foundry_git_diff_hunk_new (FoundryGitPatch *patch,
                            gsize            hunk_idx)
{
  FoundryGitDiffHunk *self;

  g_return_val_if_fail (patch != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_DIFF_HUNK, NULL);
  self->patch = _foundry_git_patch_ref (patch);
  self->hunk_idx = hunk_idx;

  return self;
}
