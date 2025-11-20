/* foundry-llm-resource.h
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

#define FOUNDRY_TYPE_LLM_RESOURCE (foundry_llm_resource_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_DERIVABLE_TYPE (FoundryLlmResource, foundry_llm_resource, FOUNDRY, LLM_RESOURCE, GObject)

struct _FoundryLlmResourceClass
{
  GObjectClass parent_class;

  char      *(*dup_uri)          (FoundryLlmResource *self);
  char      *(*dup_name)         (FoundryLlmResource *self);
  char      *(*dup_description)  (FoundryLlmResource *self);
  char      *(*dup_content_type) (FoundryLlmResource *self);
  DexFuture *(*load_bytes)       (FoundryLlmResource *self);

  /*< private >*/
  gpointer _reserved[12];
};

FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_llm_resource_dup_uri          (FoundryLlmResource *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_llm_resource_dup_name         (FoundryLlmResource *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_llm_resource_dup_description  (FoundryLlmResource *self);
FOUNDRY_AVAILABLE_IN_1_1
char      *foundry_llm_resource_dup_content_type (FoundryLlmResource *self);
FOUNDRY_AVAILABLE_IN_1_1
DexFuture *foundry_llm_resource_load_bytes       (FoundryLlmResource *self);

G_END_DECLS
