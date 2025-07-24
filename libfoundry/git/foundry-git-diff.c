/* foundry-git-diff.c
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

#include "foundry-git-diff-private.h"

struct _FoundryGitDiff
{
  FoundryVcsDiff  parent_instance;
  GMutex          mutex;
  git_diff       *diff;
};

G_DEFINE_FINAL_TYPE (FoundryGitDiff, foundry_git_diff, FOUNDRY_TYPE_VCS_DIFF)

static void
foundry_git_diff_finalize (GObject *object)
{
  FoundryGitDiff *self = (FoundryGitDiff *)object;

  g_clear_pointer (&self->diff, git_diff_free);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (foundry_git_diff_parent_class)->finalize (object);
}

static void
foundry_git_diff_class_init (FoundryGitDiffClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_git_diff_finalize;
}

static void
foundry_git_diff_init (FoundryGitDiff *self)
{
  g_mutex_init (&self->mutex);
}

FoundryGitDiff *
_foundry_git_diff_new (git_diff *diff)
{
  FoundryGitDiff *self;

  g_return_val_if_fail (diff != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_DIFF, NULL);
  self->diff = g_steal_pointer (&diff);

  return self;
}
