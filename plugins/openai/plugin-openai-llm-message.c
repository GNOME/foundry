/* plugin-openai-llm-message.c
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

#include "plugin-openai-llm-message.h"
#include "plugin-openai-llm-tool-call.h"

struct _PluginOpenaiLlmMessage
{
  FoundryLlmMessage  parent_instance;
  GListModel        *tools;
  JsonNode          *node;
  char              *role;
  GString           *content;
};

G_DEFINE_FINAL_TYPE (PluginOpenaiLlmMessage, plugin_openai_llm_message, FOUNDRY_TYPE_LLM_MESSAGE)

static char *
plugin_openai_llm_message_dup_role (FoundryLlmMessage *message)
{
  return g_strdup (PLUGIN_OPENAI_LLM_MESSAGE (message)->role);
}

static char *
plugin_openai_llm_message_dup_content (FoundryLlmMessage *message)
{
  PluginOpenaiLlmMessage *self = PLUGIN_OPENAI_LLM_MESSAGE (message);

  return g_strndup (self->content->str, self->content->len);
}

static gboolean
plugin_openai_llm_message_has_tool_call (FoundryLlmMessage *message)
{
  PluginOpenaiLlmMessage *self = PLUGIN_OPENAI_LLM_MESSAGE (message);
  JsonNode *tool_calls;

  if (self->node == NULL)
    return FALSE;

  if (FOUNDRY_JSON_OBJECT_PARSE (self->node, "tool_calls", FOUNDRY_JSON_NODE_GET_NODE (&tool_calls)))
    return tool_calls != NULL;

  return FALSE;
}

static FoundryLlmTool *
find_tool (GListModel *model,
           const char *function)
{
  guint n_items;

  if (model == NULL || function == NULL)
    return NULL;

  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryLlmTool) tool = g_list_model_get_item (model, i);
      g_autofree char *name = foundry_llm_tool_dup_name (tool);

      if (g_strcmp0 (name, function) == 0)
        return g_steal_pointer (&tool);
    }

  return NULL;
}

static GListModel *
plugin_openai_llm_message_list_tool_calls (FoundryLlmMessage *message)
{
  PluginOpenaiLlmMessage *self = (PluginOpenaiLlmMessage *)message;
  JsonNode *tool_calls = NULL;
  JsonArray *tool_calls_ar;
  g_autoptr(GListStore) store = NULL;
  guint length;

  g_assert (PLUGIN_IS_OPENAI_LLM_MESSAGE (self));

  if (self->node == NULL)
    return NULL;

  if (!FOUNDRY_JSON_OBJECT_PARSE (self->node, "tool_calls", FOUNDRY_JSON_NODE_GET_NODE (&tool_calls)) ||
      !JSON_NODE_HOLDS_ARRAY (tool_calls) ||
      !(tool_calls_ar = json_node_get_array (tool_calls)) ||
      json_array_get_length (tool_calls_ar) == 0)
    return NULL;

  length = json_array_get_length (tool_calls_ar);
  store = g_list_store_new (FOUNDRY_TYPE_LLM_TOOL_CALL);

  for (guint i = 0; i < length; i++)
    {
      g_autoptr(FoundryLlmToolCall) call = NULL;
      g_autoptr(FoundryLlmTool) tool = NULL;
      JsonNode *node = json_array_get_element (tool_calls_ar, i);

      {
        const char *function = NULL;
        const char *arguments_str = NULL;
        g_autoptr(JsonParser) parser = NULL;
        g_autoptr(JsonNode) args_node = NULL;
        JsonObject *func_obj;
        JsonNode *func_node;

        if (!FOUNDRY_JSON_OBJECT_PARSE (node,
                                        "function", FOUNDRY_JSON_NODE_GET_NODE (&func_node),
                                        NULL))
          {
            g_warning ("Failed to parse function call");
            continue;
          }

        if (!JSON_NODE_HOLDS_OBJECT (func_node) ||
            !(func_obj = json_node_get_object (func_node)) ||
            !json_object_has_member (func_obj, "name") ||
            !json_object_has_member (func_obj, "arguments"))
          {
            g_warning ("Invalid function call structure");
            continue;
          }

        function = json_object_get_string_member (func_obj, "name");
        arguments_str = json_object_get_string_member (func_obj, "arguments");

        if (!(tool = find_tool (self->tools, function)))
          {
            g_warning ("No such tool `%s`", function);
            continue;
          }

        /* Parse arguments JSON string */
        /* TODO: This should be on a fiber so we can decode off thread */
        parser = json_parser_new ();
        if (arguments_str != NULL && json_parser_load_from_data (parser, arguments_str, -1, NULL))
          {
            args_node = json_parser_get_root (parser);

            if (args_node != NULL)
              json_node_ref (args_node);
          }

        if ((call = plugin_openai_llm_tool_call_new (tool, args_node)))
          g_list_store_append (store, call);
      }
    }

  return G_LIST_MODEL (g_steal_pointer (&store));
}

static void
plugin_openai_llm_message_finalize (GObject *object)
{
  PluginOpenaiLlmMessage *self = (PluginOpenaiLlmMessage *)object;

  g_clear_pointer (&self->role, g_free);
  g_clear_pointer (&self->node, json_node_unref);
  g_string_free (self->content, TRUE), self->content = NULL;
  g_clear_object (&self->tools);

  G_OBJECT_CLASS (plugin_openai_llm_message_parent_class)->finalize (object);
}

static void
plugin_openai_llm_message_class_init (PluginOpenaiLlmMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLlmMessageClass *message_class = FOUNDRY_LLM_MESSAGE_CLASS (klass);

  object_class->finalize = plugin_openai_llm_message_finalize;

  message_class->dup_role = plugin_openai_llm_message_dup_role;
  message_class->dup_content = plugin_openai_llm_message_dup_content;
  message_class->has_tool_call = plugin_openai_llm_message_has_tool_call;
  message_class->list_tool_calls = plugin_openai_llm_message_list_tool_calls;
}

static void
plugin_openai_llm_message_init (PluginOpenaiLlmMessage *self)
{
  self->content = g_string_new (NULL);
}

FoundryLlmMessage *
plugin_openai_llm_message_new (const char *role,
                               const char *content)
{
  PluginOpenaiLlmMessage *self;

  self = g_object_new (PLUGIN_TYPE_OPENAI_LLM_MESSAGE, NULL);
  self->role = g_strdup (role);

  if (!foundry_str_empty0 (content))
    g_string_append (self->content, content);

  return FOUNDRY_LLM_MESSAGE (self);
}

FoundryLlmMessage *
plugin_openai_llm_message_new_for_node (JsonNode *node)
{
  PluginOpenaiLlmMessage *self;
  const char *role = NULL;
  const char *content = NULL;

  g_return_val_if_fail (node != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_OPENAI_LLM_MESSAGE, NULL);
  self->node = json_node_ref (node);

  if (FOUNDRY_JSON_OBJECT_PARSE (node, "role", FOUNDRY_JSON_NODE_GET_STRING (&role)))
    self->role = g_strdup (role);

  if (FOUNDRY_JSON_OBJECT_PARSE (node, "content", FOUNDRY_JSON_NODE_GET_STRING (&content)))
    g_string_append (self->content, content);

  return FOUNDRY_LLM_MESSAGE (self);
}

JsonNode *
plugin_openai_llm_message_to_json (PluginOpenaiLlmMessage *self)
{
  g_assert (PLUGIN_IS_OPENAI_LLM_MESSAGE (self));

  if (self->node == NULL)
    return FOUNDRY_JSON_OBJECT_NEW ("role", FOUNDRY_JSON_NODE_PUT_STRING (self->role),
                                    "content", FOUNDRY_JSON_NODE_PUT_STRING (self->content->str));

  return json_node_ref (self->node);
}

void
plugin_openai_llm_message_append (PluginOpenaiLlmMessage *self,
                                  JsonNode               *node)
{
  const char *content = NULL;

  g_assert (PLUGIN_IS_OPENAI_LLM_MESSAGE (self));
  g_assert (node != NULL);

  if (FOUNDRY_JSON_OBJECT_PARSE (node, "content", FOUNDRY_JSON_NODE_GET_STRING (&content)))
    {
      if (content != NULL)
        {
          g_string_append (self->content, content);
          g_object_notify (G_OBJECT (self), "content");
        }
    }
}

void
plugin_openai_llm_message_set_tools (PluginOpenaiLlmMessage *self,
                                     GListModel             *tools)
{
  g_return_if_fail (PLUGIN_IS_OPENAI_LLM_MESSAGE (self));
  g_return_if_fail (!tools || G_IS_LIST_MODEL (tools));

  g_set_object (&self->tools, tools);
}
