/* foundry-forge-listing-page-private.h
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

G_BEGIN_DECLS

#define FOUNDRY_TYPE_FORGE_LISTING_PAGE (foundry_forge_listing_page_get_type())

G_DECLARE_FINAL_TYPE (FoundryForgeListingPage, foundry_forge_listing_page, FOUNDRY, FORGE_LISTING_PAGE, GObject)

FoundryForgeListingPage *foundry_forge_listing_page_new       (DexFuture               *future,
                                                               guint                    page);
guint                    foundry_forge_listing_page_get_page  (FoundryForgeListingPage *self);
GListModel              *foundry_forge_listing_page_get_model (FoundryForgeListingPage *self);
DexFuture               *foundry_forge_listing_page_await     (FoundryForgeListingPage *self) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
