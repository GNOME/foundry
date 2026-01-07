/* plugin-openai-llm-completion-chunk.c
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

#include "plugin-openai-llm-completion-chunk.h"

struct _PluginOpenaiLlmCompletionChunk
{
  FoundryLlmCompletionChunk parent_instance;
  JsonNode *node;
};

enum {
  PROP_0,
  PROP_NODE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PluginOpenaiLlmCompletionChunk, plugin_openai_llm_completion_chunk, FOUNDRY_TYPE_LLM_COMPLETION_CHUNK)

static GParamSpec *properties[N_PROPS];

static char *
plugin_openai_llm_completion_chunk_dup_text (FoundryLlmCompletionChunk *chunk)
{
  PluginOpenaiLlmCompletionChunk *self = (PluginOpenaiLlmCompletionChunk *)chunk;

  g_assert (PLUGIN_IS_OPENAI_LLM_COMPLETION_CHUNK (self));

  if (self->node == NULL)
    return NULL;

  if (JSON_NODE_HOLDS_OBJECT (self->node))
    {
      JsonObject *obj = json_node_get_object (self->node);
      JsonNode *choices;
      JsonArray *choices_ar;
      JsonNode *choice;
      JsonObject *choice_obj;
      JsonNode *delta;
      JsonObject *delta_obj;
      JsonNode *content;

      /* OpenAI format: {choices: [{delta: {content: "..."}}]} */
      if (json_object_has_member (obj, "choices") &&
          (choices = json_object_get_member (obj, "choices")) &&
          JSON_NODE_HOLDS_ARRAY (choices) &&
          (choices_ar = json_node_get_array (choices)) &&
          json_array_get_length (choices_ar) > 0 &&
          (choice = json_array_get_element (choices_ar, 0)) &&
          JSON_NODE_HOLDS_OBJECT (choice) &&
          (choice_obj = json_node_get_object (choice)) &&
          json_object_has_member (choice_obj, "delta") &&
          (delta = json_object_get_member (choice_obj, "delta")) &&
          JSON_NODE_HOLDS_OBJECT (delta) &&
          (delta_obj = json_node_get_object (delta)) &&
          json_object_has_member (delta_obj, "content") &&
          (content = json_object_get_member (delta_obj, "content")) &&
          json_node_get_value_type (content) == G_TYPE_STRING)
        return g_strdup (json_node_get_string (content));
    }

  return NULL;
}

static gboolean
plugin_openai_llm_completion_chunk_is_done (FoundryLlmCompletionChunk *chunk)
{
  PluginOpenaiLlmCompletionChunk *self = (PluginOpenaiLlmCompletionChunk *)chunk;

  g_assert (PLUGIN_IS_OPENAI_LLM_COMPLETION_CHUNK (self));

  if (self->node == NULL)
    return TRUE;

  if (JSON_NODE_HOLDS_OBJECT (self->node))
    {
      JsonObject *obj = json_node_get_object (self->node);
      JsonNode *choices;
      JsonArray *choices_ar;
      JsonNode *choice;
      JsonObject *choice_obj;
      JsonNode *finish_reason;

      /* Check for finish_reason */
      if (json_object_has_member (obj, "choices") &&
          (choices = json_object_get_member (obj, "choices")) &&
          JSON_NODE_HOLDS_ARRAY (choices) &&
          (choices_ar = json_node_get_array (choices)) &&
          json_array_get_length (choices_ar) > 0 &&
          (choice = json_array_get_element (choices_ar, 0)) &&
          JSON_NODE_HOLDS_OBJECT (choice) &&
          (choice_obj = json_node_get_object (choice)) &&
          json_object_has_member (choice_obj, "finish_reason") &&
          (finish_reason = json_object_get_member (choice_obj, "finish_reason")) &&
          json_node_get_value_type (finish_reason) == G_TYPE_STRING)
        {
          const char *reason = json_node_get_string (finish_reason);
          return reason != NULL && reason[0] != '\0';
        }
    }

  return FALSE;
}

static void
plugin_openai_llm_completion_chunk_finalize (GObject *object)
{
  PluginOpenaiLlmCompletionChunk *self = (PluginOpenaiLlmCompletionChunk *)object;

  g_clear_pointer (&self->node, json_node_unref);

  G_OBJECT_CLASS (plugin_openai_llm_completion_chunk_parent_class)->finalize (object);
}

static void
plugin_openai_llm_completion_chunk_get_property (GObject    *object,
                                                 guint       prop_id,
                                                 GValue     *value,
                                                 GParamSpec *pspec)
{
  PluginOpenaiLlmCompletionChunk *self = PLUGIN_OPENAI_LLM_COMPLETION_CHUNK (object);

  switch (prop_id)
    {
    case PROP_NODE:
      g_value_set_boxed (value, self->node);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_openai_llm_completion_chunk_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec)
{
  PluginOpenaiLlmCompletionChunk *self = PLUGIN_OPENAI_LLM_COMPLETION_CHUNK (object);

  switch (prop_id)
    {
    case PROP_NODE:
      self->node = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_openai_llm_completion_chunk_class_init (PluginOpenaiLlmCompletionChunkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLlmCompletionChunkClass *chunk_class = FOUNDRY_LLM_COMPLETION_CHUNK_CLASS (klass);

  object_class->finalize = plugin_openai_llm_completion_chunk_finalize;
  object_class->get_property = plugin_openai_llm_completion_chunk_get_property;
  object_class->set_property = plugin_openai_llm_completion_chunk_set_property;

  chunk_class->dup_text = plugin_openai_llm_completion_chunk_dup_text;
  chunk_class->is_done = plugin_openai_llm_completion_chunk_is_done;

  properties[PROP_NODE] =
    g_param_spec_boxed ("node", NULL, NULL,
                        JSON_TYPE_NODE,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_openai_llm_completion_chunk_init (PluginOpenaiLlmCompletionChunk *self)
{
}

PluginOpenaiLlmCompletionChunk *
plugin_openai_llm_completion_chunk_new (JsonNode *node)
{
  return g_object_new (PLUGIN_TYPE_OPENAI_LLM_COMPLETION_CHUNK,
                       "node", node,
                       NULL);
}
