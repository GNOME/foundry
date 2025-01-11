/* plugin-deviced-device.c
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

#include "plugin-deviced-device.h"

struct _PluginDevicedDevice
{
  FoundryDevice  parent_instance;
  DevdDevice    *device;
};

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PluginDevicedDevice, plugin_deviced_device, FOUNDRY_TYPE_DEVICE)

static GParamSpec *properties[N_PROPS];

static void
plugin_deviced_device_constructed (GObject *object)
{
  PluginDevicedDevice *self = (PluginDevicedDevice *)object;
  DevdDeviceKind kind;
  g_autofree char *id = NULL;
  const char *name;

  G_OBJECT_CLASS (plugin_deviced_device_parent_class)->constructed (object);

  if (self->device == NULL)
    return;

  id = g_strdup_printf ("deviced:%s", devd_device_get_id (self->device));
  name = devd_device_get_name (self->device);
  kind = devd_device_get_kind (self->device);

  foundry_device_set_id (FOUNDRY_DEVICE (self), id);
  foundry_device_set_name (FOUNDRY_DEVICE (self), name);

  switch (kind)
    {
    case DEVD_DEVICE_KIND_TABLET:
      foundry_device_set_chassis (FOUNDRY_DEVICE (self),
                                  FOUNDRY_DEVICE_CHASSIS_TABLET);
      break;

    case DEVD_DEVICE_KIND_MICRO_CONTROLLER:
      foundry_device_set_chassis (FOUNDRY_DEVICE (self),
                                  FOUNDRY_DEVICE_CHASSIS_OTHER);
      break;

    case DEVD_DEVICE_KIND_PHONE:
      foundry_device_set_chassis (FOUNDRY_DEVICE (self),
                                  FOUNDRY_DEVICE_CHASSIS_HANDSET);
      break;

    case DEVD_DEVICE_KIND_COMPUTER:
    default:
      foundry_device_set_chassis (FOUNDRY_DEVICE (self),
                                  FOUNDRY_DEVICE_CHASSIS_WORKSTATION);
      break;
    }
}

static void
plugin_deviced_device_finalize (GObject *object)
{
  PluginDevicedDevice *self = (PluginDevicedDevice *)object;

  g_clear_object (&self->device);

  G_OBJECT_CLASS (plugin_deviced_device_parent_class)->finalize (object);
}

static void
plugin_deviced_device_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  PluginDevicedDevice *self = PLUGIN_DEVICED_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_take_object (value, plugin_deviced_device_dup_device (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_deviced_device_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  PluginDevicedDevice *self = PLUGIN_DEVICED_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_deviced_device_class_init (PluginDevicedDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = plugin_deviced_device_constructed;
  object_class->finalize = plugin_deviced_device_finalize;
  object_class->get_property = plugin_deviced_device_get_property;
  object_class->set_property = plugin_deviced_device_set_property;

  properties[PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         DEVD_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_deviced_device_init (PluginDevicedDevice *self)
{
}

DevdDevice *
plugin_deviced_device_dup_device (PluginDevicedDevice *self)
{
  g_return_val_if_fail (PLUGIN_IS_DEVICED_DEVICE (self), NULL);

  return g_object_ref (self->device);
}
