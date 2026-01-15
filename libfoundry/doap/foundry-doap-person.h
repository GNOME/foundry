/* foundry-doap-person.h
 *
 * Copyright 2015-2025 Christian Hergert <chergert@redhat.com>
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

#include <glib-object.h>

#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_DOAP_PERSON (foundry_doap_person_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryDoapPerson, foundry_doap_person, FOUNDRY, DOAP_PERSON, GObject)

FOUNDRY_AVAILABLE_IN_1_1
FoundryDoapPerson *foundry_doap_person_new       (void);
FOUNDRY_AVAILABLE_IN_1_1
const char        *foundry_doap_person_get_name  (FoundryDoapPerson *self);
FOUNDRY_AVAILABLE_IN_1_1
void               foundry_doap_person_set_name  (FoundryDoapPerson *self,
                                                  const char        *name);
FOUNDRY_AVAILABLE_IN_1_1
const char        *foundry_doap_person_get_email (FoundryDoapPerson *self);
FOUNDRY_AVAILABLE_IN_1_1
void               foundry_doap_person_set_email (FoundryDoapPerson *self,
                                                  const char        *email);

G_END_DECLS
