/* foundry-json-llm-resource.c
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

#include "foundry-json.h"
#include "foundry-json-llm-resource.h"

struct _FoundryJsonLlmResource
{
  FoundryLlmResource parent_instance;
  JsonNode *node;
  char *name;
  char *uri;
  char *description;
};

G_DEFINE_FINAL_TYPE (FoundryJsonLlmResource, foundry_json_llm_resource, FOUNDRY_TYPE_LLM_RESOURCE)

enum {
  PROP_0,
  PROP_NODE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static char *
foundry_json_llm_resource_dup_name (FoundryLlmResource *resource)
{
  return g_strdup (FOUNDRY_JSON_LLM_RESOURCE (resource)->name);
}

static char *
foundry_json_llm_resource_dup_uri (FoundryLlmResource *resource)
{
  return g_strdup (FOUNDRY_JSON_LLM_RESOURCE (resource)->uri);
}

static char *
foundry_json_llm_resource_dup_description (FoundryLlmResource *resource)
{
  return g_strdup (FOUNDRY_JSON_LLM_RESOURCE (resource)->description);
}

static char *
foundry_json_llm_resource_dup_content_type (FoundryLlmResource *resource)
{
  return g_strdup ("application/json");
}

static DexFuture *
foundry_json_llm_resource_load_bytes (FoundryLlmResource *resource)
{
  FoundryJsonLlmResource *self = FOUNDRY_JSON_LLM_RESOURCE (resource);

  /* Ideally the caller would try to get things as JSON rather
   * than as the serialized bytes. But we can provide it nonetheless.
   */

  if (self->node == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "No JSON data to serialize");

  return foundry_json_node_to_bytes (self->node);
}

static DexFuture *
foundry_json_llm_resource_load_json (FoundryLlmResource *resource)
{
  FoundryJsonLlmResource *self = FOUNDRY_JSON_LLM_RESOURCE (resource);

  if (self->node == NULL)
    return dex_future_new_take_boxed (JSON_TYPE_NODE, json_node_new (JSON_NODE_NULL));

  return dex_future_new_take_boxed (JSON_TYPE_NODE, json_node_ref (self->node));
}

static void
foundry_json_llm_resource_finalize (GObject *object)
{
  FoundryJsonLlmResource *self = FOUNDRY_JSON_LLM_RESOURCE (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->uri, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->node, json_node_unref);

  G_OBJECT_CLASS (foundry_json_llm_resource_parent_class)->finalize (object);
}

static void
foundry_json_llm_resource_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  FoundryJsonLlmResource *self = FOUNDRY_JSON_LLM_RESOURCE (object);

  switch (prop_id)
    {
    case PROP_NODE:
      g_value_set_boxed (value, foundry_json_llm_resource_get_node (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_json_llm_resource_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  FoundryJsonLlmResource *self = FOUNDRY_JSON_LLM_RESOURCE (object);

  switch (prop_id)
    {
    case PROP_NODE:
      foundry_json_llm_resource_take_node (self, g_value_dup_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_json_llm_resource_class_init (FoundryJsonLlmResourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLlmResourceClass *resource_class = FOUNDRY_LLM_RESOURCE_CLASS (klass);

  object_class->finalize = foundry_json_llm_resource_finalize;
  object_class->get_property = foundry_json_llm_resource_get_property;
  object_class->set_property = foundry_json_llm_resource_set_property;

  resource_class->dup_name = foundry_json_llm_resource_dup_name;
  resource_class->dup_uri = foundry_json_llm_resource_dup_uri;
  resource_class->dup_description = foundry_json_llm_resource_dup_description;
  resource_class->dup_content_type = foundry_json_llm_resource_dup_content_type;
  resource_class->load_bytes = foundry_json_llm_resource_load_bytes;
  resource_class->load_json = foundry_json_llm_resource_load_json;

  properties[PROP_NODE] =
    g_param_spec_boxed ("node", NULL, NULL,
                        JSON_TYPE_NODE,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_json_llm_resource_init (FoundryJsonLlmResource *self)
{
}

FoundryLlmResource *
foundry_json_llm_resource_new (const char *name,
                               const char *uri,
                               const char *description,
                               JsonNode   *node)
{
  FoundryJsonLlmResource *self;

  g_return_val_if_fail (uri != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_JSON_LLM_RESOURCE, NULL);
  self->name = g_strdup (name);
  self->uri = g_strdup (uri);
  self->description = g_strdup (description);
  self->node = node ? json_node_ref (node) : NULL;

  return FOUNDRY_LLM_RESOURCE (self);
}

/**
 * foundry_json_llm_resource_get_node:
 * @self: a [class@Foundry.JsonLlmResource]
 *
 * Returns: (transfer none) (nullable):
 *
 * Since: 1.1
 */
JsonNode *
foundry_json_llm_resource_get_node (FoundryJsonLlmResource *self)
{
  g_return_val_if_fail (FOUNDRY_IS_JSON_LLM_RESOURCE (self), NULL);

  return self->node;
}

/**
 * foundry_json_llm_resource_set_node:
 * @self: a [class@Foundry.JsonLlmResource]
 * @node: (nullable):
 *
 * Since: 1.1
 */
void
foundry_json_llm_resource_set_node (FoundryJsonLlmResource *self,
                                    JsonNode               *node)
{
  g_return_if_fail (FOUNDRY_IS_JSON_LLM_RESOURCE (self));

  if (node != NULL)
    json_node_ref (node);

  foundry_json_llm_resource_take_node (self, node);
}

/**
 * foundry_json_llm_resource_take_node:
 * @self: a [class@Foundry.JsonLlmResource]
 * @node: (transfer full) (nullable):
 *
 * Since: 1.1
 */
void
foundry_json_llm_resource_take_node (FoundryJsonLlmResource *self,
                                     JsonNode               *node)
{
  g_autoptr(JsonNode) hold = node;

  g_return_if_fail (FOUNDRY_IS_JSON_LLM_RESOURCE (self));

  if (self->node == node)
    return;

  g_clear_pointer (&self->node, json_node_unref);
  self->node = g_steal_pointer (&hold);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NODE]);

  foundry_llm_resource_emit_changed (FOUNDRY_LLM_RESOURCE (self));
}
