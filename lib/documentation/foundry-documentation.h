/* foundry-documentation.h
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

#define FOUNDRY_TYPE_DOCUMENTATION (foundry_documentation_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (FoundryDocumentation, foundry_documentation, FOUNDRY, DOCUMENTATION, GObject)

struct _FoundryDocumentationClass
{
  GObjectClass parent_instance;

  char      *(*dup_uri)     (FoundryDocumentation *self);
  char      *(*dup_title)   (FoundryDocumentation *self);
  DexFuture *(*find_parent) (FoundryDocumentation *self);

  /*< private >*/
  gpointer _reserved[8];
};

FOUNDRY_AVAILABLE_IN_ALL
DexFuture *foundry_documentation_find_parent (FoundryDocumentation *self);
FOUNDRY_AVAILABLE_IN_ALL
char      *foundry_documentation_dup_uri     (FoundryDocumentation *self);
FOUNDRY_AVAILABLE_IN_ALL
char      *foundry_documentation_dup_title   (FoundryDocumentation *self);

G_END_DECLS
