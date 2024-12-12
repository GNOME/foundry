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

#include "foundry-vcs-private.h"
#include "foundry-vcs-manager.h"

typedef struct _FoundryVcsPrivate
{
  GWeakRef provider_wr;
} FoundryVcsPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryVcs, foundry_vcs, FOUNDRY_TYPE_CONTEXTUAL)

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_ID,
  PROP_NAME,
  PROP_PROVIDER,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

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

    case PROP_ID:
      g_value_take_string (value, foundry_vcs_dup_id (self));
      break;

    case PROP_NAME:
      g_value_take_string (value, foundry_vcs_dup_name (self));
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

  properties[PROP_ACTIVE] =
    g_param_spec_boolean ("active", NULL, NULL,
                         FALSE,
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
