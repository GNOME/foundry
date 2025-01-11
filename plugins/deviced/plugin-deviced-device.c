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
#include "plugin-deviced-device-info.h"

struct _PluginDevicedDevice
{
  FoundryDevice  parent_instance;
  DevdDevice    *device;
  DexPromise    *client;
};

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PluginDevicedDevice, plugin_deviced_device, FOUNDRY_TYPE_DEVICE)

static GParamSpec *properties[N_PROPS];

static DexFuture *
plugin_deviced_device_load_info_cb (DexFuture *completed,
                                    gpointer   user_data)
{
  g_autoptr(DevdClient) client = dex_await_object (dex_ref (completed), NULL);
  PluginDevicedDevice *self = user_data;

  g_assert (DEVD_IS_CLIENT (client));
  g_assert (PLUGIN_IS_DEVICED_DEVICE (self));

  return plugin_deviced_device_info_new (self->device, client);
}

static DexFuture *
plugin_deviced_device_load_info (FoundryDevice *device)
{
  PluginDevicedDevice *self = (PluginDevicedDevice *)device;
  DexFuture *future;

  g_assert (PLUGIN_IS_DEVICED_DEVICE (self));

  future = plugin_deviced_device_load_client (self);
  future = dex_future_then (future,
                            plugin_deviced_device_load_info_cb,
                            g_object_ref (self),
                            g_object_unref);

  return future;
}

static char *
plugin_deviced_device_dup_id (FoundryDevice *device)
{
  PluginDevicedDevice *self = (PluginDevicedDevice *)device;
  g_autofree char *id = NULL;

  g_assert (PLUGIN_IS_DEVICED_DEVICE (self));

  g_object_get (self->device, "id", &id, NULL);

  return g_steal_pointer (&id);
}

static void
plugin_deviced_device_finalize (GObject *object)
{
  PluginDevicedDevice *self = (PluginDevicedDevice *)object;

  g_clear_object (&self->device);
  dex_clear (&self->client);

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
  FoundryDeviceClass *device_class = FOUNDRY_DEVICE_CLASS (klass);

  object_class->finalize = plugin_deviced_device_finalize;
  object_class->get_property = plugin_deviced_device_get_property;
  object_class->set_property = plugin_deviced_device_set_property;

  device_class->dup_id = plugin_deviced_device_dup_id;
  device_class->load_info = plugin_deviced_device_load_info;

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

static void
plugin_deviced_device_load_client_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  DevdClient *client = (DevdClient *)object;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DEVD_IS_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!devd_client_connect_finish (client, result, &error))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_object (promise, g_object_ref (client));
}

DexFuture *
plugin_deviced_device_load_client (PluginDevicedDevice *self)
{
  dex_return_error_if_fail (PLUGIN_IS_DEVICED_DEVICE (self));
  dex_return_error_if_fail (DEVD_IS_DEVICE (self->device));
  dex_return_error_if_fail (!self->client || DEX_IS_FUTURE (self->client));

  if (self->client == NULL)
    {
      g_autoptr(DevdClient) client = devd_device_create_client (self->device);

      self->client = dex_promise_new_cancellable ();

      devd_client_connect_async (client,
                                 dex_promise_get_cancellable (self->client),
                                 plugin_deviced_device_load_client_cb,
                                 dex_ref (self->client));
    }

  return dex_ref (DEX_FUTURE (self->client));
}
