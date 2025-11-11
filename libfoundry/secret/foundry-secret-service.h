/* foundry-secret-service.h
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

#include <gio/gio.h>

#include "foundry-service.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_SECRET_SERVICE (foundry_secret_service_get_type())

FOUNDRY_AVAILABLE_IN_1_1
FOUNDRY_DECLARE_INTERNAL_TYPE (FoundrySecretService, foundry_secret_service, FOUNDRY, SECRET_SERVICE, FoundryService)

FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_secret_service_store_api_key  (FoundrySecretService *self,
                                                  const char           *host,
                                                  const char           *service,
                                                  const char           *api_key);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_secret_service_lookup_api_key (FoundrySecretService *self,
                                                  const char           *host,
                                                  const char           *service);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_secret_service_delete_api_key (FoundrySecretService *self,
                                                  const char           *host,
                                                  const char           *service);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_secret_service_rotate_api_key (FoundrySecretService *self,
                                                  const char           *host,
                                                  const char           *service);

G_END_DECLS
