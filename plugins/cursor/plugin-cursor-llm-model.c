/* plugin-cursor-llm-model.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "plugin-cursor-client.h"
#include "plugin-cursor-llm-model.h"

struct _PluginCursorLlmModel
{
  FoundryLlmModel     parent_instance;
  PluginCursorClient *client;
  JsonNode           *node;
};

G_DEFINE_FINAL_TYPE (PluginCursorLlmModel, plugin_cursor_llm_model, FOUNDRY_TYPE_LLM_MODEL)

static char *
plugin_cursor_llm_model_dup_name (FoundryLlmModel *model)
{
  PluginCursorLlmModel *self = (PluginCursorLlmModel *)model;

  g_assert (PLUGIN_IS_CURSOR_LLM_MODEL (self));

  if (JSON_NODE_HOLDS_VALUE (self->node) &&
      json_node_get_value_type (self->node) == G_TYPE_STRING)
    return g_strconcat ("cursor:", json_node_get_string (self->node), NULL);

  return NULL;
}

static char *
plugin_cursor_llm_model_dup_digest (FoundryLlmModel *model)
{
  g_autofree char *name = plugin_cursor_llm_model_dup_name (model);
  g_autoptr(GChecksum) sum = g_checksum_new (G_CHECKSUM_SHA256);

  /* We don't get this from the remote API, so just synthesize it. */
  g_checksum_update (sum, (const guint8 *)name, strlen (name));

  return g_strdup (g_checksum_get_string (sum));
}

static DexFuture *
plugin_cursor_llm_model_complete (FoundryLlmModel    *model,
                                  const char * const *roles,
                                  const char * const *messages)
{
  return foundry_future_new_not_supported ();
}

static DexFuture *
plugin_cursor_llm_model_chat (FoundryLlmModel *model,
                             const char      *system)
{
  return foundry_future_new_not_supported ();
}

static gboolean
plugin_cursor_llm_model_is_metered (FoundryLlmModel *model)
{
  return TRUE;
}

static void
plugin_cursor_llm_model_finalize (GObject *object)
{
  PluginCursorLlmModel *self = (PluginCursorLlmModel *)object;

  g_clear_object (&self->client);
  g_clear_pointer (&self->node, json_node_unref);

  G_OBJECT_CLASS (plugin_cursor_llm_model_parent_class)->finalize (object);
}

static void
plugin_cursor_llm_model_class_init (PluginCursorLlmModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLlmModelClass *llm_model_class = FOUNDRY_LLM_MODEL_CLASS (klass);

  object_class->finalize = plugin_cursor_llm_model_finalize;

  llm_model_class->dup_name = plugin_cursor_llm_model_dup_name;
  llm_model_class->dup_digest = plugin_cursor_llm_model_dup_digest;
  llm_model_class->chat = plugin_cursor_llm_model_chat;
  llm_model_class->complete = plugin_cursor_llm_model_complete;
  llm_model_class->is_metered = plugin_cursor_llm_model_is_metered;
}

static void
plugin_cursor_llm_model_init (PluginCursorLlmModel *self)
{
}

PluginCursorLlmModel *
plugin_cursor_llm_model_new (FoundryContext     *context,
                             PluginCursorClient *client,
                             JsonNode           *node)
{
  PluginCursorLlmModel *self;

  g_return_val_if_fail (PLUGIN_IS_CURSOR_CLIENT (client), NULL);
  g_return_val_if_fail (node != NULL, NULL);

  if (!JSON_NODE_HOLDS_VALUE (node) ||
      json_node_get_value_type (node) != G_TYPE_STRING)
    return NULL;

  self = g_object_new (PLUGIN_TYPE_CURSOR_LLM_MODEL,
                       "context", context,
                       NULL);

  self->client = g_object_ref (client);
  self->node = json_node_ref (node);

  return g_steal_pointer (&self);
}
