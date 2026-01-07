/* plugin-openai-llm-tool-call.c
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

#include "plugin-openai-llm-tool-call.h"

struct _PluginOpenaiLlmToolCall
{
  FoundryLlmToolCall  parent_instance;

  FoundryLlmTool     *tool;
  JsonNode           *arguments;

  guint               is_callable : 1;
};

G_DEFINE_FINAL_TYPE (PluginOpenaiLlmToolCall, plugin_openai_llm_tool_call, FOUNDRY_TYPE_LLM_TOOL_CALL)

static char *
plugin_openai_llm_tool_call_dup_title (FoundryLlmToolCall *tool_call)
{
  PluginOpenaiLlmToolCall *self = PLUGIN_OPENAI_LLM_TOOL_CALL (tool_call);

  return foundry_llm_tool_dup_name (self->tool);
}

static gboolean
plugin_openai_llm_tool_call_is_callable (FoundryLlmToolCall *tool_call)
{
  PluginOpenaiLlmToolCall *self = PLUGIN_OPENAI_LLM_TOOL_CALL (tool_call);

  return self->is_callable;
}

static gboolean
marshal_param (GValue     *value,
               GParamSpec *pspec,
               JsonNode   *arguments)
{
  g_auto(GValue) src_value = G_VALUE_INIT;
  JsonObject *obj;
  JsonNode *node;
  GType type;

  if (!JSON_NODE_HOLDS_OBJECT (arguments))
    return FALSE;

  if (!(obj = json_node_get_object (arguments)))
    return FALSE;

  if (!(node = json_object_get_member (obj, pspec->name)))
    return FALSE;

  type = json_node_get_value_type (node);

  if (type == G_VALUE_TYPE (value))
    {
      json_node_get_value (node, value);
      return TRUE;
    }

  if (!g_value_type_transformable (type, G_VALUE_TYPE (value)))
    return FALSE;

  g_value_init (&src_value, type);
  json_node_get_value (node, &src_value);
  g_value_transform (&src_value, value);

  return TRUE;
}

static DexFuture *
plugin_openai_llm_tool_call_confirm (FoundryLlmToolCall *call)
{
  PluginOpenaiLlmToolCall *self = (PluginOpenaiLlmToolCall *)call;
  g_autofree GParamSpec **params = NULL;
  g_autoptr(GArray) values = NULL;
  guint n_params;

  dex_return_error_if_fail (PLUGIN_IS_OPENAI_LLM_TOOL_CALL (self));
  dex_return_error_if_fail (FOUNDRY_IS_LLM_TOOL (self->tool));
  dex_return_error_if_fail (self->is_callable);

  self->is_callable = FALSE;
  g_object_notify (G_OBJECT (self), "is-callable");

  params = foundry_llm_tool_list_parameters (self->tool, &n_params);
  values = g_array_new (FALSE, TRUE, sizeof (GValue));
  g_array_set_clear_func (values, (GDestroyNotify) g_value_unset);
  g_array_set_size (values, n_params);

  for (guint i = 0; i < n_params; i++)
    {
      GParamSpec *pspec = params[i];
      GValue *value = &g_array_index (values, GValue, i);

      g_value_init (value, pspec->value_type);

      if (!marshal_param (value, pspec, self->arguments))
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_INVALID_DATA,
                                      "Invalid param `%s`", pspec->name);
    }

  return foundry_llm_tool_call (self->tool,
                                &g_array_index (values, GValue, 0),
                                values->len);
}

static DexFuture *
plugin_openai_llm_tool_call_deny (FoundryLlmToolCall *call)
{
  PluginOpenaiLlmToolCall *self = (PluginOpenaiLlmToolCall *)call;

  dex_return_error_if_fail (PLUGIN_IS_OPENAI_LLM_TOOL_CALL (self));

  if (self->is_callable)
    {
      self->is_callable = FALSE;
      g_object_notify (G_OBJECT (self), "is-callable");
    }

  return dex_future_new_take_object (foundry_simple_llm_message_new (g_strdup ("user"),
                                                                     g_strdup ("You may not call that tool")));
}

static void
plugin_openai_llm_tool_call_finalize (GObject *object)
{
  PluginOpenaiLlmToolCall *self = (PluginOpenaiLlmToolCall *)object;

  g_clear_object (&self->tool);
  g_clear_pointer (&self->arguments, json_node_unref);

  G_OBJECT_CLASS (plugin_openai_llm_tool_call_parent_class)->finalize (object);
}

static void
plugin_openai_llm_tool_call_class_init (PluginOpenaiLlmToolCallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLlmToolCallClass *tool_call_class = FOUNDRY_LLM_TOOL_CALL_CLASS (klass);

  object_class->finalize = plugin_openai_llm_tool_call_finalize;

  tool_call_class->dup_title = plugin_openai_llm_tool_call_dup_title;
  tool_call_class->is_callable = plugin_openai_llm_tool_call_is_callable;
  tool_call_class->confirm = plugin_openai_llm_tool_call_confirm;
  tool_call_class->deny = plugin_openai_llm_tool_call_deny;
}

static void
plugin_openai_llm_tool_call_init (PluginOpenaiLlmToolCall *self)
{
}

FoundryLlmToolCall *
plugin_openai_llm_tool_call_new (FoundryLlmTool *tool,
                                 JsonNode       *arguments)
{
  PluginOpenaiLlmToolCall *self;

  g_return_val_if_fail (FOUNDRY_IS_LLM_TOOL (tool), NULL);
  g_return_val_if_fail (arguments != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_OPENAI_LLM_TOOL_CALL, NULL);
  self->tool = g_object_ref (tool);
  self->arguments = json_node_ref (arguments);
  self->is_callable = TRUE;

  return FOUNDRY_LLM_TOOL_CALL (self);
}
