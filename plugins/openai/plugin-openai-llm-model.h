/* plugin-openai-llm-model.h
 *
 * Copyright 2026 Christian Hergert
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

#include <foundry.h>

#include "plugin-openai-client.h"

G_BEGIN_DECLS

#define PLUGIN_TYPE_OPENAI_LLM_MODEL (plugin_openai_llm_model_get_type())

G_DECLARE_FINAL_TYPE (PluginOpenaiLlmModel, plugin_openai_llm_model, PLUGIN, OPENAI_LLM_MODEL, FoundryLlmModel)

PluginOpenaiLlmModel *plugin_openai_llm_model_new (FoundryContext     *context,
                                                   PluginOpenaiClient *client,
                                                   JsonNode           *node);

G_END_DECLS
