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
#include "foundry-llm-tool.h"
#include "foundry-mcp-server.h"
#include "foundry-model-manager.h"
#include "foundry-util.h"

struct _FoundryMcpServer
{
  FoundryContextual     parent_instance;
  FoundryJsonrpcDriver *driver;
  GListModel           *tools;
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
      result = FOUNDRY_JSON_OBJECT_NEW ("protocolVersion", "2024-11-05",
                                        "capabilities", "{",
                                          "tools", "{",
                                            "list", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                            "call", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                          "}",
                                          "resources", "{",
                                            "list", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
                                            "read", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
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
  else if (foundry_str_equal0 (method, "resources/list"))
    {
      result = FOUNDRY_JSON_OBJECT_NEW ("resources", "[",
                                        "]");
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
foundry_mcp_server_dispose (GObject *object)
{
  FoundryMcpServer *self = (FoundryMcpServer *)object;

  g_clear_object (&self->driver);

  G_OBJECT_CLASS (foundry_mcp_server_parent_class)->dispose (object);
}

static void
foundry_mcp_server_class_init (FoundryMcpServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_mcp_server_dispose;
}

static void
foundry_mcp_server_init (FoundryMcpServer *self)
{
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

  foundry_jsonrpc_driver_start (self->driver);
}

void
foundry_mcp_server_stop (FoundryMcpServer *self)
{
  g_return_if_fail (FOUNDRY_IS_MCP_SERVER (self));
  g_return_if_fail (self->driver != NULL);

  foundry_jsonrpc_driver_stop (self->driver);
}
