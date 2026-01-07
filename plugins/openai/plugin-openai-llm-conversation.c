/* plugin-openai-llm-conversation.c
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

#include "config.h"

#include "foundry-json-input-stream-private.h"

#include "plugin-openai-llm-conversation.h"
#include "plugin-openai-llm-message.h"
#include "plugin-openai-llm-model.h"

struct _PluginOpenaiLlmConversation
{
  FoundryLlmConversation  parent_instance;
  PluginOpenaiClient     *client;
  char                   *model;
  PluginOpenaiLlmMessage *system;
  GListStore             *context;
  GListStore             *history;
  guint                   busy : 1;
};

G_DEFINE_FINAL_TYPE (PluginOpenaiLlmConversation, plugin_openai_llm_conversation, FOUNDRY_TYPE_LLM_CONVERSATION)

static void
plugin_openai_llm_conversation_reset (FoundryLlmConversation *conversation)
{
  PluginOpenaiLlmConversation *self = PLUGIN_OPENAI_LLM_CONVERSATION (conversation);

  g_list_store_remove_all (self->context);
  g_list_store_remove_all (self->history);
}

static DexFuture *
plugin_openai_llm_conversation_add_context (FoundryLlmConversation *conversation,
                                            const char             *context)
{
  PluginOpenaiLlmConversation *self = PLUGIN_OPENAI_LLM_CONVERSATION (conversation);
  g_autoptr(FoundryLlmMessage) message = plugin_openai_llm_message_new ("user", context);

  g_list_store_append (self->context, message);

  return dex_future_new_true ();
}

typedef PluginOpenaiLlmConversation Busy;

static Busy *
acquire_busy (PluginOpenaiLlmConversation *self)
{
  self->busy = TRUE;
  g_object_notify (G_OBJECT (self), "is-busy");
  return g_object_ref (self);
}

static void
release_busy (Busy *busy)
{
  busy->busy = FALSE;
  g_object_notify (G_OBJECT (busy), "is-busy");
  g_object_unref (busy);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (Busy, release_busy)

static void
read_line_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  DexPromise *promise = user_data;
  g_autoptr(GError) error = NULL;
  gsize length;
  g_autofree char *line = NULL;

  g_assert (G_IS_DATA_INPUT_STREAM (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  line = g_data_input_stream_read_line_finish (G_DATA_INPUT_STREAM (object), result, &length, &error);

  if (error != NULL)
    {
      dex_promise_reject (promise, g_steal_pointer (&error));
    }
  else if (line == NULL)
    {
      dex_promise_resolve_string (promise, NULL);
    }
  else
    {
      if (length > 0 && line[length - 1] == '\n')
        {
          line[length - 1] = '\0';
          length--;
        }

      if (length > 0 && line[length - 1] == '\r')
        {
          line[length - 1] = '\0';
          length--;
        }

      dex_promise_resolve_string (promise, g_steal_pointer (&line));
    }
}

static DexFuture *
read_line_async (GDataInputStream *stream)
{
  DexPromise *promise;

  g_assert (G_IS_DATA_INPUT_STREAM (stream));

  promise = dex_promise_new_cancellable ();
  g_data_input_stream_read_line_async (stream,
                                       G_PRIORITY_DEFAULT,
                                       dex_promise_get_cancellable (promise),
                                       read_line_cb,
                                       dex_ref (promise));
  return DEX_FUTURE (promise);
}

static DexFuture *
plugin_openai_llm_conversation_converse_fiber (gpointer data)
{
  PluginOpenaiLlmConversation *self = data;
  g_autoptr(FoundryJsonInputStream) json_input = NULL;
  g_autoptr(FoundryLlmMessage) last_message = NULL;
  g_autoptr(GInputStream) input = NULL;
  g_autoptr(GListModel) tools = NULL;
  g_autoptr(JsonObject) params_obj = NULL;
  g_autoptr(JsonNode) params_node = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(Busy) busy = NULL;
  g_autofree char *line = NULL;
  guint n_context;
  guint n_history;

  g_assert (PLUGIN_IS_OPENAI_LLM_CONVERSATION (self));

  busy = acquire_busy (self);

  params_node = json_node_new (JSON_NODE_OBJECT);
  params_obj = json_object_new ();
  json_node_set_object (params_node, params_obj);

  if (self->model != NULL)
    json_object_set_string_member (params_obj, "model", self->model);

  /* Build input string from conversation history */
  {
    g_autoptr(GString) input_str = g_string_new (NULL);
    n_context = g_list_model_get_n_items (G_LIST_MODEL (self->context));
    n_history = g_list_model_get_n_items (G_LIST_MODEL (self->history));

    /* Add system message if present */
    if (self->system != NULL)
      {
        g_autofree char *system_content = foundry_llm_message_dup_content (FOUNDRY_LLM_MESSAGE (self->system));
        if (system_content != NULL && system_content[0] != '\0')
          {
            g_string_append (input_str, "System: ");
            g_string_append (input_str, system_content);
            g_string_append_c (input_str, '\n');
          }
      }

    /* Add context messages */
    for (guint i = 0; i < n_context; i++)
      {
        g_autoptr(FoundryLlmMessage) message = g_list_model_get_item (G_LIST_MODEL (self->context), i);
        if (message != NULL)
          {
            g_autofree char *role = foundry_llm_message_dup_role (message);
            g_autofree char *content = foundry_llm_message_dup_content (message);
            if (content != NULL && content[0] != '\0')
              {
                if (role != NULL)
                  {
                    g_string_append (input_str, role);
                    g_string_append (input_str, ": ");
                  }
                g_string_append (input_str, content);
                g_string_append_c (input_str, '\n');
              }
          }
      }

    /* Add history messages */
    for (guint i = 0; i < n_history; i++)
      {
        g_autoptr(FoundryLlmMessage) message = g_list_model_get_item (G_LIST_MODEL (self->history), i);
        if (message != NULL)
          {
            g_autofree char *role = foundry_llm_message_dup_role (message);
            g_autofree char *content = foundry_llm_message_dup_content (message);
            if (content != NULL && content[0] != '\0')
              {
                if (role != NULL)
                  {
                    g_string_append (input_str, role);
                    g_string_append (input_str, ": ");
                  }
                g_string_append (input_str, content);
                g_string_append_c (input_str, '\n');
              }
          }
      }

    if (input_str->len > 0)
      json_object_set_string_member (params_obj, "input", input_str->str);
  }

  json_object_set_boolean_member (params_obj, "stream", TRUE);

  if ((tools = foundry_llm_conversation_dup_tools (FOUNDRY_LLM_CONVERSATION (self))))
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
                                                        "OpenAI does not support tool parameter type `%s`",
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
        }
    }

  if (!(input = dex_await_object (plugin_openai_client_post (self->client, "responses", params_node), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  json_input = foundry_json_input_stream_new (input, TRUE);

  /* OpenAI uses SSE format, read line by line using GDataInputStream directly */
  /* FoundryJsonInputStream is a GDataInputStream, so we can cast it */
  while ((line = dex_await_string (read_line_async (G_DATA_INPUT_STREAM (json_input)), &error)))
    {
      const char *data_start;
      g_autoptr(JsonParser) parser = NULL;
      g_autoptr(JsonNode) reply = NULL;
      JsonObject *reply_obj;
      const char *content = NULL;
      const char *finish_reason = NULL;
      JsonNode *output = NULL;
      JsonArray *output_ar = NULL;
      JsonNode *output_item = NULL;
      JsonObject *output_obj = NULL;
      JsonNode *content_arr = NULL;
      JsonArray *content_ar = NULL;
      JsonNode *content_item = NULL;
      JsonObject *content_obj = NULL;
      JsonNode *text_node = NULL;

      /* Skip empty lines and non-data lines */
      if (!g_str_has_prefix (line, "data: "))
        {
          if (g_str_equal (line, ""))
            continue;
          continue;
        }

      data_start = line + 6; /* Skip "data: " */

      /* Handle "[DONE]" marker */
      if (g_str_equal (data_start, "[DONE]"))
        break;

      /* Parse JSON from the data line */
      parser = json_parser_new ();
      if (!json_parser_load_from_data (parser, data_start, -1, &error))
        {
          g_clear_error (&error);
          continue;
        }

      {
        JsonNode *root = json_parser_get_root (parser);
        if (root == NULL || !JSON_NODE_HOLDS_OBJECT (root))
          continue;

        /* Take a reference since the parser owns the node */
        reply = json_node_ref (root);
      }

      reply_obj = json_node_get_object (reply);

      /* Extract content from responses API format: output[0].content[0].text */
      /* For streaming, we might get incremental updates in output array */

      if (!json_object_has_member (reply_obj, "output") ||
          !(output = json_object_get_member (reply_obj, "output")) ||
          !JSON_NODE_HOLDS_ARRAY (output) ||
          !(output_ar = json_node_get_array (output)) ||
          json_array_get_length (output_ar) == 0 ||
          !(output_item = json_array_get_element (output_ar, 0)) ||
          !JSON_NODE_HOLDS_OBJECT (output_item) ||
          !(output_obj = json_node_get_object (output_item)) ||
          !json_object_has_member (output_obj, "content") ||
          !(content_arr = json_object_get_member (output_obj, "content")) ||
          !JSON_NODE_HOLDS_ARRAY (content_arr) ||
          !(content_ar = json_node_get_array (content_arr)) ||
          json_array_get_length (content_ar) == 0 ||
          !(content_item = json_array_get_element (content_ar, 0)) ||
          !JSON_NODE_HOLDS_OBJECT (content_item) ||
          !(content_obj = json_node_get_object (content_item)) ||
          !json_object_has_member (content_obj, "text") ||
          !(text_node = json_object_get_member (content_obj, "text")) ||
          json_node_get_value_type (text_node) != G_TYPE_STRING)
        continue;

      content = json_node_get_string (text_node);
      if (content != NULL && content[0] != '\0')
        {
          if (last_message != NULL)
            {
              /* Append to existing message */
              g_autoptr(JsonNode) content_node = json_node_new (JSON_NODE_OBJECT);
              g_autoptr(JsonObject) new_content_obj = json_object_new ();

              json_node_set_object (content_node, new_content_obj);
              json_object_set_string_member (new_content_obj, "content", content);

              plugin_openai_llm_message_append (PLUGIN_OPENAI_LLM_MESSAGE (last_message), content_node);
            }
          else
            {
              /* Create new message from output object which has role and content */
              g_autoptr(JsonNode) message_node = json_node_new (JSON_NODE_OBJECT);
              g_autoptr(JsonObject) message_obj = json_object_new ();
              const char *role = "assistant";

              json_node_set_object (message_node, message_obj);

              /* Extract role from output object if available */
              if (json_object_has_member (output_obj, "role"))
                {
                  JsonNode *role_node = json_object_get_member (output_obj, "role");
                  if (role_node != NULL && json_node_get_value_type (role_node) == G_TYPE_STRING)
                    role = json_node_get_string (role_node);
                }

              json_object_set_string_member (message_obj, "role", role);
              json_object_set_string_member (message_obj, "content", content);

              last_message = plugin_openai_llm_message_new_for_node (message_node);

              if (foundry_llm_message_has_tool_call (last_message))
                plugin_openai_llm_message_set_tools (PLUGIN_OPENAI_LLM_MESSAGE (last_message), tools);

              g_list_store_append (self->history, last_message);
            }
        }

      /* Check for completion status */
      if (json_object_has_member (reply_obj, "status") &&
          (finish_reason = json_object_get_string_member (reply_obj, "status")) != NULL &&
          (g_str_equal (finish_reason, "completed") || g_str_equal (finish_reason, "failed")))
        break;
    }

  if (error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
plugin_openai_llm_conversation_send (PluginOpenaiLlmConversation *self)
{
  g_assert (PLUGIN_IS_OPENAI_LLM_CONVERSATION (self));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_openai_llm_conversation_converse_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static DexFuture *
plugin_openai_llm_conversation_send_messages (FoundryLlmConversation *conversation,
                                              const char * const     *roles,
                                              const char * const     *messages)
{
  PluginOpenaiLlmConversation *self = PLUGIN_OPENAI_LLM_CONVERSATION (conversation);

  g_assert (PLUGIN_IS_OPENAI_LLM_CONVERSATION (self));
  g_assert (roles != NULL && roles[0] != NULL);
  g_assert (messages != NULL && messages[0] != NULL);

  for (guint i = 0; roles[i]; i++)
    {
      g_autoptr(FoundryLlmMessage) message = plugin_openai_llm_message_new (roles[i], messages[i]);

      g_list_store_append (self->history, message);
    }

  return plugin_openai_llm_conversation_send (self);
}

static GListModel *
plugin_openai_llm_conversation_list_history (FoundryLlmConversation *conversation)
{
  PluginOpenaiLlmConversation *self = PLUGIN_OPENAI_LLM_CONVERSATION (conversation);
  g_autoptr(GListStore) store = g_list_store_new (G_TYPE_LIST_MODEL);

  g_list_store_append (store, self->context);
  g_list_store_append (store, self->history);

  return foundry_flatten_list_model_new (G_LIST_MODEL (g_steal_pointer (&store)));
}

static DexFuture *
add_to_history_cb (DexFuture *completed,
                   gpointer   user_data)
{
  PluginOpenaiLlmConversation *self = user_data;
  g_autoptr(FoundryLlmMessage) message = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (PLUGIN_IS_OPENAI_LLM_CONVERSATION (self));

  if ((message = dex_await_object (dex_ref (completed), NULL)))
    {
      g_list_store_append (self->history, message);

      plugin_openai_llm_conversation_send (self);
    }

  return dex_ref (completed);
}

static DexFuture *
plugin_openai_llm_conversation_call (FoundryLlmConversation *conversation,
                                     FoundryLlmToolCall     *tool_call)
{
  PluginOpenaiLlmConversation *self = (PluginOpenaiLlmConversation *)conversation;
  DexFuture *future;

  g_assert (PLUGIN_IS_OPENAI_LLM_CONVERSATION (self));
  g_assert (FOUNDRY_IS_LLM_TOOL_CALL (tool_call));

  future = foundry_llm_tool_call_confirm (tool_call);
  future = dex_future_then (future,
                            add_to_history_cb,
                            g_object_ref (self),
                            g_object_unref);

  return future;
}

static void
plugin_openai_llm_conversation_finalize (GObject *object)
{
  PluginOpenaiLlmConversation *self = (PluginOpenaiLlmConversation *)object;

  g_clear_object (&self->history);
  g_clear_object (&self->context);
  g_clear_object (&self->system);
  g_clear_object (&self->client);
  g_clear_pointer (&self->model, g_free);

  G_OBJECT_CLASS (plugin_openai_llm_conversation_parent_class)->finalize (object);
}

static void
plugin_openai_llm_conversation_class_init (PluginOpenaiLlmConversationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLlmConversationClass *conversation_class = FOUNDRY_LLM_CONVERSATION_CLASS (klass);

  object_class->finalize = plugin_openai_llm_conversation_finalize;

  conversation_class->reset = plugin_openai_llm_conversation_reset;
  conversation_class->add_context = plugin_openai_llm_conversation_add_context;
  conversation_class->send_messages = plugin_openai_llm_conversation_send_messages;
  conversation_class->list_history = plugin_openai_llm_conversation_list_history;
  conversation_class->call = plugin_openai_llm_conversation_call;
}

static void
plugin_openai_llm_conversation_init (PluginOpenaiLlmConversation *self)
{
  self->history = g_list_store_new (FOUNDRY_TYPE_LLM_MESSAGE);
  self->context = g_list_store_new (FOUNDRY_TYPE_LLM_MESSAGE);
}

FoundryLlmConversation *
plugin_openai_llm_conversation_new (PluginOpenaiClient *client,
                                    const char         *model,
                                    const char         *system)
{
  PluginOpenaiLlmConversation *self;

  g_return_val_if_fail (PLUGIN_IS_OPENAI_CLIENT (client), NULL);
  g_return_val_if_fail (model != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_OPENAI_LLM_CONVERSATION, NULL);
  self->client = g_object_ref (client);
  self->model = g_strdup (model);

  if (system != NULL)
    self->system = PLUGIN_OPENAI_LLM_MESSAGE (plugin_openai_llm_message_new ("system", system));

  return FOUNDRY_LLM_CONVERSATION (self);
}
