/* foundry-config.c
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

#include "foundry-config-private.h"
#include "foundry-config-provider.h"

typedef struct _FoundryConfigPrivate
{
  GWeakRef provider_wr;
  char *name;
} FoundryConfigPrivate;

enum {
  PROP_0,
  PROP_NAME,
  PROP_PROVIDER,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryConfig, foundry_config, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static void
foundry_config_finalize (GObject *object)
{
  FoundryConfig *self = (FoundryConfig *)object;
  FoundryConfigPrivate *priv = foundry_config_get_instance_private (self);

  g_weak_ref_clear (&priv->provider_wr);

  g_clear_pointer (&priv->name, g_free);

  G_OBJECT_CLASS (foundry_config_parent_class)->finalize (object);
}

static void
foundry_config_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  FoundryConfig *self = FOUNDRY_CONFIG (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_take_string (value, foundry_config_dup_name (self));
      break;

    case PROP_PROVIDER:
      g_value_take_object (value, _foundry_config_dup_provider (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_config_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  FoundryConfig *self = FOUNDRY_CONFIG (object);

  switch (prop_id)
    {
    case PROP_NAME:
      foundry_config_set_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_config_class_init (FoundryConfigClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_config_finalize;
  object_class->get_property = foundry_config_get_property;
  object_class->set_property = foundry_config_set_property;

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PROVIDER] =
    g_param_spec_object ("provider", NULL, NULL,
                         FOUNDRY_TYPE_CONFIG_PROVIDER,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_config_init (FoundryConfig *self)
{
  FoundryConfigPrivate *priv = foundry_config_get_instance_private (self);

  g_weak_ref_init (&priv->provider_wr, NULL);
}

/**
 * foundry_config_dup_name:
 * @self: a #FoundryConfig
 *
 * Gets the user-visible name for the configuration.
 *
 * Returns: (transfer full): a newly allocated string
 */
char *
foundry_config_dup_name (FoundryConfig *self)
{
  FoundryConfigPrivate *priv = foundry_config_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_CONFIG (self), NULL);

  return g_strdup (priv->name);
}

/**
 * foundry_config_set_name:
 * @self: a #FoundryConfig
 *
 * Set the user-visible name of the config.
 *
 * This should only be called by implementations of #FoundryConfigProvider.
 */
void
foundry_config_set_name (FoundryConfig *self,
                         const char *name)
{
  FoundryConfigPrivate *priv = foundry_config_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_CONFIG (self));

  if (g_set_str (&priv->name, name))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
}

FoundryConfigProvider *
_foundry_config_dup_provider (FoundryConfig *self)
{
  FoundryConfigPrivate *priv = foundry_config_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_CONFIG (self), NULL);

  return g_weak_ref_get (&priv->provider_wr);
}

void
_foundry_config_set_provider (FoundryConfig         *self,
                              FoundryConfigProvider *provider)
{
  FoundryConfigPrivate *priv = foundry_config_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_CONFIG (self));
  g_return_if_fail (!provider || FOUNDRY_IS_CONFIG_PROVIDER (provider));

  g_weak_ref_set (&priv->provider_wr, provider);
}
