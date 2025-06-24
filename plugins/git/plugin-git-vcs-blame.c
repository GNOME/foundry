/* plugin-git-vcs-blame.c
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

#include "plugin-git-autocleanups.h"
#include "plugin-git-vcs-blame.h"
#include "plugin-git-vcs-signature.h"

struct _PluginGitVcsBlame
{
  FoundryVcsBlame parent_instance;
  FoundryVcsFile *file;
  git_blame *base_blame;
  git_blame *bytes_blame;
};

G_DEFINE_FINAL_TYPE (PluginGitVcsBlame, plugin_git_vcs_blame, FOUNDRY_TYPE_VCS_BLAME)

static git_blame *
get_blame (PluginGitVcsBlame *self)
{
  return self->bytes_blame ? self->bytes_blame : self->base_blame;
}

static DexFuture *
plugin_git_vcs_blame_update (FoundryVcsBlame *vcs_blame,
                             GBytes          *contents)
{
  PluginGitVcsBlame *self = (PluginGitVcsBlame *)vcs_blame;
  g_autoptr(git_blame) blame = NULL;
  gconstpointer data = NULL;
  gsize size;

  dex_return_error_if_fail (PLUGIN_IS_GIT_VCS_BLAME (self));
  dex_return_error_if_fail (contents != NULL);

  if (contents != NULL)
    data = g_bytes_get_data (contents, &size);

  g_clear_pointer (&self->bytes_blame, git_blame_free);

  if (git_blame_buffer (&blame, self->base_blame, data, size) == 0)
    self->bytes_blame = g_steal_pointer (&blame);

  return dex_future_new_true ();
}

static FoundryVcsSignature *
plugin_git_vcs_blame_query_line (FoundryVcsBlame *blame,
                                 guint            line)
{
  PluginGitVcsBlame *self = (PluginGitVcsBlame *)blame;
  const git_blame_hunk *hunk;
  git_blame *gblame;

  g_assert (PLUGIN_IS_GIT_VCS_BLAME (self));
  g_assert (self->base_blame != NULL);

  gblame = get_blame (self);

  if ((hunk = git_blame_get_hunk_byline (gblame, line + 1)))
    return plugin_git_vcs_signature_new (&hunk->final_commit_id, hunk->final_signature);

  return NULL;
}

static guint
plugin_git_vcs_blame_get_n_lines (FoundryVcsBlame *blame)
{
  PluginGitVcsBlame *self = (PluginGitVcsBlame *)blame;
  git_blame *gblame;
  gsize hunk_count;
  guint n_lines = 0;

  g_assert (PLUGIN_IS_GIT_VCS_BLAME (self));
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
plugin_git_vcs_blame_finalize (GObject *object)
{
  PluginGitVcsBlame *self = (PluginGitVcsBlame *)object;

  g_clear_object (&self->file);
  g_clear_pointer (&self->bytes_blame, git_blame_free);
  g_clear_pointer (&self->base_blame, git_blame_free);

  G_OBJECT_CLASS (plugin_git_vcs_blame_parent_class)->finalize (object);
}

static void
plugin_git_vcs_blame_class_init (PluginGitVcsBlameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsBlameClass *vcs_blame_class = FOUNDRY_VCS_BLAME_CLASS (klass);

  object_class->finalize = plugin_git_vcs_blame_finalize;

  vcs_blame_class->update = plugin_git_vcs_blame_update;
  vcs_blame_class->query_line = plugin_git_vcs_blame_query_line;
  vcs_blame_class->get_n_lines = plugin_git_vcs_blame_get_n_lines;
}

static void
plugin_git_vcs_blame_init (PluginGitVcsBlame *self)
{
}

PluginGitVcsBlame *
plugin_git_vcs_blame_new (FoundryVcsFile *file,
                          git_blame      *base_blame,
                          git_blame      *bytes_blame)
{
  PluginGitVcsBlame *self;

  g_return_val_if_fail (FOUNDRY_IS_VCS_FILE (file), NULL);
  g_return_val_if_fail (base_blame != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_GIT_VCS_BLAME,
                       "file", file,
                       NULL);

  self->base_blame = base_blame;
  self->bytes_blame = bytes_blame;

  return self;
}
