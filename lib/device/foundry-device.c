/* foundry-device.c
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

#include "foundry-device-chassis.h"
#include "foundry-device-manager.h"
#include "foundry-device-private.h"
#include "foundry-device-provider.h"
#include "foundry-triplet.h"

typedef struct _FoundryDevicePrivate
{
  GWeakRef              provider_wr;
  char                 *id;
  char                 *name;
  FoundryTriplet       *triplet;
  FoundryDeviceChassis  chassis;
} FoundryDevicePrivate;

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_CHASSIS,
  PROP_ID,
  PROP_NAME,
  PROP_PROVIDER,
  PROP_TRIPLET,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryDevice, foundry_device, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static void
foundry_device_finalize (GObject *object)
{
  FoundryDevice *self = (FoundryDevice *)object;
  FoundryDevicePrivate *priv = foundry_device_get_instance_private (self);

  g_weak_ref_clear (&priv->provider_wr);

  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->triplet, foundry_triplet_unref);

  G_OBJECT_CLASS (foundry_device_parent_class)->finalize (object);
}

static void
foundry_device_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  FoundryDevice *self = FOUNDRY_DEVICE (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, foundry_device_get_active (self));
      break;

    case PROP_CHASSIS:
      g_value_set_enum (value, foundry_device_get_chassis (self));
      break;

    case PROP_ID:
      g_value_take_string (value, foundry_device_dup_id (self));
      break;

    case PROP_NAME:
      g_value_take_string (value, foundry_device_dup_name (self));
      break;

    case PROP_PROVIDER:
      g_value_take_object (value, _foundry_device_dup_provider (self));
      break;

    case PROP_TRIPLET:
      g_value_take_boxed (value, foundry_device_dup_triplet (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_device_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  FoundryDevice *self = FOUNDRY_DEVICE (object);

  switch (prop_id)
    {
    case PROP_CHASSIS:
      foundry_device_set_chassis (self, g_value_get_enum (value));
      break;

    case PROP_ID:
      foundry_device_set_id (self, g_value_get_string (value));
      break;

    case PROP_NAME:
      foundry_device_set_name (self, g_value_get_string (value));
      break;

    case PROP_TRIPLET:
      foundry_device_set_triplet (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_device_class_init (FoundryDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_device_finalize;
  object_class->get_property = foundry_device_get_property;
  object_class->set_property = foundry_device_set_property;

  properties[PROP_ACTIVE] =
    g_param_spec_boolean ("active", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_CHASSIS] =
    g_param_spec_enum ("chassis", NULL, NULL,
                       FOUNDRY_TYPE_DEVICE_CHASSIS,
                       FOUNDRY_DEVICE_CHASSIS_WORKSTATION,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
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
                         FOUNDRY_TYPE_DEVICE_PROVIDER,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TRIPLET] =
    g_param_spec_boxed ("triplet", NULL, NULL,
                        FOUNDRY_TYPE_TRIPLET,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_device_init (FoundryDevice *self)
{
  FoundryDevicePrivate *priv = foundry_device_get_instance_private (self);

  g_weak_ref_init (&priv->provider_wr, NULL);

  priv->chassis = FOUNDRY_DEVICE_CHASSIS_WORKSTATION;
}

/**
 * foundry_device_dup_id:
 * @self: a #FoundryDevice
 *
 * Gets the user-visible id for the device.
 *
 * Returns: (transfer full): a newly allocated string
 */
char *
foundry_device_dup_id (FoundryDevice *self)
{
  FoundryDevicePrivate *priv = foundry_device_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_DEVICE (self), NULL);

  return g_strdup (priv->id);
}

/**
 * foundry_device_set_id:
 * @self: a #FoundryDevice
 *
 * Set the user-visible id of the device.
 *
 * This should only be called by implementations of #FoundryDeviceProvider.
 */
void
foundry_device_set_id (FoundryDevice *self,
                       const char    *id)
{
  FoundryDevicePrivate *priv = foundry_device_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_DEVICE (self));

  if (g_set_str (&priv->id, id))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ID]);
}

/**
 * foundry_device_dup_name:
 * @self: a #FoundryDevice
 *
 * Gets the user-visible name for the SDK.
 *
 * Returns: (transfer full): a newly allocated string
 */
char *
foundry_device_dup_name (FoundryDevice *self)
{
  FoundryDevicePrivate *priv = foundry_device_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_DEVICE (self), NULL);

  return g_strdup (priv->name);
}

/**
 * foundry_device_set_name:
 * @self: a #FoundryDevice
 *
 * Set the user-visible name of the sdk.
 *
 * This should only be called by implementations of #FoundryDeviceProvider.
 */
void
foundry_device_set_name (FoundryDevice *self,
                         const char    *name)
{
  FoundryDevicePrivate *priv = foundry_device_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_DEVICE (self));

  if (g_set_str (&priv->name, name))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
}

FoundryDeviceProvider *
_foundry_device_dup_provider (FoundryDevice *self)
{
  FoundryDevicePrivate *priv = foundry_device_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_DEVICE (self), NULL);

  return g_weak_ref_get (&priv->provider_wr);
}

void
_foundry_device_set_provider (FoundryDevice         *self,
                              FoundryDeviceProvider *provider)
{
  FoundryDevicePrivate *priv = foundry_device_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_DEVICE (self));
  g_return_if_fail (!provider || FOUNDRY_IS_DEVICE_PROVIDER (provider));

  g_weak_ref_set (&priv->provider_wr, provider);
}

/**
 * foundry_device_get_chassis:
 *
 * Gets the chassis type for the device.
 *
 * Returns: a #FoundryDeviceChassis
 */
FoundryDeviceChassis
foundry_device_get_chassis (FoundryDevice *self)
{
  FoundryDevicePrivate *priv = foundry_device_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_DEVICE (self), 0);

  return priv->chassis;
}

/**
 * foundry_device_set_chassis:
 *
 * Set the chassis device type.
 *
 * This should only be called by implementations of #FoundryDeviceProvider.
 */
void
foundry_device_set_chassis (FoundryDevice        *self,
                            FoundryDeviceChassis  chassis)
{
  FoundryDevicePrivate *priv = foundry_device_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_DEVICE (self));
  g_return_if_fail ((int)chassis >= 0);
  g_return_if_fail (chassis < FOUNDRY_DEVICE_CHASSIS_LAST);

  if (priv->chassis != chassis)
    {
      priv->chassis = chassis;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHASSIS]);
    }
}

/**
 * foundry_device_dup_triplet:
 *
 * Get the triplet for the device.
 *
 * Returns: (transfer full): a #FoundryDevice
 */
FoundryTriplet *
foundry_device_dup_triplet (FoundryDevice *self)
{
  FoundryDevicePrivate *priv = foundry_device_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_DEVICE (self), NULL);

  if (priv->triplet)
    return foundry_triplet_ref (priv->triplet);

  return NULL;
}

/**
 * foundry_device_set_triplet:
 * @self: a #FoundryDevice
 * @triplet: a #FoundryTriplet
 *
 * Sets the triplet which matches the device system.
 *
 * This should only be called by implementations of #FoundryDeviceProvider.
 */
void
foundry_device_set_triplet (FoundryDevice  *self,
                            FoundryTriplet *triplet)
{
  FoundryDevicePrivate *priv = foundry_device_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_DEVICE (self));
  g_return_if_fail (triplet != NULL);

  if (priv->triplet == triplet)
    return;

  foundry_triplet_ref (triplet);
  g_clear_pointer (&priv->triplet, foundry_triplet_unref);
  priv->triplet = triplet;
}

gboolean
foundry_device_get_active (FoundryDevice *self)
{
  g_autoptr(FoundryContext) context = NULL;

  g_return_val_if_fail (FOUNDRY_IS_DEVICE (self), FALSE);

  if ((context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))))
    {
      g_autoptr(FoundryDeviceManager) device_manager = foundry_context_dup_device_manager (context);
      g_autoptr(FoundryDevice) device = foundry_device_manager_dup_device (device_manager);

      return device == self;
    }

  return FALSE;
}
