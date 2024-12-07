/* foundry-device.h
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

#pragma once

#include "foundry-contextual.h"
#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_DEVICE (foundry_device_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (FoundryDevice, foundry_device, FOUNDRY, DEVICE, FoundryContextual)

struct _FoundryDeviceClass
{
  FoundryContextualClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

FOUNDRY_AVAILABLE_IN_ALL
gboolean              foundry_device_get_active   (FoundryDevice        *self);
FOUNDRY_AVAILABLE_IN_ALL
char                 *foundry_device_dup_id       (FoundryDevice        *self);
FOUNDRY_AVAILABLE_IN_ALL
void                  foundry_device_set_id       (FoundryDevice        *self,
                                                   const char           *id);
FOUNDRY_AVAILABLE_IN_ALL
char                 *foundry_device_dup_name     (FoundryDevice        *self);
FOUNDRY_AVAILABLE_IN_ALL
void                  foundry_device_set_name     (FoundryDevice        *self,
                                                   const char           *name);
FOUNDRY_AVAILABLE_IN_ALL
FoundryDeviceChassis  foundry_device_get_chassis  (FoundryDevice        *self);
FOUNDRY_AVAILABLE_IN_ALL
void                  foundry_device_set_chassis  (FoundryDevice        *self,
                                                   FoundryDeviceChassis  chassis);
FOUNDRY_AVAILABLE_IN_ALL
FoundryTriplet       *foundry_device_dup_triplet  (FoundryDevice        *self);
FOUNDRY_AVAILABLE_IN_ALL
void                  foundry_device_set_triplet  (FoundryDevice        *self,
                                                   FoundryTriplet       *triplet);

G_END_DECLS
