/* foundry-file-search-provider.h
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <libpeas.h>

#include "foundry-contextual.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_FILE_SEARCH_PROVIDER (foundry_file_search_provider_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryFileSearchProvider, foundry_file_search_provider, FOUNDRY, FILE_SEARCH_PROVIDER, FoundryContextual)

struct _FoundryFileSearchProviderClass
{
  FoundryContextualClass parent_class;

  DexFuture *(*search) (FoundryFileSearchProvider *self,
                        FoundryFileSearchOptions  *options,
                        FoundryOperation          *operation);

  gpointer _reserved[7];
};

FOUNDRY_AVAILABLE_IN_1_1
PeasPluginInfo *foundry_file_search_provider_dup_plugin_info (FoundryFileSearchProvider *self);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture      *foundry_file_search_provider_search          (FoundryFileSearchProvider *self,
                                                              FoundryFileSearchOptions  *options,
                                                              FoundryOperation          *operation);

G_END_DECLS
