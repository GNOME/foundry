/* foundry-menu-search-provider.h
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

#include "foundry-search-provider.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_MENU_SEARCH_PROVIDER (foundry_menu_search_provider_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryMenuSearchProvider, foundry_menu_search_provider, FOUNDRY, MENU_SEARCH_PROVIDER, FoundrySearchProvider)

struct _FoundryMenuSearchProviderClass
{
  FoundrySearchProviderClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

FOUNDRY_AVAILABLE_IN_1_1
FoundryMenuSearchProvider *foundry_menu_search_provider_new      (void);
FOUNDRY_AVAILABLE_IN_1_1
void                       foundry_menu_search_provider_add_menu (FoundryMenuSearchProvider *self,
                                                                  GMenuModel                *menu_model);

G_END_DECLS
