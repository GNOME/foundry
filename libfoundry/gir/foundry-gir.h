/* foundry-gir.h
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

#include "foundry-gir-node.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_GIR (foundry_gir_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryGir, foundry_gir, FOUNDRY, GIR, GObject)

typedef enum _FoundryGirError
{
  FOUNDRY_GIR_ERROR_FAILED,
  FOUNDRY_GIR_ERROR_PARSE,
} FoundryGirError;

#define FOUNDRY_GIR_ERROR (foundry_gir_error_quark())

FOUNDRY_AVAILABLE_IN_1_1
GQuark           foundry_gir_error_quark     (void);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture       *foundry_gir_new             (GFile       *file) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_1
DexFuture       *foundry_gir_new_for_path    (const char  *path) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_1
GFile           *foundry_gir_get_file        (FoundryGir  *gir);
FOUNDRY_AVAILABLE_IN_1_1
FoundryGirNode  *foundry_gir_get_repository  (FoundryGir  *gir);
FOUNDRY_AVAILABLE_IN_1_1
FoundryGirNode  *foundry_gir_get_namespace   (FoundryGir  *gir,
                                              const char  *namespace_name);
FOUNDRY_AVAILABLE_IN_1_1
FoundryGirNode **foundry_gir_list_namespaces (FoundryGir  *gir,
                                              guint       *n_namespaces);

G_END_DECLS
