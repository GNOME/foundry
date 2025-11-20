/* foundry-json-list-llm-resource.h
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

#include "foundry-llm-resource.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_JSON_LIST_LLM_RESOURCE (foundry_json_list_llm_resource_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryJsonListLlmResource, foundry_json_list_llm_resource, FOUNDRY, JSON_LIST_LLM_RESOURCE, FoundryLlmResource)

FOUNDRY_AVAILABLE_IN_1_1
FoundryLlmResource *foundry_json_list_llm_resource_new (const char *name,
                                                        const char *uri,
                                                        const char *description,
                                                        GListModel *model);

G_END_DECLS
