/* foundry-git-diff-line.c
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

#include "foundry-git-diff-line-private.h"
#include "foundry-git-patch-private.h"

struct _FoundryGitDiffLine
{
  FoundryVcsDiffLine parent_instance;
  FoundryGitPatch *patch;
  gsize hunk_idx;
  gsize line_idx;
};

G_DEFINE_FINAL_TYPE (FoundryGitDiffLine, foundry_git_diff_line, FOUNDRY_TYPE_VCS_DIFF_LINE)

static FoundryVcsDiffLineOrigin
map_git_line_origin (char origin)
{
  switch (origin)
    {
    case ' ':
      return FOUNDRY_VCS_DIFF_LINE_ORIGIN_CONTEXT;

    case '+':
      return FOUNDRY_VCS_DIFF_LINE_ORIGIN_ADDED;

    case '-':
      return FOUNDRY_VCS_DIFF_LINE_ORIGIN_DELETED;

    case '=':
      return FOUNDRY_VCS_DIFF_LINE_ORIGIN_CONTEXT_EOFNL;

    case '>':
      return FOUNDRY_VCS_DIFF_LINE_ORIGIN_ADD_EOFNL;

    case '<':
      return FOUNDRY_VCS_DIFF_LINE_ORIGIN_DEL_EOFNL;

    default:
      return FOUNDRY_VCS_DIFF_LINE_ORIGIN_CONTEXT;
    }
}

static void
foundry_git_diff_line_finalize (GObject *object)
{
  FoundryGitDiffLine *self = (FoundryGitDiffLine *)object;

  g_clear_pointer (&self->patch, _foundry_git_patch_unref);

  G_OBJECT_CLASS (foundry_git_diff_line_parent_class)->finalize (object);
}

static FoundryVcsDiffLineOrigin
foundry_git_diff_line_get_origin (FoundryVcsDiffLine *line)
{
  FoundryGitDiffLine *self = FOUNDRY_GIT_DIFF_LINE (line);
  const git_diff_line *gline;

  g_assert (FOUNDRY_IS_GIT_DIFF_LINE (self));
  g_assert (self->patch != NULL);

  gline = _foundry_git_patch_get_line (self->patch, self->hunk_idx, self->line_idx);

  if (gline == NULL)
    return FOUNDRY_VCS_DIFF_LINE_ORIGIN_CONTEXT;

  return map_git_line_origin (gline->origin);
}

static guint
foundry_git_diff_line_get_old_line (FoundryVcsDiffLine *line)
{
  FoundryGitDiffLine *self = FOUNDRY_GIT_DIFF_LINE (line);
  const git_diff_line *gline;

  g_assert (FOUNDRY_IS_GIT_DIFF_LINE (self));
  g_assert (self->patch != NULL);

  gline = _foundry_git_patch_get_line (self->patch, self->hunk_idx, self->line_idx);

  if (gline == NULL)
    return 0;

  return gline->old_lineno;
}

static guint
foundry_git_diff_line_get_new_line (FoundryVcsDiffLine *line)
{
  FoundryGitDiffLine *self = FOUNDRY_GIT_DIFF_LINE (line);
  const git_diff_line *gline;

  g_assert (FOUNDRY_IS_GIT_DIFF_LINE (self));
  g_assert (self->patch != NULL);

  gline = _foundry_git_patch_get_line (self->patch, self->hunk_idx, self->line_idx);

  if (gline == NULL)
    return 0;

  return gline->new_lineno;
}

static char *
foundry_git_diff_line_dup_text (FoundryVcsDiffLine *line)
{
  FoundryGitDiffLine *self = FOUNDRY_GIT_DIFF_LINE (line);
  const git_diff_line *gline;

  g_assert (FOUNDRY_IS_GIT_DIFF_LINE (self));
  g_assert (self->patch != NULL);

  gline = _foundry_git_patch_get_line (self->patch, self->hunk_idx, self->line_idx);

  if (gline == NULL)
    return NULL;

  return g_strndup ((const char *)gline->content, gline->content_len);
}

static gboolean
foundry_git_diff_line_get_has_newline (FoundryVcsDiffLine *line)
{
  FoundryGitDiffLine *self = FOUNDRY_GIT_DIFF_LINE (line);
  const git_diff_line *gline;

  g_assert (FOUNDRY_IS_GIT_DIFF_LINE (self));
  g_assert (self->patch != NULL);

  gline = _foundry_git_patch_get_line (self->patch, self->hunk_idx, self->line_idx);

  if (gline == NULL)
    return FALSE;

  return (gline->content_len > 0 && gline->content[gline->content_len - 1] == '\n');
}

static gsize
foundry_git_diff_line_get_length (FoundryVcsDiffLine *line)
{
  FoundryGitDiffLine *self = FOUNDRY_GIT_DIFF_LINE (line);
  const git_diff_line *gline;

  g_assert (FOUNDRY_IS_GIT_DIFF_LINE (self));
  g_assert (self->patch != NULL);

  gline = _foundry_git_patch_get_line (self->patch, self->hunk_idx, self->line_idx);

  if (gline == NULL)
    return 0;

  return gline->content_len;
}

static void
foundry_git_diff_line_class_init (FoundryGitDiffLineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsDiffLineClass *line_class = FOUNDRY_VCS_DIFF_LINE_CLASS (klass);

  object_class->finalize = foundry_git_diff_line_finalize;

  line_class->get_origin = foundry_git_diff_line_get_origin;
  line_class->get_old_line = foundry_git_diff_line_get_old_line;
  line_class->get_new_line = foundry_git_diff_line_get_new_line;
  line_class->dup_text = foundry_git_diff_line_dup_text;
  line_class->get_has_newline = foundry_git_diff_line_get_has_newline;
  line_class->get_length = foundry_git_diff_line_get_length;
}

static void
foundry_git_diff_line_init (FoundryGitDiffLine *self)
{
}

FoundryGitDiffLine *
_foundry_git_diff_line_new (FoundryGitPatch *patch,
                            gsize            hunk_idx,
                            gsize            line_idx)
{
  FoundryGitDiffLine *self;

  g_return_val_if_fail (patch != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_DIFF_LINE, NULL);
  self->patch = _foundry_git_patch_ref (patch);
  self->hunk_idx = hunk_idx;
  self->line_idx = line_idx;

  return self;
}

FoundryGitPatch *
_foundry_git_diff_line_get_patch (FoundryGitDiffLine *line)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_DIFF_LINE (line), NULL);

  return line->patch ? _foundry_git_patch_ref (line->patch) : NULL;
}

gsize
_foundry_git_diff_line_get_hunk_idx (FoundryGitDiffLine *line)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_DIFF_LINE (line), 0);

  return line->hunk_idx;
}

gsize
_foundry_git_diff_line_get_line_idx (FoundryGitDiffLine *line)
{
  g_return_val_if_fail (FOUNDRY_IS_GIT_DIFF_LINE (line), 0);

  return line->line_idx;
}
