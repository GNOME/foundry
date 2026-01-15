/* foundry-doap-file.h
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

#include "foundry-doap-person.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_DOAP_FILE_ERROR (foundry_doap_file_error_quark())
#define FOUNDRY_TYPE_DOAP_FILE  (foundry_doap_file_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryDoapFile, foundry_doap_file, FOUNDRY, DOAP_FILE, GObject)

typedef enum
{
  FOUNDRY_DOAP_FILE_ERROR_INVALID_FORMAT = 1,
} FoundryDoapFileError;

FOUNDRY_AVAILABLE_IN_1_1
GQuark            foundry_doap_file_error_quark       (void);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture        *foundry_doap_file_new_from_file     (GFile           *file) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_1
DexFuture        *foundry_doap_file_new_from_bytes    (GBytes          *bytes) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_1
const char       *foundry_doap_file_get_name          (FoundryDoapFile *self);
FOUNDRY_AVAILABLE_IN_1_1
const char       *foundry_doap_file_get_shortdesc     (FoundryDoapFile *self);
FOUNDRY_AVAILABLE_IN_1_1
const char       *foundry_doap_file_get_description   (FoundryDoapFile *self);
FOUNDRY_AVAILABLE_IN_1_1
const char       *foundry_doap_file_get_bug_database  (FoundryDoapFile *self);
FOUNDRY_AVAILABLE_IN_1_1
const char       *foundry_doap_file_get_download_page (FoundryDoapFile *self);
FOUNDRY_AVAILABLE_IN_1_1
const char       *foundry_doap_file_get_homepage      (FoundryDoapFile *self);
FOUNDRY_AVAILABLE_IN_1_1
const char       *foundry_doap_file_get_category      (FoundryDoapFile *self);
FOUNDRY_AVAILABLE_IN_1_1
char            **foundry_doap_file_get_languages     (FoundryDoapFile *self);
FOUNDRY_AVAILABLE_IN_1_1
GList            *foundry_doap_file_get_maintainers   (FoundryDoapFile *self);

G_END_DECLS
