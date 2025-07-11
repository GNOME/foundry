/* foundry-git-file-list.c
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

#include "foundry-git-file-list.h"
#include "foundry-git-file-private.h"

struct _FoundryGitFileList
{
  FoundryContextual  parent_instance;
  git_index         *index;
  GFile             *workdir;
};

static GType
foundry_git_file_list_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_VCS_FILE;
}

static guint
foundry_git_file_list_get_n_items (GListModel *model)
{
  FoundryGitFileList *self = FOUNDRY_GIT_FILE_LIST (model);

  return git_index_entrycount (self->index);
}

static gpointer
foundry_git_file_list_get_item (GListModel *model,
                               guint       position)
{
  FoundryGitFileList *self = FOUNDRY_GIT_FILE_LIST (model);
  const git_index_entry *entry;

  if (position >= git_index_entrycount (self->index))
    return NULL;

  entry = git_index_get_byindex (self->index, position);

  return foundry_git_file_new (self->workdir, entry->path);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_git_file_list_get_item_type;
  iface->get_n_items = foundry_git_file_list_get_n_items;
  iface->get_item = foundry_git_file_list_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryGitFileList, foundry_git_file_list, FOUNDRY_TYPE_CONTEXTUAL,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
foundry_git_file_list_finalize (GObject *object)
{
  FoundryGitFileList *self = (FoundryGitFileList *)object;

  g_clear_pointer (&self->index, git_index_free);
  g_clear_object (&self->workdir);

  G_OBJECT_CLASS (foundry_git_file_list_parent_class)->finalize (object);
}

static void
foundry_git_file_list_class_init (FoundryGitFileListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_git_file_list_finalize;
}

static void
foundry_git_file_list_init (FoundryGitFileList *self)
{
}

FoundryGitFileList *
foundry_git_file_list_new (FoundryContext *context,
                          GFile          *workdir,
                          git_index      *index)
{
  FoundryGitFileList *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (G_IS_FILE (workdir), NULL);
  g_return_val_if_fail (index != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_FILE_LIST,
                       "context", context,
                       NULL);
  self->workdir = g_object_ref (workdir);
  self->index = index;

  return self;
}
