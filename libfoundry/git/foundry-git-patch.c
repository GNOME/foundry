/* foundry-git-patch.c
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

#include "foundry-git-patch-private.h"

struct _FoundryGitPatch
{
  gatomicrefcount ref_count;
  git_patch *patch;
};

FoundryGitPatch *
_foundry_git_patch_new (git_patch *patch)
{
  FoundryGitPatch *self;

  g_return_val_if_fail (patch != NULL, NULL);

  self = g_new0 (FoundryGitPatch, 1);
  g_atomic_ref_count_init (&self->ref_count);
  self->patch = patch;

  return self;
}

FoundryGitPatch *
_foundry_git_patch_ref (FoundryGitPatch *patch)
{
  g_return_val_if_fail (patch != NULL, NULL);

  g_atomic_ref_count_inc (&patch->ref_count);

  return patch;
}

void
_foundry_git_patch_unref (FoundryGitPatch *patch)
{
  if (patch == NULL)
    return;

  if (g_atomic_ref_count_dec (&patch->ref_count))
    {
      g_clear_pointer (&patch->patch, git_patch_free);
      g_free (patch);
    }
}

gsize
_foundry_git_patch_get_num_hunks (FoundryGitPatch *patch)
{
  g_return_val_if_fail (patch != NULL, 0);
  g_return_val_if_fail (patch->patch != NULL, 0);

  return git_patch_num_hunks (patch->patch);
}

const git_diff_hunk *
_foundry_git_patch_get_hunk (FoundryGitPatch *patch,
                             gsize            hunk_idx)
{
  const git_diff_hunk *hunk;
  size_t lines_in_hunk;

  g_return_val_if_fail (patch != NULL, NULL);
  g_return_val_if_fail (patch->patch != NULL, NULL);

  if (git_patch_get_hunk (&hunk, &lines_in_hunk, patch->patch, hunk_idx) != 0)
    return NULL;

  return hunk;
}

gsize
_foundry_git_patch_get_num_lines_in_hunk (FoundryGitPatch *patch,
                                          gsize            hunk_idx)
{
  const git_diff_hunk *hunk;
  size_t lines_in_hunk;

  g_return_val_if_fail (patch != NULL, 0);
  g_return_val_if_fail (patch->patch != NULL, 0);

  if (git_patch_get_hunk (&hunk, &lines_in_hunk, patch->patch, hunk_idx) != 0)
    return 0;

  return lines_in_hunk;
}

const git_diff_line *
_foundry_git_patch_get_line (FoundryGitPatch *patch,
                             gsize            hunk_idx,
                             gsize            line_idx)
{
  const git_diff_line *line;

  g_return_val_if_fail (patch != NULL, NULL);
  g_return_val_if_fail (patch->patch != NULL, NULL);

  if (git_patch_get_line_in_hunk (&line, patch->patch, hunk_idx, line_idx) != 0)
    return NULL;

  return line;
}
