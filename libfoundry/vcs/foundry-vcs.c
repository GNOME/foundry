/* foundry-vcs.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "foundry-vcs-blame.h"
#include "foundry-vcs-private.h"
#include "foundry-vcs-manager.h"
#include "foundry-util.h"

typedef struct _FoundryVcsPrivate
{
  GWeakRef provider_wr;
} FoundryVcsPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryVcs, foundry_vcs, FOUNDRY_TYPE_CONTEXTUAL)

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_BRANCH_NAME,
  PROP_ID,
  PROP_NAME,
  PROP_PRIORITY,
  PROP_PROVIDER,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean
foundry_vcs_real_is_file_ignored (FoundryVcs *self,
                                  GFile      *file)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GFile) project_dir = NULL;

  g_assert (FOUNDRY_IS_VCS (self));
  g_assert (G_IS_FILE (file));

  if ((context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))) &&
      (project_dir = foundry_context_dup_project_directory (context)) &&
      g_file_has_prefix (file, project_dir))
    {
      g_autofree char *relative_path = g_file_get_relative_path (project_dir, file);
      return foundry_vcs_is_ignored (self, relative_path);
    }

  return FALSE;
}

static void
foundry_vcs_finalize (GObject *object)
{
  FoundryVcs *self = (FoundryVcs *)object;
  FoundryVcsPrivate *priv = foundry_vcs_get_instance_private (self);

  g_weak_ref_clear (&priv->provider_wr);

  G_OBJECT_CLASS (foundry_vcs_parent_class)->finalize (object);
}

static void
foundry_vcs_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  FoundryVcs *self = FOUNDRY_VCS (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, foundry_vcs_get_active (self));
      break;

    case PROP_BRANCH_NAME:
      g_value_take_string (value, foundry_vcs_dup_branch_name (self));
      break;

    case PROP_ID:
      g_value_take_string (value, foundry_vcs_dup_id (self));
      break;

    case PROP_NAME:
      g_value_take_string (value, foundry_vcs_dup_name (self));
      break;

    case PROP_PRIORITY:
      g_value_set_uint (value, foundry_vcs_get_priority (self));
      break;

    case PROP_PROVIDER:
      g_value_take_object (value, _foundry_vcs_dup_provider (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_class_init (FoundryVcsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_vcs_finalize;
  object_class->get_property = foundry_vcs_get_property;

  klass->is_file_ignored = foundry_vcs_real_is_file_ignored;

  properties[PROP_ACTIVE] =
    g_param_spec_boolean ("active", NULL, NULL,
                         FALSE,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_BRANCH_NAME] =
    g_param_spec_string ("branch-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PRIORITY] =
    g_param_spec_uint ("priority", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_PROVIDER] =
    g_param_spec_object ("provider", NULL, NULL,
                         FOUNDRY_TYPE_VCS_PROVIDER,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_vcs_init (FoundryVcs *self)
{
}

gboolean
foundry_vcs_get_active (FoundryVcs *self)
{
  g_autoptr(FoundryContext) context = NULL;

  g_return_val_if_fail (FOUNDRY_IS_VCS (self), FALSE);

  if ((context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))))
    {
      g_autoptr(FoundryVcsManager) vcs_manager = foundry_context_dup_vcs_manager (context);
      g_autoptr(FoundryVcs) vcs = foundry_vcs_manager_dup_vcs (vcs_manager);

      return vcs == self;
    }

  return FALSE;
}

/**
 * foundry_vcs_dup_id:
 * @self: a #FoundryVcs
 *
 * Gets the identifier for the VCS such as "git" or "none".
 *
 * Returns: (transfer full): a string containing the identifier
 */
char *
foundry_vcs_dup_id (FoundryVcs *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS (self), NULL);

  return FOUNDRY_VCS_GET_CLASS (self)->dup_id (self);
}

/**
 * foundry_vcs_dup_name:
 * @self: a #FoundryVcs
 *
 * Gets the name of the vcs in title format such as "Git"
 *
 * Returns: (transfer full): a string containing the name
 */
char *
foundry_vcs_dup_name (FoundryVcs *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS (self), NULL);

  return FOUNDRY_VCS_GET_CLASS (self)->dup_name (self);
}

/**
 * foundry_vcs_dup_branch_name:
 * @self: a #FoundryVcs
 *
 * Gets the name of the branch such as "main".
 *
 * Returns: (transfer full): a string containing the branch name
 */
char *
foundry_vcs_dup_branch_name (FoundryVcs *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS (self), NULL);

  return FOUNDRY_VCS_GET_CLASS (self)->dup_branch_name (self);
}

FoundryVcsProvider *
_foundry_vcs_dup_provider (FoundryVcs *self)
{
  FoundryVcsPrivate *priv = foundry_vcs_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_VCS (self), NULL);

  return g_weak_ref_get (&priv->provider_wr);
}

void
_foundry_vcs_set_provider (FoundryVcs         *self,
                           FoundryVcsProvider *provider)
{
  FoundryVcsPrivate *priv = foundry_vcs_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_VCS (self));
  g_return_if_fail (!provider || FOUNDRY_IS_VCS_PROVIDER (provider));

  g_weak_ref_set (&priv->provider_wr, provider);
}

guint
foundry_vcs_get_priority (FoundryVcs *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS (self), 0);

  if (FOUNDRY_VCS_GET_CLASS (self)->get_priority)
    return FOUNDRY_VCS_GET_CLASS (self)->get_priority (self);

  return 0;
}

gboolean
foundry_vcs_is_ignored (FoundryVcs *self,
                        const char *relative_path)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS (self), FALSE);
  g_return_val_if_fail (relative_path != NULL, FALSE);

  if (FOUNDRY_VCS_GET_CLASS (self)->is_ignored)
    return FOUNDRY_VCS_GET_CLASS (self)->is_ignored (self, relative_path);

  return FALSE;
}

gboolean
foundry_vcs_is_file_ignored (FoundryVcs *self,
                             GFile      *file)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  return FOUNDRY_VCS_GET_CLASS (self)->is_file_ignored (self, file);
}

/**
 * foundry_vcs_list_files:
 * @self: a [class@Foundry.Vcs]
 *
 * List all files in the repository.
 *
 * It is not required that implementations return files that are not
 * indexed in their caches from this method.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves
 *   to [iface@Gio.ListModel] of [class@Foundry.VcsFile].
 */
DexFuture *
foundry_vcs_list_files (FoundryVcs *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_VCS (self));

  if (FOUNDRY_VCS_GET_CLASS (self)->list_files)
    return FOUNDRY_VCS_GET_CLASS (self)->list_files (self);

  return foundry_future_new_not_supported ();
}

/**
 * foundry_vcs_find_file:
 * @self: a [class@Foundry.Vcs]
 * @file: the file to locate
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.VcsFile] or rejects with error
 */
DexFuture *
foundry_vcs_find_file (FoundryVcs *self,
                       GFile      *file)
{
  dex_return_error_if_fail (FOUNDRY_IS_VCS (self));

  if (FOUNDRY_VCS_GET_CLASS (self)->find_file)
    return FOUNDRY_VCS_GET_CLASS (self)->find_file (self, file);

  return foundry_future_new_not_supported ();
}

/**
 * foundry_vcs_blame:
 * @self: a [class@Foundry.Vcs]
 * @file: a [class@Foundry.VcsFile]
 * @bytes: (nullable): optional contents for the file
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.VcsBlame] or rejects with error
 */
DexFuture *
foundry_vcs_blame (FoundryVcs     *self,
                   FoundryVcsFile *file,
                   GBytes         *bytes)
{
  dex_return_error_if_fail (FOUNDRY_IS_VCS (self));

  if (FOUNDRY_VCS_GET_CLASS (self)->blame)
    return FOUNDRY_VCS_GET_CLASS (self)->blame (self, file, bytes);

  return foundry_future_new_not_supported ();
}

/**
 * foundry_vcs_list_branches:
 * @self: a [class@Foundry.Vcs]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.VcsBranch].
 */
DexFuture *
foundry_vcs_list_branches (FoundryVcs *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_VCS (self));

  if (FOUNDRY_VCS_GET_CLASS (self)->list_branches)
    return FOUNDRY_VCS_GET_CLASS (self)->list_branches (self);

  return foundry_future_new_not_supported ();
}

/**
 * foundry_vcs_list_tags:
 * @self: a [class@Foundry.Vcs]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.VcsTag].
 */
DexFuture *
foundry_vcs_list_tags (FoundryVcs *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_VCS (self));

  if (FOUNDRY_VCS_GET_CLASS (self)->list_tags)
    return FOUNDRY_VCS_GET_CLASS (self)->list_tags (self);

  return foundry_future_new_not_supported ();
}
