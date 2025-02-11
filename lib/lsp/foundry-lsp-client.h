/* foundry-lsp-client.h
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

#include "foundry-service.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_LSP_CLIENT (foundry_lsp_client_get_type())

FOUNDRY_AVAILABLE_IN_ALL
FOUNDRY_DECLARE_INTERNAL_TYPE (FoundryLspClient, foundry_lsp_client, FOUNDRY, LSP_CLIENT, FoundryContextual)

FOUNDRY_AVAILABLE_IN_ALL
FoundryLspClient *foundry_lsp_client_new         (FoundryContext   *context,
                                                  GIOStream        *stream);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture *foundry_lsp_client_query_capabilities (FoundryLspClient *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_ALL
DexFuture *foundry_lsp_client_call               (FoundryLspClient *self,
                                                  const char       *method,
                                                  GVariant         *params) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_ALL
DexFuture *foundry_lsp_client_notify             (FoundryLspClient *self,
                                                  const char       *method,
                                                  GVariant         *params) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
