/* foundry-forge.c
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

#include "foundry-forge-private.h"
#include "foundry-util.h"

typedef struct
{
  PeasPluginInfo *plugin_info;
} FoundryForgePrivate;

enum {
  PROP_0,
  PROP_ID,
  PROP_PLUGIN_INFO,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryForge, foundry_forge, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static char *
foundry_forge_dup_id (FoundryForge *self)
{
  FoundryForgePrivate *priv = foundry_forge_get_instance_private (self);

  g_assert (FOUNDRY_IS_FORGE (self));

  if (priv->plugin_info == NULL)
    return NULL;

  return g_strdup (peas_plugin_info_get_module_name (priv->plugin_info));
}

static void
foundry_forge_finalize (GObject *object)
{
  FoundryForge *self = (FoundryForge *)object;
  FoundryForgePrivate *priv = foundry_forge_get_instance_private (self);

  g_clear_object (&priv->plugin_info);

  G_OBJECT_CLASS (foundry_forge_parent_class)->finalize (object);
}

static void
foundry_forge_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  FoundryForge *self = FOUNDRY_FORGE (object);
  FoundryForgePrivate *priv = foundry_forge_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_take_string (value, foundry_forge_dup_id (self));
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_object (value, priv->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_forge_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  FoundryForge *self = FOUNDRY_FORGE (object);
  FoundryForgePrivate *priv = foundry_forge_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_forge_class_init (FoundryForgeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_forge_finalize;
  object_class->get_property = foundry_forge_get_property;
  object_class->set_property = foundry_forge_set_property;

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_forge_init (FoundryForge *self)
{
}

/**
 * foundry_forge_dup_plugin_info:
 * @self: a [class@Foundry.Forge]
 *
 * Returns: (transfer full) (nullable):
 */
PeasPluginInfo *
foundry_forge_dup_plugin_info (FoundryForge *self)
{
  FoundryForgePrivate *priv = foundry_forge_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_FORGE (self), NULL);

  return priv->plugin_info ? g_object_ref (priv->plugin_info) : NULL;
}

DexFuture *
_foundry_forge_load (FoundryForge *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_FORGE (self));

  if (FOUNDRY_FORGE_GET_CLASS (self)->load)
    return FOUNDRY_FORGE_GET_CLASS (self)->load (self);

  return dex_future_new_true ();
}

DexFuture *
_foundry_forge_unload (FoundryForge *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_FORGE (self));

  if (FOUNDRY_FORGE_GET_CLASS (self)->unload)
    return FOUNDRY_FORGE_GET_CLASS (self)->unload (self);

  return dex_future_new_true ();
}

/**
 * foundry_forge_find_user:
 * @self: a [class@Foundry.Forge]
 *
 * Find the [class@Foundry.ForgeUser] that represents the current user.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.ForgeUser] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_forge_find_user (FoundryForge *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_FORGE (self));

  if (FOUNDRY_FORGE_GET_CLASS (self)->find_user)
    return FOUNDRY_FORGE_GET_CLASS (self)->find_user (self);

  return foundry_future_new_not_supported ();
}

/**
 * foundry_forge_find_project:
 * @self: a [class@Foundry.Forge]
 *
 * Find the [class@Foundry.ForgeProject] that represents the current project.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.ForgeProject] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_forge_find_project (FoundryForge *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_FORGE (self));

  if (FOUNDRY_FORGE_GET_CLASS (self)->find_project)
    return FOUNDRY_FORGE_GET_CLASS (self)->find_project (self);

  return foundry_future_new_not_supported ();
}
