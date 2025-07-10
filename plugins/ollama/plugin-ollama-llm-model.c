/* plugin-ollama-llm-model.c
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

#include "plugin-ollama-llm-completion.h"
#include "plugin-ollama-llm-model.h"

struct _PluginOllamaLlmModel
{
  FoundryLlmModel     parent_instance;
  PluginOllamaClient *client;
  JsonNode           *node;
};

G_DEFINE_FINAL_TYPE (PluginOllamaLlmModel, plugin_ollama_llm_model, FOUNDRY_TYPE_LLM_MODEL)

static char *
plugin_ollama_llm_model_dup_name (FoundryLlmModel *model)
{
  PluginOllamaLlmModel *self = (PluginOllamaLlmModel *)model;
  JsonObject *obj;
  JsonNode *node;

  g_assert (PLUGIN_IS_OLLAMA_LLM_MODEL (self));

  if ((obj = json_node_get_object (self->node)) &&
      json_object_has_member (obj, "name") &&
      (node = json_object_get_member (obj, "name")) &&
      json_node_get_value_type (node) == G_TYPE_STRING)
    return g_strdup (json_node_get_string (node));

  return NULL;
}

static char *
plugin_ollama_llm_model_dup_digest (FoundryLlmModel *model)
{
  PluginOllamaLlmModel *self = (PluginOllamaLlmModel *)model;
  JsonObject *obj;
  JsonNode *node;

  g_assert (PLUGIN_IS_OLLAMA_LLM_MODEL (self));

  if ((obj = json_node_get_object (self->node)) &&
      json_object_has_member (obj, "digest") &&
      (node = json_object_get_member (obj, "digest")) &&
      json_node_get_value_type (node) == G_TYPE_STRING)
    return g_strdup (json_node_get_string (node));

  return NULL;
}

static DexFuture *
plugin_ollama_llm_model_complete (FoundryLlmModel            *model,
                                  FoundryLlmCompletionParams *params)
{
  PluginOllamaLlmModel *self = (PluginOllamaLlmModel *)model;
  g_autoptr(FoundryJsonInputStream) json_input = NULL;
  g_autoptr(GInputStream) input = NULL;
  g_autoptr(JsonObject) params_obj = NULL;
  g_autoptr(JsonNode) params_node = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *prompt = NULL;
  g_autofree char *suffix = NULL;
  g_autofree char *system = NULL;
  g_autofree char *context = NULL;
  g_autofree char *name = NULL;

  g_assert (PLUGIN_IS_OLLAMA_LLM_MODEL (self));
  g_assert (FOUNDRY_IS_LLM_COMPLETION_PARAMS (params));

  params_node = json_node_new (JSON_NODE_OBJECT);
  params_obj = json_object_new ();

  json_node_set_object (params_node, params_obj);

  if ((name = plugin_ollama_llm_model_dup_name (model)))
    json_object_set_string_member (params_obj, "model", name);

  if ((prompt = foundry_llm_completion_params_dup_prompt (params)))
    json_object_set_string_member (params_obj, "prompt", prompt);

  if ((suffix = foundry_llm_completion_params_dup_suffix (params)))
    json_object_set_string_member (params_obj, "suffix", suffix);

  if ((system = foundry_llm_completion_params_dup_system (params)))
    json_object_set_string_member (params_obj, "system", system);

  if ((context = foundry_llm_completion_params_dup_context (params)))
    json_object_set_string_member (params_obj, "context", context);

  if (foundry_llm_completion_params_get_raw (params))
    json_object_set_boolean_member (params_obj, "raw", TRUE);

  json_object_set_boolean_member (params_obj, "stream", TRUE);

  if (!(input = dex_await_object (plugin_ollama_client_post (self->client, "/api/generate", params_node), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  json_input = foundry_json_input_stream_new (input, TRUE);

  return dex_future_new_take_object (plugin_ollama_llm_completion_new (json_input));
}

static void
plugin_ollama_llm_model_finalize (GObject *object)
{
  PluginOllamaLlmModel *self = (PluginOllamaLlmModel *)object;

  g_clear_object (&self->client);
  g_clear_pointer (&self->node, json_node_unref);

  G_OBJECT_CLASS (plugin_ollama_llm_model_parent_class)->finalize (object);
}

static void
plugin_ollama_llm_model_class_init (PluginOllamaLlmModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLlmModelClass *llm_model_class = FOUNDRY_LLM_MODEL_CLASS (klass);

  object_class->finalize = plugin_ollama_llm_model_finalize;

  llm_model_class->dup_name = plugin_ollama_llm_model_dup_name;
  llm_model_class->dup_digest = plugin_ollama_llm_model_dup_digest;
  llm_model_class->complete = plugin_ollama_llm_model_complete;
}

static void
plugin_ollama_llm_model_init (PluginOllamaLlmModel *self)
{
}

PluginOllamaLlmModel *
plugin_ollama_llm_model_new (FoundryContext     *context,
                             PluginOllamaClient *client,
                             JsonNode           *node)
{
  PluginOllamaLlmModel *self;

  g_return_val_if_fail (PLUGIN_IS_OLLAMA_CLIENT (client), NULL);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (node), NULL);

  self = g_object_new (PLUGIN_TYPE_OLLAMA_LLM_MODEL,
                       "context", context,
                       NULL);
  self->client = g_object_ref (client);
  self->node = json_node_ref (node);

  return g_steal_pointer (&self);
}
