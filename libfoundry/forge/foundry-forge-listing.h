/* foundry-forge-listing.h
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

#include <libdex.h>

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_FORGE_LISTING (foundry_forge_listing_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryForgeListing, foundry_forge_listing, FOUNDRY, FORGE_LISTING, GObject)

struct _FoundryForgeListingClass
{
  GObjectClass parent_class;

  guint      (*get_n_pages)   (FoundryForgeListing *self);
  guint      (*get_page_size) (FoundryForgeListing *self);
  DexFuture *(*load_page)     (FoundryForgeListing *self,
                               guint                page);

  /*< private >*/
  gpointer _reserved[12];
};

FOUNDRY_AVAILABLE_IN_1_1
guint      foundry_forge_listing_get_n_pages   (FoundryForgeListing *self);
FOUNDRY_AVAILABLE_IN_1_1
guint      foundry_forge_listing_get_page_size (FoundryForgeListing *self);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_forge_listing_load_page     (FoundryForgeListing *self,
                                                guint                page);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_forge_listing_load_all      (FoundryForgeListing *self);
FOUNDRY_AVAILABLE_IN_1_1
gboolean   foundry_forge_listing_get_auto_load (FoundryForgeListing *self);
FOUNDRY_AVAILABLE_IN_1_1
void       foundry_forge_listing_set_auto_load (FoundryForgeListing *self,
                                                gboolean             auto_load);

G_END_DECLS
