/*
 * foundry-local-device.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include "foundry-local-device.h"
#include "foundry-triplet.h"

struct _FoundryLocalDevice
{
  FoundryDevice parent_instance;
};

G_DEFINE_FINAL_TYPE (FoundryLocalDevice, foundry_local_device, FOUNDRY_TYPE_DEVICE)

static void
foundry_local_device_class_init (FoundryLocalDeviceClass *klass)
{
}

static void
foundry_local_device_init (FoundryLocalDevice *self)
{
}

FoundryDevice *
foundry_local_device_new (FoundryContext *context)
{
  g_autoptr(FoundryTriplet) system = NULL;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);

  system = foundry_triplet_new_from_system ();

  return g_object_new (FOUNDRY_TYPE_LOCAL_DEVICE,
                       "id", "host",
                       "context", context,
                       "triplet", system,
                       "name", _("My Computer"),
                       NULL);
}
