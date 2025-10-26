/* foundry-intent.h
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

#include "foundry-contextual.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_INTENT (foundry_intent_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryIntent, foundry_intent, FOUNDRY, INTENT, FoundryContextual)

struct _FoundryIntentClass
{
  FoundryContextualClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

FOUNDRY_AVAILABLE_IN_1_1
gboolean      foundry_intent_has_attribute         (FoundryIntent *self,
                                                    const char    *attribute);
FOUNDRY_AVAILABLE_IN_1_1
void          foundry_intent_set_attribute_value   (FoundryIntent *self,
                                                    const char    *attribute,
                                                    const GValue  *value);
FOUNDRY_AVAILABLE_IN_1_1
GType         foundry_intent_get_attribute_type    (FoundryIntent *self,
                                                    const char    *attribute);
FOUNDRY_AVAILABLE_IN_1_1
const GValue *foundry_intent_get_attribute_value   (FoundryIntent *self,
                                                    const char    *attribute);
FOUNDRY_AVAILABLE_IN_1_1
char         *foundry_intent_dup_attribute_string  (FoundryIntent *self,
                                                    const char    *attribute);
FOUNDRY_AVAILABLE_IN_1_1
gboolean      foundry_intent_get_attribute_boolean (FoundryIntent *self,
                                                    const char    *attribute);
FOUNDRY_AVAILABLE_IN_1_1
gpointer      foundry_intent_dup_attribute_object  (FoundryIntent *self,
                                                    const char    *attribute);
FOUNDRY_AVAILABLE_IN_1_1
void          foundry_intent_set_attribute         (FoundryIntent *self,
                                                    const char    *attribute,
                                                    GType          type,
                                                    ...);

G_END_DECLS
