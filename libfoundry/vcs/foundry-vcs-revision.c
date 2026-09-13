/* foundry-vcs-revision.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#include "foundry-vcs-commit.h"
#include "foundry-vcs-revision-private.h"
#include "foundry-vcs-tree.h"

struct _FoundryVcsRevision
{
  GObject parent_instance;
  FoundryVcsRevisionKind kind;
  GObject *object;
};

G_DEFINE_FINAL_TYPE (FoundryVcsRevision, foundry_vcs_revision, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_KIND,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_vcs_revision_finalize (GObject *object)
{
  FoundryVcsRevision *self = (FoundryVcsRevision *)object;

  g_clear_object (&self->object);

  G_OBJECT_CLASS (foundry_vcs_revision_parent_class)->finalize (object);
}

static void
foundry_vcs_revision_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  FoundryVcsRevision *self = FOUNDRY_VCS_REVISION (object);

  switch (prop_id)
    {
    case PROP_KIND:
      g_value_set_enum (value, foundry_vcs_revision_get_kind (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_revision_class_init (FoundryVcsRevisionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_vcs_revision_finalize;
  object_class->get_property = foundry_vcs_revision_get_property;

  properties[PROP_KIND] =
    g_param_spec_enum ("kind", NULL, NULL,
                       FOUNDRY_TYPE_VCS_REVISION_KIND,
                       FOUNDRY_VCS_REVISION_KIND_EMPTY,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_vcs_revision_init (FoundryVcsRevision *self)
{
  self->kind = FOUNDRY_VCS_REVISION_KIND_EMPTY;
}

static FoundryVcsRevision *
foundry_vcs_revision_new (FoundryVcsRevisionKind  kind,
                          GObject                *object)
{
  FoundryVcsRevision *self;

  self = g_object_new (FOUNDRY_TYPE_VCS_REVISION, NULL);
  self->kind = kind;
  self->object = g_steal_pointer (&object);

  return self;
}

/**
 * foundry_vcs_revision_new_for_commit:
 * @commit: a [class@Foundry.VcsCommit]
 *
 * Creates a revision endpoint for a commit.
 *
 * Returns: (transfer full): a new revision endpoint.
 *
 * Since: 1.3
 */
FoundryVcsRevision *
foundry_vcs_revision_new_for_commit (FoundryVcsCommit *commit)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_COMMIT (commit), NULL);

  return foundry_vcs_revision_new (FOUNDRY_VCS_REVISION_KIND_COMMIT,
                                   G_OBJECT (g_object_ref (commit)));
}

FoundryVcsRevision *
_foundry_vcs_revision_new_take_commit (FoundryVcsCommit *commit)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_COMMIT (commit), NULL);

  return foundry_vcs_revision_new (FOUNDRY_VCS_REVISION_KIND_COMMIT,
                                   G_OBJECT (commit));
}

/**
 * foundry_vcs_revision_new_for_tree:
 * @tree: a [class@Foundry.VcsTree]
 *
 * Creates a revision endpoint for a tree.
 *
 * Returns: (transfer full): a new revision endpoint.
 *
 * Since: 1.3
 */
FoundryVcsRevision *
foundry_vcs_revision_new_for_tree (FoundryVcsTree *tree)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_TREE (tree), NULL);

  return foundry_vcs_revision_new (FOUNDRY_VCS_REVISION_KIND_TREE,
                                   G_OBJECT (g_object_ref (tree)));
}

FoundryVcsRevision *
_foundry_vcs_revision_new_take_tree (FoundryVcsTree *tree)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_TREE (tree), NULL);

  return foundry_vcs_revision_new (FOUNDRY_VCS_REVISION_KIND_TREE,
                                   G_OBJECT (tree));
}

/**
 * foundry_vcs_revision_new_for_index:
 *
 * Creates a revision endpoint for the current index.
 *
 * Returns: (transfer full): a new revision endpoint.
 *
 * Since: 1.3
 */
FoundryVcsRevision *
foundry_vcs_revision_new_for_index (void)
{
  return foundry_vcs_revision_new (FOUNDRY_VCS_REVISION_KIND_INDEX, NULL);
}

/**
 * foundry_vcs_revision_new_for_worktree:
 *
 * Creates a revision endpoint for the current working tree.
 *
 * Returns: (transfer full): a new revision endpoint.
 *
 * Since: 1.3
 */
FoundryVcsRevision *
foundry_vcs_revision_new_for_worktree (void)
{
  return foundry_vcs_revision_new (FOUNDRY_VCS_REVISION_KIND_WORKTREE, NULL);
}

/**
 * foundry_vcs_revision_new_empty:
 *
 * Creates an empty revision endpoint.
 *
 * Returns: (transfer full): a new revision endpoint.
 *
 * Since: 1.3
 */
FoundryVcsRevision *
foundry_vcs_revision_new_empty (void)
{
  return foundry_vcs_revision_new (FOUNDRY_VCS_REVISION_KIND_EMPTY, NULL);
}

/**
 * foundry_vcs_revision_get_kind:
 * @self: a [class@Foundry.VcsRevision]
 *
 * Gets the endpoint kind for the revision.
 *
 * Returns: the endpoint kind.
 *
 * Since: 1.3
 */
FoundryVcsRevisionKind
foundry_vcs_revision_get_kind (FoundryVcsRevision *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_REVISION (self), 0);

  return self->kind;
}

/**
 * foundry_vcs_revision_dup_commit:
 * @self: a [class@Foundry.VcsRevision]
 *
 * Gets the commit for a commit revision.
 *
 * Returns: (transfer full) (nullable): the commit, or %NULL.
 *
 * Since: 1.3
 */
FoundryVcsCommit *
foundry_vcs_revision_dup_commit (FoundryVcsRevision *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_REVISION (self), NULL);

  if (self->kind != FOUNDRY_VCS_REVISION_KIND_COMMIT)
    return NULL;

  return g_object_ref (FOUNDRY_VCS_COMMIT (self->object));
}

/**
 * foundry_vcs_revision_dup_tree:
 * @self: a [class@Foundry.VcsRevision]
 *
 * Gets the tree for a tree revision.
 *
 * Returns: (transfer full) (nullable): the tree, or %NULL.
 *
 * Since: 1.3
 */
FoundryVcsTree *
foundry_vcs_revision_dup_tree (FoundryVcsRevision *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_REVISION (self), NULL);

  if (self->kind != FOUNDRY_VCS_REVISION_KIND_TREE)
    return NULL;

  return g_object_ref (FOUNDRY_VCS_TREE (self->object));
}

G_DEFINE_ENUM_TYPE (FoundryVcsRevisionKind, foundry_vcs_revision_kind,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_VCS_REVISION_KIND_COMMIT, "commit"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_VCS_REVISION_KIND_TREE, "tree"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_VCS_REVISION_KIND_INDEX, "index"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_VCS_REVISION_KIND_WORKTREE, "worktree"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_VCS_REVISION_KIND_EMPTY, "empty"))
