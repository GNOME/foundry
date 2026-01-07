/* plugin-openai-llm-conversation.h
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

#define PLUGIN_TYPE_OPENAI_LLM_CONVERSATION (plugin_openai_llm_conversation_get_type())

G_DECLARE_FINAL_TYPE (PluginOpenaiLlmConversation, plugin_openai_llm_conversation, PLUGIN, OPENAI_LLM_CONVERSATION, FoundryLlmConversation)

FoundryLlmConversation *plugin_openai_llm_conversation_new (PluginOpenaiClient *client,
                                                            const char         *model,
                                                            const char         *system);

G_END_DECLS
