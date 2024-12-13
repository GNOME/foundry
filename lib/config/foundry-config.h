/* foundry-config.h
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

#define FOUNDRY_TYPE_CONFIG (foundry_config_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (FoundryConfig, foundry_config, FOUNDRY, CONFIG, FoundryContextual)

struct _FoundryConfigClass
{
  FoundryContextualClass parent_class;

  FoundrySdk *(*dup_sdk)     (FoundryConfig *self);
  DexFuture  *(*resolve_sdk) (FoundryConfig *self,
                              FoundryDevice *device);

  /*< private >*/
  gpointer _reserved[8];
};

FOUNDRY_AVAILABLE_IN_ALL
gboolean    foundry_config_get_active  (FoundryConfig *self);
FOUNDRY_AVAILABLE_IN_ALL
char       *foundry_config_dup_id      (FoundryConfig *self);
FOUNDRY_AVAILABLE_IN_ALL
void        foundry_config_set_id      (FoundryConfig *self,
                                        const char    *id);
FOUNDRY_AVAILABLE_IN_ALL
char       *foundry_config_dup_name    (FoundryConfig *self);
FOUNDRY_AVAILABLE_IN_ALL
void        foundry_config_set_name    (FoundryConfig *self,
                                        const char    *name);
FOUNDRY_AVAILABLE_IN_ALL
FoundrySdk *foundry_config_dup_sdk     (FoundryConfig *self);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture  *foundry_config_resolve_sdk (FoundryConfig *self,
                                        FoundryDevice *device);

G_END_DECLS
