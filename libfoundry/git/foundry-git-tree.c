/* foundry-git-tree.c
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

#include "foundry-git-tree-private.h"

struct _FoundryGitTree
{
  FoundryVcsTree  parent_instance;
  GMutex          mutex;
  git_tree       *tree;
};

G_DEFINE_FINAL_TYPE (FoundryGitTree, foundry_git_tree, FOUNDRY_TYPE_VCS_TREE)

static void
foundry_git_tree_finalize (GObject *object)
{
  FoundryGitTree *self = (FoundryGitTree *)object;

  g_clear_pointer (&self->tree, git_tree_free);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (foundry_git_tree_parent_class)->finalize (object);
}

static void
foundry_git_tree_class_init (FoundryGitTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_git_tree_finalize;
}

static void
foundry_git_tree_init (FoundryGitTree *self)
{
  g_mutex_init (&self->mutex);
}

FoundryGitTree *
_foundry_git_tree_new (git_tree *tree)
{
  FoundryGitTree *self;

  g_return_val_if_fail (tree != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_TREE, NULL);
  self->tree = g_steal_pointer (&tree);

  return self;
}
