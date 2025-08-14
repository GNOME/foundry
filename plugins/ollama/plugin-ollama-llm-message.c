/* plugin-ollama-llm-message.c
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

#include "plugin-ollama-llm-message.h"

struct _PluginOllamaLlmMessage
{
  FoundryLlmMessage parent_instance;
  JsonNode *node;
  char *role;
  char *content;
};

G_DEFINE_FINAL_TYPE (PluginOllamaLlmMessage, plugin_ollama_llm_message, FOUNDRY_TYPE_LLM_MESSAGE)

static char *
plugin_ollama_llm_message_dup_role (FoundryLlmMessage *message)
{
  return g_strdup (PLUGIN_OLLAMA_LLM_MESSAGE (message)->role);
}

static char *
plugin_ollama_llm_message_dup_content (FoundryLlmMessage *message)
{
  return g_strdup (PLUGIN_OLLAMA_LLM_MESSAGE (message)->content);
}

static void
plugin_ollama_llm_message_finalize (GObject *object)
{
  PluginOllamaLlmMessage *self = (PluginOllamaLlmMessage *)object;

  g_clear_pointer (&self->role, g_free);
  g_clear_pointer (&self->content, g_free);
  g_clear_pointer (&self->node, json_node_unref);

  G_OBJECT_CLASS (plugin_ollama_llm_message_parent_class)->finalize (object);
}

static void
plugin_ollama_llm_message_class_init (PluginOllamaLlmMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLlmMessageClass *message_class = FOUNDRY_LLM_MESSAGE_CLASS (klass);

  object_class->finalize = plugin_ollama_llm_message_finalize;

  message_class->dup_role = plugin_ollama_llm_message_dup_role;
  message_class->dup_content = plugin_ollama_llm_message_dup_content;
}

static void
plugin_ollama_llm_message_init (PluginOllamaLlmMessage *self)
{
}

FoundryLlmMessage *
plugin_ollama_llm_message_new (const char *role,
                               const char *content)
{
  PluginOllamaLlmMessage *self;

  self = g_object_new (PLUGIN_TYPE_OLLAMA_LLM_MESSAGE, NULL);
  self->role = g_strdup (role);
  self->content = g_strdup (content);

  return FOUNDRY_LLM_MESSAGE (self);
}

FoundryLlmMessage *
plugin_ollama_llm_message_new_for_node (JsonNode *node)
{
  PluginOllamaLlmMessage *self;
  const char *role = NULL;
  const char *content = NULL;

  g_return_val_if_fail (node != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_OLLAMA_LLM_MESSAGE, NULL);
  self->node = json_node_ref (node);

  if (FOUNDRY_JSON_OBJECT_PARSE (node, "role", FOUNDRY_JSON_NODE_GET_STRING (&role)))
    self->role = g_strdup (role);

  if (FOUNDRY_JSON_OBJECT_PARSE (node, "content", FOUNDRY_JSON_NODE_GET_STRING (&content)))
    self->content = g_strdup (content);

  return FOUNDRY_LLM_MESSAGE (self);
}

JsonNode *
plugin_ollama_llm_message_to_json (PluginOllamaLlmMessage *self)
{
  g_assert (PLUGIN_IS_OLLAMA_LLM_MESSAGE (self));

  if (self->node == NULL)
    return FOUNDRY_JSON_OBJECT_NEW ("role", FOUNDRY_JSON_NODE_PUT_STRING (self->role),
                                    "content", FOUNDRY_JSON_NODE_PUT_STRING (self->content));

  return json_node_ref (self->node);
}
