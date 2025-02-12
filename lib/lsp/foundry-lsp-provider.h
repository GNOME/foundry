/* foundry-lsp-provider.h
 *
 * Copyright 2024-2025 Christian Hergert <chergert@redhat.com>
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

#include <libpeas.h>

#include "foundry-contextual.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_LSP_PROVIDER (foundry_lsp_provider_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (FoundryLspProvider, foundry_lsp_provider, FOUNDRY, LSP_PROVIDER, FoundryContextual)

struct _FoundryLspProviderClass
{
  FoundryContextualClass parent_class;

  DexFuture *(*load)                       (FoundryLspProvider *self);
  DexFuture *(*unload)                     (FoundryLspProvider *self);
  GVariant  *(*dup_initialization_options) (FoundryLspProvider *self);

  /*< private >*/
  gpointer _reserved[8];
};

FOUNDRY_AVAILABLE_IN_ALL
void            foundry_lsp_provider_add                        (FoundryLspProvider *self,
                                                                 FoundryLspServer   *server);
FOUNDRY_AVAILABLE_IN_ALL
void            foundry_lsp_provider_remove                     (FoundryLspProvider *self,
                                                                 FoundryLspServer   *server);
FOUNDRY_AVAILABLE_IN_ALL
PeasPluginInfo *foundry_lsp_provider_dup_plugin_info            (FoundryLspProvider *self);
FOUNDRY_AVAILABLE_IN_ALL
GVariant       *foundry_lsp_provider_dup_initialization_options (FoundryLspProvider *self);

G_END_DECLS
