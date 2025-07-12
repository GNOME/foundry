/* foundry-git-tag.c
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

#include "foundry-git-reference-private.h"
#include "foundry-git-tag-private.h"

struct _FoundryGitTag
{
  FoundryVcsTag parent_instance;
  FoundryGitVcs *vcs;
  git_oid oid;
  char *name;
};

G_DEFINE_FINAL_TYPE (FoundryGitTag, foundry_git_tag, FOUNDRY_TYPE_VCS_TAG)

static char *
foundry_git_tag_dup_id (FoundryVcsTag *tag)
{
  FoundryGitTag *self = (FoundryGitTag *)tag;

  g_assert (FOUNDRY_IS_GIT_TAG (self));

  return g_strdup (self->name);
}

static char *
foundry_git_tag_dup_title (FoundryVcsTag *tag)
{
  FoundryGitTag *self = (FoundryGitTag *)tag;
  const char *suffix;

  g_assert (FOUNDRY_IS_GIT_TAG (self));

  if ((suffix = strrchr (self->name, '/')))
    return g_strdup (++suffix);

  return g_strdup (self->name);
}

static gboolean
foundry_git_tag_is_local (FoundryVcsTag *tag)
{
  return g_str_has_prefix (FOUNDRY_GIT_TAG (tag)->name, "refs/tags/");
}

static DexFuture *
foundry_git_tag_load_target (FoundryVcsTag *tag)
{
  FoundryGitTag *self = (FoundryGitTag *)tag;

  dex_return_error_if_fail (FOUNDRY_IS_GIT_TAG (self));

  return dex_future_new_take_object (_foundry_git_reference_new (self->vcs, &self->oid));
}

static void
foundry_git_tag_finalize (GObject *object)
{
  FoundryGitTag *self = (FoundryGitTag *)object;

  g_clear_object (&self->vcs);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (foundry_git_tag_parent_class)->finalize (object);
}

static void
foundry_git_tag_class_init (FoundryGitTagClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryVcsTagClass *vcs_tag_class = FOUNDRY_VCS_TAG_CLASS (klass);

  object_class->finalize = foundry_git_tag_finalize;

  vcs_tag_class->dup_id = foundry_git_tag_dup_id;
  vcs_tag_class->dup_title = foundry_git_tag_dup_title;
  vcs_tag_class->is_local = foundry_git_tag_is_local;
  vcs_tag_class->load_target = foundry_git_tag_load_target;
}

static void
foundry_git_tag_init (FoundryGitTag *self)
{
}

/**
 * foundry_git_tag_new:
 */
FoundryGitTag *
_foundry_git_tag_new (FoundryGitVcs *vcs,
                      git_reference *ref)
{
  FoundryGitTag *self;
  const git_oid *oid;
  const char *name;

  g_return_val_if_fail (ref != NULL, NULL);

  if (!(name = git_reference_name (ref)))
    return NULL;

  if (!(oid = git_reference_target (ref)))
    return NULL;

  self = g_object_new (FOUNDRY_TYPE_GIT_TAG, NULL);
  self->oid = *oid;
  self->name = g_strdup (name);
  self->vcs = g_object_ref (vcs);

  return self;
}
