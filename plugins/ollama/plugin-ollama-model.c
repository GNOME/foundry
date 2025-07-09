/* plugin-ollama-model.c
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

#include "plugin-ollama-model.h"

struct _PluginOllamaModel
{
  GObject   parent_instance;
  JsonNode *node;
};

G_DEFINE_FINAL_TYPE (PluginOllamaModel, plugin_ollama_model, G_TYPE_OBJECT)

static void
plugin_ollama_model_finalize (GObject *object)
{
  PluginOllamaModel *self = (PluginOllamaModel *)object;

  g_clear_pointer (&self->node, json_node_unref);

  G_OBJECT_CLASS (plugin_ollama_model_parent_class)->finalize (object);
}

static void
plugin_ollama_model_class_init (PluginOllamaModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_ollama_model_finalize;
}

static void
plugin_ollama_model_init (PluginOllamaModel *self)
{
}

PluginOllamaModel *
plugin_ollama_model_new (JsonNode *node)
{
  PluginOllamaModel *self;

  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (node), NULL);

  self = g_object_new (PLUGIN_TYPE_OLLAMA_MODEL, NULL);
  self->node = json_node_ref (node);

  return g_steal_pointer (&self);
}

char *
plugin_ollama_model_dup_name (PluginOllamaModel *self)
{
  JsonObject *obj;
  JsonNode *name;

  g_return_val_if_fail (PLUGIN_IS_OLLAMA_MODEL (self), NULL);

  if ((obj = json_node_get_object (self->node)) &&
      json_object_has_member (obj, "name") &&
      (name = json_object_get_member (obj, "name")) &&
      json_node_get_value_type (name) == G_TYPE_STRING)
    return g_strdup (json_node_get_string (name));

  return NULL;
}
