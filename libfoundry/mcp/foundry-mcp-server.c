/* foundry-mcp-server.c
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
#include "foundry-json-node.h"
#include "foundry-jsonrpc-driver-private.h"
#include "foundry-llm-manager.h"
#include "foundry-llm-message.h"
#include "foundry-llm-resource.h"
#include "foundry-llm-tool.h"
#include "foundry-mcp-server.h"
#include "foundry-model-manager.h"
#include "foundry-util.h"

struct _FoundryMcpServer
{
  FoundryContextual     parent_instance;
  FoundryJsonrpcDriver *driver;
  GListModel           *tools;
  GListModel           *resources;
  GHashTable           *subscribed_resources;
  guint                 started : 1;
  guint                 got_initialize : 1;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryMcpServer, foundry_mcp_server, FOUNDRY_TYPE_CONTEXTUAL)

typedef struct
{
  FoundryJsonrpcDriver *driver;
  JsonNode *id;
} PropagateError;

static PropagateError *
propagate_error_new (FoundryJsonrpcDriver *driver,
                     JsonNode             *id)
{
  PropagateError *state = g_new0 (PropagateError, 1);

  state->driver = g_object_ref (driver);
  state->id = json_node_ref (id);

  return state;
}

static void
propagate_error_free (PropagateError *state)
{
  g_clear_object (&state->driver);
  g_clear_pointer (&state->id, json_node_unref);
  g_free (state);
}

static DexFuture *
propagate_error (DexFuture *failure,
                 gpointer   user_data)
{
  PropagateError *state = user_data;
  g_autoptr(GError) error = NULL;

  if (dex_future_get_value (failure, &error))
    return NULL;

  return foundry_jsonrpc_driver_reply_with_error (state->driver, state->id, -1, error->message);
}

static void
foundry_mcp_server_resource_changed (FoundryMcpServer   *self,
                                     FoundryLlmResource *resource)
{
  g_autofree char *uri = NULL;

  g_assert (FOUNDRY_IS_MCP_SERVER (self));
  g_assert (FOUNDRY_IS_LLM_RESOURCE (resource));

  if (!self->started)
    return;

  uri = foundry_llm_resource_dup_uri (resource);

  if (uri != NULL)
    {
      g_autoptr(JsonNode) params = NULL;

      params = FOUNDRY_JSON_OBJECT_NEW ("uri", FOUNDRY_JSON_NODE_PUT_STRING (uri));

      dex_future_disown (foundry_jsonrpc_driver_notify (self->driver,
                                                        "notifications/resources/updated",
                                                        params));
    }
}

static void
foundry_mcp_server_resources_changed (FoundryMcpServer *self,
                                      guint             position,
                                      guint             removed,
                                      guint             added,
                                      GListModel       *model)
{
  g_assert (FOUNDRY_IS_MCP_SERVER (self));
  g_assert (G_IS_LIST_MODEL (model));

  if (!self->started || !self->got_initialize)
    return;

  dex_future_disown (foundry_jsonrpc_driver_notify (self->driver,
                                                    "notifications/resources/list_changed",
                                                    NULL));
}

static DexFuture *
foundry_mcp_server_handle_method_call_fiber (FoundryMcpServer     *self,
                                             const char           *method,
                                             JsonNode             *params,
                                             JsonNode             *id,
                                             FoundryJsonrpcDriver *driver)
{
  g_autoptr(FoundryLlmManager) llm_manager = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(JsonNode) result = NULL;

  g_assert (FOUNDRY_IS_MCP_SERVER (self));
  g_assert (FOUNDRY_IS_JSONRPC_DRIVER (driver));
  g_assert (method != NULL);

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  llm_manager = foundry_context_dup_llm_manager (context);

  if (foundry_str_equal0 (method, "initialize"))
    {
      self->got_initialize = TRUE;
      result = FOUNDRY_JSON_OBJECT_NEW ("protocolVersion", "2024-11-05",
                                        "capabilities", "{",
                                          "tools", "{",
                                            "list", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                            "call", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                          "}",
                                          "resources", "{",
                                            "list", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                            "listChanged", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                            "read", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                            "subscribe", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                          "}",
                                          "prompts", "{",
                                            "list", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                            "get", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                          "}",
                                        "}",
                                        "serverInfo", "{",
                                          "name", "foundry",
                                          "version", "1.0.0",
                                        "}");
    }
  else if (foundry_str_equal0 (method, "tools/list"))
    {
      g_autoptr(GListModel) tools = NULL;
      g_autoptr(JsonArray) tools_ar = json_array_new ();
      g_autoptr(JsonNode) tools_node = json_node_new (JSON_NODE_ARRAY);
      guint n_items;

      if (!(tools = dex_await_object (foundry_llm_manager_list_tools (llm_manager), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      dex_await (foundry_list_model_await (tools), NULL);

      n_items = g_list_model_get_n_items (tools);

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(FoundryLlmTool) tool = g_list_model_get_item (tools, i);
          g_autofree char *name = foundry_llm_tool_dup_name (tool);
          g_autofree char *desc = foundry_llm_tool_dup_description (tool);
          g_autoptr(JsonNode) properties_node = json_node_new (JSON_NODE_OBJECT);
          g_autoptr(JsonObject) properties_obj = json_object_new ();
          g_autofree GParamSpec **pspecs = NULL;
          guint n_pspecs;

          if ((pspecs = foundry_llm_tool_list_parameters (tool, &n_pspecs)))
            {
              for (guint j = 0; j < n_pspecs; j++)
                {
                  GParamSpec *pspec = pspecs[j];
                  const char *pdesc = g_param_spec_get_blurb (pspec);
                  JsonNode *param_node;

                  if (pdesc == NULL)
                    pdesc = "";

                  if (G_IS_PARAM_SPEC_STRING (pspec))
                    param_node = FOUNDRY_JSON_OBJECT_NEW ("type", "string",
                                                          "description", FOUNDRY_JSON_NODE_PUT_STRING (pdesc));
                  else if (G_IS_PARAM_SPEC_INT (pspec) ||
                           G_IS_PARAM_SPEC_UINT (pspec) ||
                           G_IS_PARAM_SPEC_FLOAT (pspec) ||
                           G_IS_PARAM_SPEC_DOUBLE (pspec) ||
                           G_IS_PARAM_SPEC_INT64 (pspec) ||
                           G_IS_PARAM_SPEC_UINT64 (pspec))
                    param_node = FOUNDRY_JSON_OBJECT_NEW ("type", "number",
                                                          "description", FOUNDRY_JSON_NODE_PUT_STRING (pdesc));
                  else if (G_IS_PARAM_SPEC_BOOLEAN (pspec))
                    param_node = FOUNDRY_JSON_OBJECT_NEW ("type", "boolean",
                                                          "description", FOUNDRY_JSON_NODE_PUT_STRING (pdesc));
                  else
                    param_node = FOUNDRY_JSON_OBJECT_NEW ("description", FOUNDRY_JSON_NODE_PUT_STRING (pdesc));

                  json_object_set_member (properties_obj, pspec->name, param_node);
                }
            }

          json_node_set_object (properties_node, properties_obj);
          json_array_add_element (tools_ar,
                                  FOUNDRY_JSON_OBJECT_NEW ("name", FOUNDRY_JSON_NODE_PUT_STRING (name),
                                                           "description", FOUNDRY_JSON_NODE_PUT_STRING (desc),
                                                           "inputSchema", "{",
                                                             "type", "object",
                                                             "properties", FOUNDRY_JSON_NODE_PUT_NODE (properties_node),
                                                           "}"));
        }

      json_node_set_array (tools_node, tools_ar);

      result = FOUNDRY_JSON_OBJECT_NEW ("tools", FOUNDRY_JSON_NODE_PUT_NODE (tools_node));
    }
  else if (foundry_str_equal0 (method, "tools/call"))
    {
      const char *name = NULL;
      JsonNode *arguments = NULL;
      g_autoptr(GListModel) tools = NULL;
      g_autoptr(FoundryLlmTool) tool = NULL;
      g_autoptr(FoundryLlmMessage) message = NULL;
      g_autofree char *content = NULL;
      guint n_items;

      if (!FOUNDRY_JSON_OBJECT_PARSE (params,
                                       "name", FOUNDRY_JSON_NODE_GET_STRING (&name),
                                       "arguments", FOUNDRY_JSON_NODE_GET_NODE (&arguments)))
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_INVALID_DATA,
                                      "Invalid params for tools/call");

      if (!(tools = dex_await_object (foundry_llm_manager_list_tools (llm_manager), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      dex_await (foundry_list_model_await (tools), NULL);

      n_items = g_list_model_get_n_items (tools);

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(FoundryLlmTool) candidate = g_list_model_get_item (tools, i);
          g_autofree char *tool_name = foundry_llm_tool_dup_name (candidate);

          if (g_strcmp0 (tool_name, name) == 0)
            {
              tool = g_steal_pointer (&candidate);
              break;
            }
        }

      if (tool == NULL)
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_NOT_FOUND,
                                      "No such tool `%s`", name);

      {
        g_autofree GParamSpec **params_pspecs = NULL;
        g_autoptr(GArray) values = NULL;
        guint n_params;
        g_auto(GValue) src_value = G_VALUE_INIT;
        JsonObject *obj;
        JsonNode *node;

        if (!JSON_NODE_HOLDS_OBJECT (arguments))
          return dex_future_new_reject (G_IO_ERROR,
                                        G_IO_ERROR_INVALID_DATA,
                                        "Arguments must be an object");

        if (!(obj = json_node_get_object (arguments)))
          return dex_future_new_reject (G_IO_ERROR,
                                        G_IO_ERROR_INVALID_DATA,
                                        "Invalid arguments object");

        params_pspecs = foundry_llm_tool_list_parameters (tool, &n_params);
        values = g_array_new (FALSE, TRUE, sizeof (GValue));
        g_array_set_clear_func (values, (GDestroyNotify) g_value_unset);
        g_array_set_size (values, n_params);

        for (guint i = 0; i < n_params; i++)
          {
            GParamSpec *pspec = params_pspecs[i];
            GValue *value = &g_array_index (values, GValue, i);
            GType type;

            g_value_init (value, pspec->value_type);

            if (!(node = json_object_get_member (obj, pspec->name)))
              return dex_future_new_reject (G_IO_ERROR,
                                            G_IO_ERROR_INVALID_DATA,
                                            "Missing param `%s`", pspec->name);

            type = json_node_get_value_type (node);

            if (type == G_VALUE_TYPE (value))
              {
                json_node_get_value (node, value);
              }
            else if (g_value_type_transformable (type, G_VALUE_TYPE (value)))
              {
                g_value_init (&src_value, type);
                json_node_get_value (node, &src_value);
                g_value_transform (&src_value, value);
                g_value_unset (&src_value);
              }
            else
              {
                return dex_future_new_reject (G_IO_ERROR,
                                              G_IO_ERROR_INVALID_DATA,
                                              "Invalid param `%s`", pspec->name);
              }
          }

        if (!(message = dex_await_object (foundry_llm_tool_call (tool,
                                                                 &g_array_index (values, GValue, 0),
                                                                 values->len),
                                          &error)))
          return dex_future_new_for_error (g_steal_pointer (&error));
      }

      content = foundry_llm_message_dup_content (message);

      result = FOUNDRY_JSON_OBJECT_NEW ("content", "[",
                                         "{",
                                           "type", "text",
                                           "text", FOUNDRY_JSON_NODE_PUT_STRING (content),
                                         "}",
                                       "]");
    }
  else if (foundry_str_equal0 (method, "resources/list"))
    {
      g_autoptr(JsonArray) resources_ar = json_array_new ();
      g_autoptr(JsonNode) resources_node = json_node_new (JSON_NODE_ARRAY);
      guint n_items;

      if (self->resources == NULL)
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_FAILED,
                                      "Resources not available");

      dex_await (foundry_list_model_await (self->resources), NULL);

      n_items = g_list_model_get_n_items (self->resources);

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(FoundryLlmResource) resource = g_list_model_get_item (self->resources, i);
          g_autofree char *uri = foundry_llm_resource_dup_uri (resource);
          g_autofree char *name = foundry_llm_resource_dup_name (resource);
          g_autofree char *description = foundry_llm_resource_dup_description (resource);
          g_autofree char *content_type = foundry_llm_resource_dup_content_type (resource);
          g_autoptr(JsonObject) resource_obj = json_object_new ();

          if (uri != NULL)
            json_object_set_string_member (resource_obj, "uri", uri);

          if (name != NULL)
            json_object_set_string_member (resource_obj, "name", name);

          if (description != NULL)
            json_object_set_string_member (resource_obj, "description", description);

          if (content_type != NULL)
            {
              g_autofree char *mime_type = g_content_type_get_mime_type (content_type);

              if (mime_type == NULL)
                json_object_set_string_member (resource_obj, "mimeType", content_type);
              else
                json_object_set_string_member (resource_obj, "mimeType", mime_type);
            }

          {
            g_autoptr(JsonNode) resource_node = json_node_new (JSON_NODE_OBJECT);
            json_node_set_object (resource_node, resource_obj);
            json_array_add_element (resources_ar, g_steal_pointer (&resource_node));
          }
        }

      json_node_set_array (resources_node, resources_ar);

      result = FOUNDRY_JSON_OBJECT_NEW ("resources", FOUNDRY_JSON_NODE_PUT_NODE (resources_node));
    }
  else if (foundry_str_equal0 (method, "resources/read"))
    {
      const char *uri = NULL;
      g_autoptr(FoundryLlmResource) resource = NULL;
      g_autoptr(GBytes) bytes = NULL;
      g_autofree char *content_type = NULL;
      g_autofree char *mime_type = NULL;
      gboolean is_text = FALSE;

      if (!FOUNDRY_JSON_OBJECT_PARSE (params,
                                       "uri", FOUNDRY_JSON_NODE_GET_STRING (&uri)))
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_INVALID_DATA,
                                      "Invalid params for resources/read");

      if (!(resource = dex_await_object (foundry_llm_manager_find_resource (llm_manager, uri), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!(bytes = dex_await_object (foundry_llm_resource_load_bytes (resource), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      content_type = foundry_llm_resource_dup_content_type (resource);

      if (content_type != NULL)
        {
          mime_type = g_content_type_get_mime_type (content_type);
          if (mime_type == NULL)
            mime_type = g_strdup (content_type);

          if (mime_type != NULL)
            {
              if (g_str_has_prefix (mime_type, "text/"))
                is_text = TRUE;
              else if (g_str_equal (mime_type, "application/json") ||
                       g_str_equal (mime_type, "application/xml") ||
                       g_str_equal (mime_type, "application/javascript") ||
                       g_str_equal (mime_type, "application/x-javascript"))
                is_text = TRUE;
            }
        }

      if (is_text)
        {
          gsize size;
          const guchar *data = g_bytes_get_data (bytes, &size);
          g_autofree char *text = g_utf8_make_valid ((const char *)data, size);
          g_autoptr(JsonNode) content_node = json_node_new (JSON_NODE_OBJECT);
          g_autoptr(JsonObject) content_obj = json_object_new ();
          g_autoptr(JsonArray) contents_ar = json_array_new ();
          g_autoptr(JsonNode) contents_node = json_node_new (JSON_NODE_ARRAY);

          json_object_set_string_member (content_obj, "type", "text");
          json_object_set_string_member (content_obj, "text", text);
          json_node_set_object (content_node, content_obj);
          json_array_add_element (contents_ar, g_steal_pointer (&content_node));
          json_node_set_array (contents_node, contents_ar);

          result = FOUNDRY_JSON_OBJECT_NEW ("contents", FOUNDRY_JSON_NODE_PUT_NODE (contents_node));
        }
      else
        {
          gsize size;
          const guchar *data = g_bytes_get_data (bytes, &size);
          g_autofree char *base64 = g_base64_encode (data, size);
          g_autoptr(JsonNode) content_node = json_node_new (JSON_NODE_OBJECT);
          g_autoptr(JsonObject) content_obj = json_object_new ();
          g_autoptr(JsonArray) contents_ar = json_array_new ();
          g_autoptr(JsonNode) contents_node = json_node_new (JSON_NODE_ARRAY);

          json_object_set_string_member (content_obj, "type", "blob");
          json_object_set_string_member (content_obj, "blob", base64);
          if (mime_type != NULL)
            json_object_set_string_member (content_obj, "mimeType", mime_type);
          json_node_set_object (content_node, content_obj);
          json_array_add_element (contents_ar, g_steal_pointer (&content_node));
          json_node_set_array (contents_node, contents_ar);

          result = FOUNDRY_JSON_OBJECT_NEW ("contents", FOUNDRY_JSON_NODE_PUT_NODE (contents_node));
        }
    }
  else if (foundry_str_equal0 (method, "resources/subscribe"))
    {
      const char *uri = NULL;

      if (!FOUNDRY_JSON_OBJECT_PARSE (params,
                                       "uri", FOUNDRY_JSON_NODE_GET_STRING (&uri)))
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_INVALID_DATA,
                                      "Invalid params for resources/subscribe");

      if (!g_hash_table_contains (self->subscribed_resources, uri))
        {
          g_autoptr(FoundryLlmResource) resource = NULL;

          if (!(resource = dex_await_object (foundry_llm_manager_find_resource (llm_manager, uri), &error)))
            return dex_future_new_for_error (g_steal_pointer (&error));

          g_hash_table_insert (self->subscribed_resources,
                               g_strdup (uri),
                               g_object_ref (resource));

          g_signal_connect_object (resource,
                                   "changed",
                                   G_CALLBACK (foundry_mcp_server_resource_changed),
                                   self,
                                   G_CONNECT_SWAPPED);
        }

      result = json_node_new (JSON_NODE_OBJECT);
      json_node_take_object (result, json_object_new ());
    }
  else if (foundry_str_equal0 (method, "prompts/list"))
    {
      result = FOUNDRY_JSON_OBJECT_NEW ("prompts", "[",
                                        "]");
    }

  if (result != NULL)
    return foundry_jsonrpc_driver_reply (driver, id, result);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_FAILED,
                                "No such method `%s`", method);
}

static gboolean
foundry_mcp_server_handle_method_call (FoundryMcpServer     *self,
                                       const char           *method,
                                       JsonNode             *params,
                                       JsonNode             *id,
                                       FoundryJsonrpcDriver *driver)
{
  g_assert (FOUNDRY_IS_MCP_SERVER (self));
  g_assert (FOUNDRY_IS_JSONRPC_DRIVER (driver));
  g_assert (method != NULL);

  dex_future_disown (dex_future_catch (foundry_scheduler_spawn (NULL, 0,
                                                                G_CALLBACK (foundry_mcp_server_handle_method_call_fiber),
                                                                5,
                                                                FOUNDRY_TYPE_MCP_SERVER, self,
                                                                G_TYPE_STRING, method,
                                                                JSON_TYPE_NODE, params,
                                                                JSON_TYPE_NODE, id,
                                                                FOUNDRY_TYPE_JSONRPC_DRIVER, driver),
                                       propagate_error,
                                       propagate_error_new (driver, id),
                                       (GDestroyNotify) propagate_error_free));

  return TRUE;
}

static void
foundry_mcp_server_subscribed_resource_destroy (gpointer data)
{
  FoundryLlmResource *resource = data;

  g_assert (FOUNDRY_IS_LLM_RESOURCE (resource));

  g_signal_handlers_disconnect_by_func (resource,
                                        G_CALLBACK (foundry_mcp_server_resource_changed),
                                        NULL);

  g_object_unref (resource);
}

static DexFuture *
foundry_mcp_server_load_resources_fiber (gpointer data)
{
  FoundryMcpServer *self = data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryLlmManager) llm_manager = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GListModel) resources = NULL;

  g_assert (FOUNDRY_IS_MCP_SERVER (self));

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (self), &error)))
    {
      g_warning ("Failed to acquire context: %s", error->message);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  llm_manager = foundry_context_dup_llm_manager (context);

  if (llm_manager == NULL)
    {
      g_warning ("Failed to get LlmManager");
      return dex_future_new_reject (G_IO_ERROR,
                                     G_IO_ERROR_FAILED,
                                     "Failed to get LlmManager");
    }

  if (!(resources = dex_await_object (foundry_llm_manager_list_resources (llm_manager), &error)))
    {
      g_warning ("Failed to list resources: %s", error->message);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  self->resources = g_object_ref (resources);

  g_signal_connect_object (self->resources,
                           "items-changed",
                           G_CALLBACK (foundry_mcp_server_resources_changed),
                           self,
                           G_CONNECT_SWAPPED);

  return dex_future_new_true ();
}

static void
foundry_mcp_server_constructed (GObject *object)
{
  FoundryMcpServer *self = (FoundryMcpServer *)object;

  G_OBJECT_CLASS (foundry_mcp_server_parent_class)->constructed (object);

  dex_future_disown (dex_scheduler_spawn (NULL, 0,
                                          foundry_mcp_server_load_resources_fiber,
                                          g_object_ref (self),
                                          g_object_unref));
}

static void
foundry_mcp_server_dispose (GObject *object)
{
  FoundryMcpServer *self = (FoundryMcpServer *)object;

  g_clear_pointer (&self->subscribed_resources, g_hash_table_unref);
  g_clear_object (&self->resources);
  g_clear_object (&self->driver);

  G_OBJECT_CLASS (foundry_mcp_server_parent_class)->dispose (object);
}

static void
foundry_mcp_server_class_init (FoundryMcpServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = foundry_mcp_server_constructed;
  object_class->dispose = foundry_mcp_server_dispose;
}

static void
foundry_mcp_server_init (FoundryMcpServer *self)
{
  self->subscribed_resources = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       foundry_mcp_server_subscribed_resource_destroy);
}

FoundryMcpServer *
foundry_mcp_server_new (FoundryContext *context,
                        GIOStream      *stream)
{
  FoundryMcpServer *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (G_IS_IO_STREAM (stream), NULL);

  self = g_object_new (FOUNDRY_TYPE_MCP_SERVER,
                       "context", context,
                       NULL);
  self->driver = foundry_jsonrpc_driver_new (stream, FOUNDRY_JSONRPC_STYLE_LF);

  g_signal_connect_object (self->driver,
                           "handle-method-call",
                           G_CALLBACK (foundry_mcp_server_handle_method_call),
                           self,
                           G_CONNECT_SWAPPED);

  return self;
}

void
foundry_mcp_server_start (FoundryMcpServer *self)
{
  g_return_if_fail (FOUNDRY_IS_MCP_SERVER (self));
  g_return_if_fail (self->driver != NULL);
  g_return_if_fail (self->started == FALSE);

  self->started = TRUE;

  foundry_jsonrpc_driver_start (self->driver);
}

void
foundry_mcp_server_stop (FoundryMcpServer *self)
{
  g_return_if_fail (FOUNDRY_IS_MCP_SERVER (self));
  g_return_if_fail (self->driver != NULL);
  g_return_if_fail (self->started);

  self->started = FALSE;

  g_hash_table_remove_all (self->subscribed_resources);

  foundry_jsonrpc_driver_stop (self->driver);
}
