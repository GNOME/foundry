/* foundry-key-rotator.h
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

#pragma once

#include <libdex.h>

#include "foundry-contextual.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_KEY_ROTATOR (foundry_key_rotator_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryKeyRotator, foundry_key_rotator, FOUNDRY, KEY_ROTATOR, FoundryContextual)

struct _FoundryKeyRotatorClass
{
  FoundryContextualClass parent_class;

  DexFuture *(*check_expires_at) (FoundryKeyRotator *self,
                                  const char        *host,
                                  const char        *service_name,
                                  const char        *secret);
  gboolean   (*can_rotate)       (FoundryKeyRotator *self,
                                  const char        *host,
                                  const char        *service_name,
                                  const char        *secret);
  DexFuture *(*rotate)           (FoundryKeyRotator *self,
                                  const char        *host,
                                  const char        *service_name,
                                  const char        *secret);

  /*< private >*/
  gpointer _reserved[13];
};

FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_key_rotator_check_expires_at (FoundryKeyRotator *self,
                                                 const char        *host,
                                                 const char        *service_name,
                                                 const char        *secret);
FOUNDRY_AVAILABLE_IN_1_1
gboolean   foundry_key_rotator_can_rotate       (FoundryKeyRotator *self,
                                                 const char        *host,
                                                 const char        *service_name,
                                                 const char        *secret);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_key_rotator_rotate           (FoundryKeyRotator *self,
                                                 const char        *host,
                                                 const char        *service_name,
                                                 const char        *secret);

G_END_DECLS
