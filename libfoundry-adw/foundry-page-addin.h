/* foundry-page-addin.h
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <foundry.h>

#include "foundry-page.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_PAGE_ADDIN (foundry_page_addin_get_type())

FOUNDRY_AVAILABLE_IN_1_3
G_DECLARE_DERIVABLE_TYPE (FoundryPageAddin, foundry_page_addin, FOUNDRY, PAGE_ADDIN, GObject)

struct _FoundryPageAddinClass
{
  GObjectClass parent_class;

  void (*load)   (FoundryPageAddin *self);
  void (*unload) (FoundryPageAddin *self);

  /*< private >*/
  gpointer _reserved[13];
};

FOUNDRY_AVAILABLE_IN_1_3
FoundryPage *foundry_page_addin_dup_page (FoundryPageAddin *self);

G_END_DECLS
