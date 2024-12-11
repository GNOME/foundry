/* foundry-sdk.c
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

#include "foundry-contextual-private.h"
#include "foundry-sdk-manager.h"
#include "foundry-sdk-private.h"
#include "foundry-sdk-provider.h"

typedef struct _FoundrySdkPrivate
{
  GWeakRef provider_wr;
  char *id;
  char *arch;
  char *name;
  char *kind;
  guint installed : 1;
} FoundrySdkPrivate;

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_ARCH,
  PROP_ID,
  PROP_INSTALLED,
  PROP_KIND,
  PROP_NAME,
  PROP_PROVIDER,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundrySdk, foundry_sdk, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static void
foundry_sdk_finalize (GObject *object)
{
  FoundrySdk *self = (FoundrySdk *)object;
  FoundrySdkPrivate *priv = foundry_sdk_get_instance_private (self);

  g_weak_ref_clear (&priv->provider_wr);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->kind, g_free);
  g_clear_pointer (&priv->arch, g_free);

  G_OBJECT_CLASS (foundry_sdk_parent_class)->finalize (object);
}

static void
foundry_sdk_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  FoundrySdk *self = FOUNDRY_SDK (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, foundry_sdk_get_active (self));
      break;

    case PROP_ARCH:
      g_value_take_string (value, foundry_sdk_dup_arch (self));
      break;

    case PROP_ID:
      g_value_take_string (value, foundry_sdk_dup_id (self));
      break;

    case PROP_INSTALLED:
      g_value_set_boolean (value, foundry_sdk_get_installed (self));
      break;

    case PROP_KIND:
      g_value_take_string (value, foundry_sdk_dup_kind (self));
      break;

    case PROP_NAME:
      g_value_take_string (value, foundry_sdk_dup_name (self));
      break;

    case PROP_PROVIDER:
      g_value_take_object (value, _foundry_sdk_dup_provider (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_sdk_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  FoundrySdk *self = FOUNDRY_SDK (object);

  switch (prop_id)
    {
    case PROP_ARCH:
      foundry_sdk_set_arch (self, g_value_get_string (value));
      break;

    case PROP_ID:
      foundry_sdk_set_id (self, g_value_get_string (value));
      break;

    case PROP_INSTALLED:
      foundry_sdk_set_installed (self, g_value_get_boolean (value));
      break;

    case PROP_KIND:
      foundry_sdk_set_kind (self, g_value_get_string (value));
      break;

    case PROP_NAME:
      foundry_sdk_set_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_sdk_class_init (FoundrySdkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_sdk_finalize;
  object_class->get_property = foundry_sdk_get_property;
  object_class->set_property = foundry_sdk_set_property;

  properties[PROP_ACTIVE] =
    g_param_spec_boolean ("active", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_ARCH] =
    g_param_spec_string ("arch", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_INSTALLED] =
    g_param_spec_boolean ("installed", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_KIND] =
    g_param_spec_string ("kind", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PROVIDER] =
    g_param_spec_object ("provider", NULL, NULL,
                         FOUNDRY_TYPE_SDK_PROVIDER,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_sdk_init (FoundrySdk *self)
{
  FoundrySdkPrivate *priv = foundry_sdk_get_instance_private (self);

  g_weak_ref_init (&priv->provider_wr, NULL);
}

/**
 * foundry_sdk_dup_id:
 * @self: a #FoundrySdk
 *
 * Gets the user-visible id for the SDK.
 *
 * Returns: (transfer full): a newly allocated string
 */
char *
foundry_sdk_dup_id (FoundrySdk *self)
{
  FoundrySdkPrivate *priv = foundry_sdk_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_SDK (self), NULL);

  return g_strdup (priv->id);
}

/**
 * foundry_sdk_set_id:
 * @self: a #FoundrySdk
 *
 * Set the unique id of the SDK.
 *
 * This should only be called by implementations of #FoundrySdkProvider.
 */
void
foundry_sdk_set_id (FoundrySdk *self,
                    const char *id)
{
  FoundrySdkPrivate *priv = foundry_sdk_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_SDK (self));

  if (g_set_str (&priv->id, id))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ID]);
}

/**
 * foundry_sdk_dup_arch:
 * @self: a #FoundrySdk
 *
 * Gets the architecture of the SDK.
 *
 * Returns: (transfer full): a newly allocated string
 */
char *
foundry_sdk_dup_arch (FoundrySdk *self)
{
  FoundrySdkPrivate *priv = foundry_sdk_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_SDK (self), NULL);

  return g_strdup (priv->arch);
}

/**
 * foundry_sdk_set_arch:
 * @self: a #FoundrySdk
 *
 * Set the architecture of the SDK.
 *
 * This should only be called by [class@Foundry.SdkProvider] classes.
 */
void
foundry_sdk_set_arch (FoundrySdk *self,
                      const char *arch)
{
  FoundrySdkPrivate *priv = foundry_sdk_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_SDK (self));

  if (g_set_str (&priv->arch, arch))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ARCH]);
}

/**
 * foundry_sdk_dup_kind:
 * @self: a #FoundrySdk
 *
 * Gets the user-visible kind for the SDK.
 *
 * Returns: (transfer full): a newly allocated string
 */
char *
foundry_sdk_dup_kind (FoundrySdk *self)
{
  FoundrySdkPrivate *priv = foundry_sdk_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_SDK (self), NULL);

  return g_strdup (priv->kind);
}

/**
 * foundry_sdk_set_kind:
 * @self: a #FoundrySdk
 *
 * Set the user-visible kind of the sdk.
 *
 * This should only be called by implementations of #FoundrySdkProvider.
 */
void
foundry_sdk_set_kind (FoundrySdk *self,
                      const char *kind)
{
  FoundrySdkPrivate *priv = foundry_sdk_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_SDK (self));

  if (g_set_str (&priv->kind, kind))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KIND]);
}

/**
 * foundry_sdk_dup_name:
 * @self: a #FoundrySdk
 *
 * Gets the user-visible name for the SDK.
 *
 * Returns: (transfer full): a newly allocated string
 */
char *
foundry_sdk_dup_name (FoundrySdk *self)
{
  FoundrySdkPrivate *priv = foundry_sdk_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_SDK (self), NULL);

  return g_strdup (priv->name);
}

/**
 * foundry_sdk_set_name:
 * @self: a #FoundrySdk
 *
 * Set the user-visible name of the sdk.
 *
 * This should only be called by implementations of #FoundrySdkProvider.
 */
void
foundry_sdk_set_name (FoundrySdk *self,
                      const char *name)
{
  FoundrySdkPrivate *priv = foundry_sdk_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_SDK (self));

  if (g_set_str (&priv->name, name))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
}

FoundrySdkProvider *
_foundry_sdk_dup_provider (FoundrySdk *self)
{
  FoundrySdkPrivate *priv = foundry_sdk_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_SDK (self), NULL);

  return g_weak_ref_get (&priv->provider_wr);
}

void
_foundry_sdk_set_provider (FoundrySdk         *self,
                           FoundrySdkProvider *provider)
{
  FoundrySdkPrivate *priv = foundry_sdk_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_SDK (self));
  g_return_if_fail (!provider || FOUNDRY_IS_SDK_PROVIDER (provider));

  g_weak_ref_set (&priv->provider_wr, provider);
}

gboolean
foundry_sdk_get_active (FoundrySdk *self)
{
  g_autoptr(FoundryContext) context = NULL;

  g_return_val_if_fail (FOUNDRY_IS_SDK (self), FALSE);

  if ((context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))))
    {
      g_autoptr(FoundrySdkManager) sdk_manager = foundry_context_dup_sdk_manager (context);
      g_autoptr(FoundrySdk) sdk = foundry_sdk_manager_dup_sdk (sdk_manager);

      return sdk == self;
    }

  return FALSE;
}

gboolean
foundry_sdk_get_installed (FoundrySdk *self)
{
  FoundrySdkPrivate *priv = foundry_sdk_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_SDK (self), FALSE);

  return priv->installed;
}

void
foundry_sdk_set_installed (FoundrySdk *self,
                           gboolean    installed)
{
  FoundrySdkPrivate *priv = foundry_sdk_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_SDK (self));

  installed = !!installed;

  if (priv->installed != installed)
    {
      priv->installed = installed;

      if (foundry_sdk_get_active (self))
        _foundry_contextual_invalidate_pipeline (FOUNDRY_CONTEXTUAL (self));

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INSTALLED]);
    }
}
