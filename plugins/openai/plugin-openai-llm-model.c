/* plugin-openai-llm-model.c
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
#include "foundry-json-output-stream-private.h"

#include "plugin-openai-llm-completion.h"
#include "plugin-openai-llm-conversation.h"
#include "plugin-openai-llm-model.h"

struct _PluginOpenaiLlmModel
{
  FoundryLlmModel     parent_instance;
  PluginOpenaiClient *client;
  JsonNode           *node;
};

G_DEFINE_FINAL_TYPE (PluginOpenaiLlmModel, plugin_openai_llm_model, FOUNDRY_TYPE_LLM_MODEL)

static char *
plugin_openai_llm_model_dup_name (FoundryLlmModel *model)
{
  PluginOpenaiLlmModel *self = (PluginOpenaiLlmModel *)model;
  JsonObject *obj;
  JsonNode *node;

  g_assert (PLUGIN_IS_OPENAI_LLM_MODEL (self));

  if ((obj = json_node_get_object (self->node)) &&
      json_object_has_member (obj, "id") &&
      (node = json_object_get_member (obj, "id")) &&
      json_node_get_value_type (node) == G_TYPE_STRING)
    return g_strconcat ("openai:", json_node_get_string (node), NULL);

  return NULL;
}

static char *
plugin_openai_llm_model_dup_digest (FoundryLlmModel *model)
{
  PluginOpenaiLlmModel *self = (PluginOpenaiLlmModel *)model;
  JsonObject *obj;
  JsonNode *node;

  g_assert (PLUGIN_IS_OPENAI_LLM_MODEL (self));

  if ((obj = json_node_get_object (self->node)) &&
      json_object_has_member (obj, "id") &&
      (node = json_object_get_member (obj, "id")) &&
      json_node_get_value_type (node) == G_TYPE_STRING)
    return g_strdup (json_node_get_string (node));

  return NULL;
}

static gboolean
plugin_openai_llm_model_is_metered (FoundryLlmModel *model)
{
  return TRUE;
}

static DexFuture *
plugin_openai_llm_model_complete (FoundryLlmModel    *model,
                                  const char * const *roles,
                                  const char * const *messages)
{
  PluginOpenaiLlmModel *self = (PluginOpenaiLlmModel *)model;
  g_autoptr(FoundryJsonInputStream) json_input = NULL;
  g_autoptr(GInputStream) input = NULL;
  g_autoptr(JsonObject) params_obj = NULL;
  g_autoptr(JsonArray) messages_ar = NULL;
  g_autoptr(JsonNode) params_node = NULL;
  g_autoptr(JsonNode) messages_node = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *name = NULL;

  g_assert (PLUGIN_IS_OPENAI_LLM_MODEL (self));
  g_assert (roles != NULL);
  g_assert (messages != NULL);

  params_node = json_node_new (JSON_NODE_OBJECT);
  params_obj = json_object_new ();
  json_node_set_object (params_node, params_obj);

  messages_node = json_node_new (JSON_NODE_ARRAY);
  messages_ar = json_array_new ();
  json_node_set_array (messages_node, messages_ar);

  if ((name = plugin_openai_llm_model_dup_name (model)))
    {
      if (g_str_has_prefix (name, "openai:"))
        json_object_set_string_member (params_obj, "model", name + strlen ("openai:"));
      else
        json_object_set_string_member (params_obj, "model", name);
    }

  for (guint i = 0; roles[i]; i++)
    {
      g_autoptr(JsonNode) message_node = NULL;
      g_autoptr(JsonObject) message_obj = NULL;

      message_node = json_node_new (JSON_NODE_OBJECT);
      message_obj = json_object_new ();
      json_node_set_object (message_node, message_obj);

      json_object_set_string_member (message_obj, "role", roles[i]);
      json_object_set_string_member (message_obj, "content", messages[i]);

      json_array_add_element (messages_ar, g_steal_pointer (&message_node));
    }

  json_object_set_member (params_obj, "messages", g_steal_pointer (&messages_node));
  json_object_set_boolean_member (params_obj, "stream", TRUE);

  if (!(input = dex_await_object (plugin_openai_client_post (self->client, "chat/completions", params_node), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  json_input = foundry_json_input_stream_new (input, TRUE);

  return dex_future_new_take_object (plugin_openai_llm_completion_new (json_input));
}

static DexFuture *
plugin_openai_llm_model_chat (FoundryLlmModel *model,
                              const char      *system)
{
  PluginOpenaiLlmModel *self = (PluginOpenaiLlmModel *)model;
  g_autofree char *name = NULL;

  g_assert (PLUGIN_IS_OPENAI_LLM_MODEL (self));

  name = plugin_openai_llm_model_dup_name (model);
  if (name && g_str_has_prefix (name, "openai:"))
    name = g_strdup (name + strlen ("openai:"));

  return dex_future_new_take_object (plugin_openai_llm_conversation_new (self->client, name, system));
}

static void
plugin_openai_llm_model_finalize (GObject *object)
{
  PluginOpenaiLlmModel *self = (PluginOpenaiLlmModel *)object;

  g_clear_object (&self->client);
  g_clear_pointer (&self->node, json_node_unref);

  G_OBJECT_CLASS (plugin_openai_llm_model_parent_class)->finalize (object);
}

static void
plugin_openai_llm_model_class_init (PluginOpenaiLlmModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLlmModelClass *llm_model_class = FOUNDRY_LLM_MODEL_CLASS (klass);

  object_class->finalize = plugin_openai_llm_model_finalize;

  llm_model_class->dup_name = plugin_openai_llm_model_dup_name;
  llm_model_class->dup_digest = plugin_openai_llm_model_dup_digest;
  llm_model_class->is_metered = plugin_openai_llm_model_is_metered;
  llm_model_class->chat = plugin_openai_llm_model_chat;
  llm_model_class->complete = plugin_openai_llm_model_complete;
}

static void
plugin_openai_llm_model_init (PluginOpenaiLlmModel *self)
{
}

PluginOpenaiLlmModel *
plugin_openai_llm_model_new (FoundryContext     *context,
                             PluginOpenaiClient *client,
                             JsonNode           *node)
{
  PluginOpenaiLlmModel *self;

  g_return_val_if_fail (PLUGIN_IS_OPENAI_CLIENT (client), NULL);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (node), NULL);

  self = g_object_new (PLUGIN_TYPE_OPENAI_LLM_MODEL,
                       "context", context,
                       NULL);
  self->client = g_object_ref (client);
  self->node = json_node_ref (node);

  return g_steal_pointer (&self);
}
