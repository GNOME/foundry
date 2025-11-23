/* foundry-git-delta.c
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

#include "foundry-git-delta-private.h"

struct _FoundryGitDelta
{
  GObject parent_instance;

  char *old_path;
  char *new_path;

  git_oid old_oid;
  git_oid new_oid;

  guint old_mode;
  guint new_mode;
  FoundryVcsDeltaStatus status;
};

G_DEFINE_FINAL_TYPE (FoundryGitDelta, foundry_git_delta, FOUNDRY_TYPE_VCS_DELTA)

static char *
foundry_git_delta_dup_old_path (FoundryVcsDelta *delta)
{
  return g_strdup (FOUNDRY_GIT_DELTA (delta)->old_path);
}

static char *
foundry_git_delta_dup_new_path (FoundryVcsDelta *delta)
{
  return g_strdup (FOUNDRY_GIT_DELTA (delta)->new_path);
}

static char *
foundry_git_delta_dup_old_id (FoundryVcsDelta *delta)
{
  FoundryGitDelta *self = FOUNDRY_GIT_DELTA (delta);
  char str[GIT_OID_HEXSZ + 1];

  git_oid_tostr (str, sizeof str, &self->old_oid);
  str[GIT_OID_HEXSZ] = 0;

  return g_strdup (str);
}

static char *
foundry_git_delta_dup_new_id (FoundryVcsDelta *delta)
{
  FoundryGitDelta *self = FOUNDRY_GIT_DELTA (delta);
  char str[GIT_OID_HEXSZ + 1];

  git_oid_tostr (str, sizeof str, &self->new_oid);
  str[GIT_OID_HEXSZ] = 0;

  return g_strdup (str);
}

static guint
foundry_git_delta_get_old_mode (FoundryVcsDelta *delta)
{
  return FOUNDRY_GIT_DELTA (delta)->old_mode;
}

static guint
foundry_git_delta_get_new_mode (FoundryVcsDelta *delta)
{
  return FOUNDRY_GIT_DELTA (delta)->new_mode;
}

static FoundryVcsDeltaStatus
foundry_git_delta_get_status (FoundryVcsDelta *delta)
{
  return FOUNDRY_GIT_DELTA (delta)->status;
}

static void
foundry_git_delta_finalize (GObject *object)
{
  FoundryGitDelta *self = (FoundryGitDelta *)object;

  g_clear_pointer (&self->old_path, g_free);
  g_clear_pointer (&self->new_path, g_free);

  G_OBJECT_CLASS (foundry_git_delta_parent_class)->finalize (object);
}

static void
foundry_git_delta_class_init (FoundryGitDeltaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsDeltaClass *vcs_delta_class = FOUNDRY_VCS_DELTA_CLASS (klass);

  object_class->finalize = foundry_git_delta_finalize;

  vcs_delta_class->dup_old_path = foundry_git_delta_dup_old_path;
  vcs_delta_class->dup_new_path = foundry_git_delta_dup_new_path;
  vcs_delta_class->dup_old_id = foundry_git_delta_dup_old_id;
  vcs_delta_class->dup_new_id = foundry_git_delta_dup_new_id;
  vcs_delta_class->get_old_mode = foundry_git_delta_get_old_mode;
  vcs_delta_class->get_new_mode = foundry_git_delta_get_new_mode;
  vcs_delta_class->get_status = foundry_git_delta_get_status;
}

static void
foundry_git_delta_init (FoundryGitDelta *self)
{
}

static FoundryVcsDeltaStatus
map_git_delta_status (git_delta_t git_status)
{
  switch (git_status)
    {
    case GIT_DELTA_UNMODIFIED:
      return FOUNDRY_VCS_DELTA_STATUS_UNMODIFIED;
    case GIT_DELTA_ADDED:
      return FOUNDRY_VCS_DELTA_STATUS_ADDED;
    case GIT_DELTA_DELETED:
      return FOUNDRY_VCS_DELTA_STATUS_DELETED;
    case GIT_DELTA_MODIFIED:
      return FOUNDRY_VCS_DELTA_STATUS_MODIFIED;
    case GIT_DELTA_RENAMED:
      return FOUNDRY_VCS_DELTA_STATUS_RENAMED;
    case GIT_DELTA_COPIED:
      return FOUNDRY_VCS_DELTA_STATUS_COPIED;
    case GIT_DELTA_IGNORED:
      return FOUNDRY_VCS_DELTA_STATUS_IGNORED;
    case GIT_DELTA_UNTRACKED:
      return FOUNDRY_VCS_DELTA_STATUS_UNTRACKED;
    case GIT_DELTA_TYPECHANGE:
      return FOUNDRY_VCS_DELTA_STATUS_TYPECHANGE;
    case GIT_DELTA_UNREADABLE:
      return FOUNDRY_VCS_DELTA_STATUS_UNREADABLE;
    case GIT_DELTA_CONFLICTED:
      return FOUNDRY_VCS_DELTA_STATUS_CONFLICTED;
    default:
      return FOUNDRY_VCS_DELTA_STATUS_UNMODIFIED;
    }
}

FoundryGitDelta *
_foundry_git_delta_new (const git_diff_delta *delta)
{
  FoundryGitDelta *self;

  g_return_val_if_fail (delta != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_GIT_DELTA, NULL);
  self->old_path = g_strdup (delta->old_file.path);
  self->new_path = g_strdup (delta->new_file.path);
  self->old_oid = delta->old_file.id;
  self->new_oid = delta->new_file.id;
  self->old_mode = delta->old_file.mode;
  self->new_mode = delta->new_file.mode;
  self->status = map_git_delta_status (delta->status);

  return self;
}
