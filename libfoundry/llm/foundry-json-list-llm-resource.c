/* foundry-json-list-llm-resource.c
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

#include "foundry-json.h"
#include "foundry-json-list-llm-resource.h"

struct _FoundryJsonListLlmResource
{
  FoundryLlmResource parent_instance;
  GListModel *model;
  char *name;
  char *uri;
  char *description;
};

G_DEFINE_FINAL_TYPE (FoundryJsonListLlmResource, foundry_json_list_llm_resource, FOUNDRY_TYPE_LLM_RESOURCE)

static char *
foundry_json_list_llm_resource_dup_name (FoundryLlmResource *resource)
{
  return g_strdup (FOUNDRY_JSON_LIST_LLM_RESOURCE (resource)->name);
}

static char *
foundry_json_list_llm_resource_dup_uri (FoundryLlmResource *resource)
{
  return g_strdup (FOUNDRY_JSON_LIST_LLM_RESOURCE (resource)->uri);
}

static char *
foundry_json_list_llm_resource_dup_description (FoundryLlmResource *resource)
{
  return g_strdup (FOUNDRY_JSON_LIST_LLM_RESOURCE (resource)->description);
}

static char *
foundry_json_list_llm_resource_dup_content_type (FoundryLlmResource *resource)
{
  return g_strdup ("application/json");
}

static JsonNode *
serialize_item (gpointer item)
{
  JsonNode *node = NULL;

  if (item == NULL || !(node = json_gobject_serialize (G_OBJECT (item))))
    return json_node_new (JSON_NODE_NULL);

  return node;
}

static DexFuture *
foundry_json_list_llm_resource_load_json (FoundryLlmResource *resource)
{
  FoundryJsonListLlmResource *self = FOUNDRY_JSON_LIST_LLM_RESOURCE (resource);
  g_autoptr(JsonArray) array = NULL;
  g_autoptr(JsonNode) node = NULL;
  guint n_items;

  if (self->model == NULL)
    return dex_future_new_take_boxed (JSON_TYPE_NODE, json_node_new (JSON_NODE_NULL));

  n_items = g_list_model_get_n_items (self->model);
  array = json_array_new ();
  node = json_node_new (JSON_NODE_ARRAY);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GObject) item = g_list_model_get_item (self->model, i);

      json_array_add_element (array, serialize_item (item));
    }

  json_node_set_array (node, array);

  return dex_future_new_take_boxed (JSON_TYPE_NODE, g_steal_pointer (&node));
}

static DexFuture *
foundry_json_list_llm_resource_load_bytes_cb (DexFuture *future,
                                              gpointer   user_data)
{
  g_autoptr(JsonNode) node = dex_await_boxed (dex_ref (future), NULL);

  return foundry_json_node_to_bytes (node);
}

static DexFuture *
foundry_json_list_llm_resource_load_bytes (FoundryLlmResource *resource)
{
  return dex_future_then (foundry_json_list_llm_resource_load_json (resource),
                          foundry_json_list_llm_resource_load_bytes_cb,
                          NULL, NULL);
}

static void
foundry_json_list_llm_resource_items_changed (FoundryJsonListLlmResource *self,
                                              guint                       position,
                                              guint                       removed,
                                              guint                       added)
{
  foundry_llm_resource_emit_changed (FOUNDRY_LLM_RESOURCE (self));
}

static void
foundry_json_list_llm_resource_finalize (GObject *object)
{
  FoundryJsonListLlmResource *self = FOUNDRY_JSON_LIST_LLM_RESOURCE (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->uri, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_object (&self->model);

  G_OBJECT_CLASS (foundry_json_list_llm_resource_parent_class)->finalize (object);
}

static void
foundry_json_list_llm_resource_class_init (FoundryJsonListLlmResourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLlmResourceClass *resource_class = FOUNDRY_LLM_RESOURCE_CLASS (klass);

  object_class->finalize = foundry_json_list_llm_resource_finalize;

  resource_class->dup_name = foundry_json_list_llm_resource_dup_name;
  resource_class->dup_uri = foundry_json_list_llm_resource_dup_uri;
  resource_class->dup_description = foundry_json_list_llm_resource_dup_description;
  resource_class->dup_content_type = foundry_json_list_llm_resource_dup_content_type;
  resource_class->load_bytes = foundry_json_list_llm_resource_load_bytes;
  resource_class->load_json = foundry_json_list_llm_resource_load_json;
}

static void
foundry_json_list_llm_resource_init (FoundryJsonListLlmResource *self)
{
}

/**
 * foundry_json_list_llm_resource_new:
 *
 * Createa a new LlmResource that will monitor changes to @model and
 * update the contents of the resource.
 *
 * It is expected that elements within this model can be serialized
 * using Json-GLib gobject serialization.
 *
 * Returns: (transfer full):
 *
 * Since: 1.1
 */
FoundryLlmResource *
foundry_json_list_llm_resource_new (const char *name,
                                    const char *uri,
                                    const char *description,
                                    GListModel *model)
{
  FoundryJsonListLlmResource *self;

  g_return_val_if_fail (uri != NULL, NULL);
  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  self = g_object_new (FOUNDRY_TYPE_JSON_LIST_LLM_RESOURCE, NULL);
  self->name = g_strdup (name);
  self->uri = g_strdup (uri);
  self->description = g_strdup (description);
  self->model = g_object_ref (model);

  g_signal_connect_object (model,
                           "items-changed",
                           G_CALLBACK (foundry_json_list_llm_resource_items_changed),
                           self,
                           G_CONNECT_SWAPPED);

  return FOUNDRY_LLM_RESOURCE (self);
}
