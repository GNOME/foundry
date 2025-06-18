/* foundry-vcs-blame.c
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

#include "foundry-vcs-blame.h"
#include "foundry-vcs-file.h"

typedef struct
{
  FoundryVcsFile *file;
} FoundryVcsBlamePrivate;

enum {
  PROP_0,
  PROP_FILE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryVcsBlame, foundry_vcs_blame, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_vcs_blame_finalize (GObject *object)
{
  FoundryVcsBlame *self = (FoundryVcsBlame *)object;
  FoundryVcsBlamePrivate *priv = foundry_vcs_blame_get_instance_private (self);

  g_clear_object (&priv->file);

  G_OBJECT_CLASS (foundry_vcs_blame_parent_class)->finalize (object);
}

static void
foundry_vcs_blame_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  FoundryVcsBlame *self = FOUNDRY_VCS_BLAME (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_take_object (value, foundry_vcs_blame_dup_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_blame_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  FoundryVcsBlame *self = FOUNDRY_VCS_BLAME (object);
  FoundryVcsBlamePrivate *priv = foundry_vcs_blame_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_FILE:
      priv->file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_blame_class_init (FoundryVcsBlameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_vcs_blame_finalize;
  object_class->get_property = foundry_vcs_blame_get_property;
  object_class->set_property = foundry_vcs_blame_set_property;

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         FOUNDRY_TYPE_VCS_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_vcs_blame_init (FoundryVcsBlame *self)
{
}

/**
 * foundry_vcs_blame_dup_file:
 * @self: a [class@Foundry.VcsBlame]
 *
 * Gets the underlying file being blamed
 *
 * Returns: (transfer full): a [class@Foundry.VcsFile]
 */
FoundryVcsFile *
foundry_vcs_blame_dup_file (FoundryVcsBlame *self)
{
  FoundryVcsBlamePrivate *priv = foundry_vcs_blame_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_VCS_BLAME (self), NULL);

  return g_object_ref (priv->file);
}

/**
 * foundry_vcs_blame_update:
 * @self: a [class@Foundry.VcsBlame]
 * @bytes: (nullable): data for the blame or %NULL to reset to file defaults
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves
 *   to any value or rejects with error
 */
DexFuture *
foundry_vcs_blame_update (FoundryVcsBlame *self,
                          GBytes          *bytes)
{
  dex_return_error_if_fail (FOUNDRY_IS_VCS_BLAME (self));

  if (FOUNDRY_VCS_BLAME_GET_CLASS (self)->update)
    return FOUNDRY_VCS_BLAME_GET_CLASS (self)->update (self, bytes);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not supported");
}
