/* plugin-ollama-llm-conversation.c
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

#include "foundry-json-input-stream-private.h"

#include "plugin-ollama-llm-conversation.h"
#include "plugin-ollama-llm-message.h"
#include "plugin-ollama-llm-model.h"

struct _PluginOllamaLlmConversation
{
  FoundryLlmConversation  parent_instance;
  PluginOllamaClient     *client;
  char                   *model;
  PluginOllamaLlmMessage *system;
  GListStore             *context;
  GListStore             *history;
};

G_DEFINE_FINAL_TYPE (PluginOllamaLlmConversation, plugin_ollama_llm_conversation, FOUNDRY_TYPE_LLM_CONVERSATION)

static void
plugin_ollama_llm_conversation_reset (FoundryLlmConversation *conversation)
{
  PluginOllamaLlmConversation *self = PLUGIN_OLLAMA_LLM_CONVERSATION (conversation);

  g_list_store_remove_all (self->context);
  g_list_store_remove_all (self->history);
}

static DexFuture *
plugin_ollama_llm_conversation_add_context (FoundryLlmConversation *conversation,
                                            const char             *context)
{
  PluginOllamaLlmConversation *self = PLUGIN_OLLAMA_LLM_CONVERSATION (conversation);
  g_autoptr(FoundryLlmMessage) message = plugin_ollama_llm_message_new ("context", context);

  g_list_store_append (self->context, message);

  return dex_future_new_true ();
}

static DexFuture *
plugin_ollama_llm_conversation_converse_fiber (gpointer data)
{
  PluginOllamaLlmConversation *self = data;
  g_autoptr(FoundryJsonInputStream) json_input = NULL;
  g_autoptr(GInputStream) input = NULL;
  g_autoptr(GListModel) tools = NULL;
  g_autoptr(JsonObject) params_obj = NULL;
  g_autoptr(JsonArray) messages_ar = NULL;
  g_autoptr(JsonNode) params_node = NULL;
  g_autoptr(JsonNode) messages_node = NULL;
  g_autoptr(GError) error = NULL;
  gpointer replyptr;
  guint n_context;
  int stream = -1;

  g_assert (PLUGIN_IS_OLLAMA_LLM_CONVERSATION (self));

  params_node = json_node_new (JSON_NODE_OBJECT);
  params_obj = json_object_new ();
  json_node_set_object (params_node, params_obj);

  messages_node = json_node_new (JSON_NODE_ARRAY);
  messages_ar = json_array_new ();
  json_node_set_array (messages_node, messages_ar);

  if (self->model != NULL)
    json_object_set_string_member (params_obj, "model", self->model);

  if (self->system != NULL)
    json_array_add_element (messages_ar, plugin_ollama_llm_message_to_json (self->system));

  n_context = g_list_model_get_n_items (G_LIST_MODEL (self->context));

  for (guint i = 0; i < n_context; i++)
    {
      g_autoptr(PluginOllamaLlmMessage) message = g_list_model_get_item (G_LIST_MODEL (self->context), i);

      if (message != NULL)
        json_array_add_element (messages_ar, plugin_ollama_llm_message_to_json (message));
    }

  json_object_set_member (params_obj, "messages", g_steal_pointer (&messages_node));

  if ((tools = foundry_llm_conversation_list_tools (FOUNDRY_LLM_CONVERSATION (self))))
    {
      guint n_tools = g_list_model_get_n_items (tools);

      if (n_tools > 0)
        {
          g_autoptr(JsonArray) tools_ar = NULL;
          g_autoptr(JsonNode) tools_node = NULL;

          tools_node = json_node_new (JSON_NODE_ARRAY);
          tools_ar = json_array_new ();
          json_node_set_array (tools_node, tools_ar);

          for (guint i = 0; i < n_tools; i++)
            {
              g_autoptr(FoundryLlmTool) tool = g_list_model_get_item (tools, i);
              g_autofree GParamSpec **parameters = NULL;
              g_autofree char *tool_name = foundry_llm_tool_dup_name (tool);
              g_autofree char *tool_desc = foundry_llm_tool_dup_description (tool);
              g_autoptr(JsonObject) properties_obj = json_object_new ();
              g_autoptr(JsonArray) required_ar = json_array_new ();
              g_autoptr(JsonNode) properties_node = json_node_new (JSON_NODE_OBJECT);
              g_autoptr(JsonNode) required_node = json_node_new (JSON_NODE_ARRAY);
              g_autoptr(JsonNode) tool_node = NULL;
              guint n_params = 0;

              json_node_set_object (properties_node, properties_obj);
              json_node_set_array (required_node, required_ar);

              if ((parameters = foundry_llm_tool_list_parameters (tool, &n_params)))
                {
                  for (guint p = 0; p < n_params; p++)
                    {
                      g_autoptr(JsonObject) property_obj = json_object_new ();
                      g_autoptr(JsonNode) property_node = json_node_new (JSON_NODE_OBJECT);
                      GParamSpec *pspec = parameters[p];
                      const char *blurb;

                      json_node_set_object (property_node, property_obj);

                      if (G_IS_PARAM_SPEC_STRING (pspec))
                        {
                          json_object_set_string_member (property_obj, "type", "string");
                        }
                      else if (G_IS_PARAM_SPEC_DOUBLE (pspec))
                        {
                          json_object_set_string_member (property_obj, "type", "number");
                        }
                      else if (G_IS_PARAM_SPEC_INT64 (pspec) || G_IS_PARAM_SPEC_INT (pspec))
                        {
                          json_object_set_string_member (property_obj, "type", "integer");
                        }
                      else if (G_IS_PARAM_SPEC_BOOLEAN (pspec))
                        {
                          json_object_set_string_member (property_obj, "type", "boolean");
                        }
                      else if (G_IS_PARAM_SPEC_BOXED (pspec) &&
                               g_type_is_a (pspec->value_type, JSON_TYPE_ARRAY))
                        {
                          json_object_set_string_member (property_obj, "type", "array");
                        }
                      else if (G_IS_PARAM_SPEC_BOXED (pspec) &&
                               g_type_is_a (pspec->value_type, JSON_TYPE_OBJECT))
                        {
                          json_object_set_string_member (property_obj, "type", "object");
                        }
                      else
                        {
                          return dex_future_new_reject (G_IO_ERROR,
                                                        G_IO_ERROR_NOT_SUPPORTED,
                                                        "Ollama does not support tool parameter type `%s`",
                                                        g_type_name (pspec->value_type));
                        }

                      if ((blurb = g_param_spec_get_blurb (pspec)))
                        json_object_set_string_member (property_obj, "description", blurb);

                      json_array_add_string_element (required_ar, pspec->name);

                      json_object_set_member (properties_obj, pspec->name, g_steal_pointer (&property_node));
                    }
                }

              tool_node = FOUNDRY_JSON_OBJECT_NEW ("type", "function",
                                                   "function", "{",
                                                     "name", FOUNDRY_JSON_NODE_PUT_STRING (tool_name),
                                                     "description", FOUNDRY_JSON_NODE_PUT_STRING (tool_desc),
                                                     "parameters", "{",
                                                       "type", "object",
                                                       "properties", FOUNDRY_JSON_NODE_PUT_NODE (properties_node),
                                                       "required", FOUNDRY_JSON_NODE_PUT_NODE (required_node),
                                                     "}",
                                                   "}");

              json_array_add_element (tools_ar, g_steal_pointer (&tool_node));
            }

          json_object_set_member (params_obj, "tools", g_steal_pointer (&tools_node));

          stream = FALSE;
        }
    }

  if (stream != -1)
    json_object_set_boolean_member (params_obj, "stream", stream);

  if (!(input = dex_await_object (plugin_ollama_client_post (self->client, "/api/chat", params_node), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  json_input = foundry_json_input_stream_new (input, TRUE);

  while ((replyptr = dex_await_boxed (foundry_json_input_stream_read_upto (json_input, "\n", 1), &error)))
    {
      g_autoptr(FoundryLlmMessage) message = NULL;
      g_autoptr(JsonNode) reply = replyptr;
      const char *role = NULL;
      const char *content = NULL;
      JsonObject *reply_obj;
      JsonNode *message_node;

      if (!JSON_NODE_HOLDS_OBJECT (reply) ||
          !(reply_obj = json_node_get_object (reply)) ||
          !(message_node = json_object_get_member (reply_obj, "message")) ||
          !JSON_NODE_HOLDS_OBJECT (message_node))
        continue;

      if (!FOUNDRY_JSON_OBJECT_PARSE (message_node,
                                      "role", FOUNDRY_JSON_NODE_GET_STRING (&role),
                                      "content", FOUNDRY_JSON_NODE_GET_STRING (&content)))
        continue;

      message = plugin_ollama_llm_message_new_for_node (message_node);

      g_list_store_append (self->history, message);
    }

  if (error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
plugin_ollama_llm_conversation_send_messages (FoundryLlmConversation *conversation,
                                              const char * const     *roles,
                                              const char * const     *messages)
{
  PluginOllamaLlmConversation *self = PLUGIN_OLLAMA_LLM_CONVERSATION (conversation);

  g_assert (PLUGIN_IS_OLLAMA_LLM_CONVERSATION (self));
  g_assert (roles != NULL && roles[0] != NULL);
  g_assert (messages != NULL && messages[0] != NULL);

  for (guint i = 0; roles[i]; i++)
    {
      g_autoptr(FoundryLlmMessage) message = plugin_ollama_llm_message_new (roles[i], messages[i]);

      g_list_store_append (self->history, message);
    }

  return dex_scheduler_spawn (NULL, 0,
                              plugin_ollama_llm_conversation_converse_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static void
plugin_ollama_llm_conversation_finalize (GObject *object)
{
  PluginOllamaLlmConversation *self = (PluginOllamaLlmConversation *)object;

  g_clear_object (&self->history);
  g_clear_object (&self->context);
  g_clear_object (&self->system);
  g_clear_object (&self->client);
  g_clear_pointer (&self->model, g_free);

  G_OBJECT_CLASS (plugin_ollama_llm_conversation_parent_class)->finalize (object);
}

static void
plugin_ollama_llm_conversation_class_init (PluginOllamaLlmConversationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLlmConversationClass *conversation_class = FOUNDRY_LLM_CONVERSATION_CLASS (klass);

  object_class->finalize = plugin_ollama_llm_conversation_finalize;

  conversation_class->reset = plugin_ollama_llm_conversation_reset;
  conversation_class->add_context = plugin_ollama_llm_conversation_add_context;
  conversation_class->send_messages = plugin_ollama_llm_conversation_send_messages;
}

static void
plugin_ollama_llm_conversation_init (PluginOllamaLlmConversation *self)
{
  self->history = g_list_store_new (FOUNDRY_TYPE_LLM_MESSAGE);
  self->context = g_list_store_new (FOUNDRY_TYPE_LLM_MESSAGE);
}

FoundryLlmConversation *
plugin_ollama_llm_conversation_new (PluginOllamaClient *client,
                                    const char         *model,
                                    const char         *system)
{
  PluginOllamaLlmConversation *self;

  g_return_val_if_fail (PLUGIN_IS_OLLAMA_CLIENT (client), NULL);
  g_return_val_if_fail (model != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_OLLAMA_LLM_CONVERSATION, NULL);
  self->client = g_object_ref (client);
  self->model = g_strdup (model);

  if (system != NULL)
    {
      self->system = PLUGIN_OLLAMA_LLM_MESSAGE (plugin_ollama_llm_message_new ("system", system));
      g_list_store_append (self->history, self->system);
    }

  return FOUNDRY_LLM_CONVERSATION (self);
}
