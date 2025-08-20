/* plugin-ollama-llm-tool-call.c
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

#include "config.h"

#include "plugin-ollama-llm-tool-call.h"

struct _PluginOllamaLlmToolCall
{
  FoundryLlmToolCall parent_instance;
  FoundryLlmTool *tool;
  JsonNode *arguments;
  guint is_callable : 1;
};

G_DEFINE_FINAL_TYPE (PluginOllamaLlmToolCall, plugin_ollama_llm_tool_call, FOUNDRY_TYPE_LLM_TOOL_CALL)

static char *
plugin_ollama_llm_tool_call_dup_title (FoundryLlmToolCall *tool_call)
{
  PluginOllamaLlmToolCall *self = PLUGIN_OLLAMA_LLM_TOOL_CALL (tool_call);

  return foundry_llm_tool_dup_name (self->tool);
}

static gboolean
plugin_ollama_llm_tool_call_is_callable (FoundryLlmToolCall *tool_call)
{
  PluginOllamaLlmToolCall *self = PLUGIN_OLLAMA_LLM_TOOL_CALL (tool_call);

  return self->is_callable;
}

static void
plugin_ollama_llm_tool_call_finalize (GObject *object)
{
  PluginOllamaLlmToolCall *self = (PluginOllamaLlmToolCall *)object;

  g_clear_object (&self->tool);
  g_clear_pointer (&self->arguments, json_node_unref);

  G_OBJECT_CLASS (plugin_ollama_llm_tool_call_parent_class)->finalize (object);
}

static void
plugin_ollama_llm_tool_call_class_init (PluginOllamaLlmToolCallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLlmToolCallClass *tool_call_class = FOUNDRY_LLM_TOOL_CALL_CLASS (klass);

  object_class->finalize = plugin_ollama_llm_tool_call_finalize;

  tool_call_class->dup_title = plugin_ollama_llm_tool_call_dup_title;
  tool_call_class->is_callable = plugin_ollama_llm_tool_call_is_callable;
}

static void
plugin_ollama_llm_tool_call_init (PluginOllamaLlmToolCall *self)
{
}

FoundryLlmToolCall *
plugin_ollama_llm_tool_call_new (FoundryLlmTool *tool,
                                 JsonNode       *arguments)
{
  PluginOllamaLlmToolCall *self;

  g_return_val_if_fail (FOUNDRY_IS_LLM_TOOL (tool), NULL);
  g_return_val_if_fail (arguments != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_OLLAMA_LLM_TOOL_CALL, NULL);
  self->tool = g_object_ref (tool);
  self->arguments = json_node_ref (arguments);
  self->is_callable = TRUE;

  return FOUNDRY_LLM_TOOL_CALL (self);
}
