/* foundry-acp-connection.c
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-acp-agent.h"
#include "foundry-acp-client.h"
#include "foundry-acp-connection-private.h"
#include "foundry-acp-enums.h"
#include "foundry-acp-event-private.h"
#include "foundry-acp-permission-request-private.h"
#include "foundry-acp-permission-response-private.h"
#include "foundry-acp-project-client.h"
#include "foundry-acp-session-private.h"
#include "foundry-acp-session-update-private.h"
#include "foundry-acp-terminal-private.h"
#include "foundry-acp-terminal-output-private.h"
#include "foundry-build-pipeline.h"
#include "foundry-context.h"
#include "foundry-json-node.h"
#include "foundry-jsonrpc-driver-private.h"
#include "foundry-util.h"

struct _FoundryAcpConnection
{
  GObject parent_instance;
  FoundryContext *context;
  FoundryAcpAgent *agent;
  FoundryAcpClient *client;
  FoundryBuildPipeline *pipeline;
  FoundryJsonrpcDriver *driver;
  GHashTable *sessions;
  char *agent_name;
  char *agent_title;
  char *agent_version;
  FoundryAcpConnectionState state;
  FoundryAcpClientCapabilityFlags client_capabilities;
  guint protocol_version;
  guint started : 1;
};

G_DEFINE_FINAL_TYPE (FoundryAcpConnection, foundry_acp_connection, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_STATE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

typedef struct _ReplyState
{
  FoundryJsonrpcDriver *driver;
  JsonNode *id;
} ReplyState;

typedef struct _StrvState
{
  char **argv;
  char **environ;
} StrvState;

typedef struct _TerminalCreateState
{
  FoundryAcpSession *session;
  char **argv;
  char *turn_id;
  char *command;
  char *cwd;
  gssize output_byte_limit;
} TerminalCreateState;

typedef struct _TerminalMethodState
{
  FoundryAcpSession *session;
  FoundryAcpTerminal *terminal;
  FoundryAcpProjectClient *project_client;
  char *terminal_id;
  char *method;
} TerminalMethodState;

static void
strv_state_free (StrvState *state)
{
  g_clear_pointer (&state->argv, g_strfreev);
  g_clear_pointer (&state->environ, g_strfreev);
  g_free (state);
}

static void
terminal_create_state_free (TerminalCreateState *state)
{
  if (state != NULL)
    {
      g_clear_object (&state->session);
      g_clear_pointer (&state->argv, g_strfreev);
      g_clear_pointer (&state->turn_id, g_free);
      g_clear_pointer (&state->command, g_free);
      g_clear_pointer (&state->cwd, g_free);
      g_free (state);
    }
}

static void
terminal_method_state_free (TerminalMethodState *state)
{
  if (state != NULL)
    {
      g_clear_object (&state->session);
      g_clear_object (&state->terminal);
      g_clear_object (&state->project_client);
      g_clear_pointer (&state->terminal_id, g_free);
      g_clear_pointer (&state->method, g_free);
      g_free (state);
    }
}

static void
reply_state_free (ReplyState *state)
{
  g_clear_object (&state->driver);
  g_clear_pointer (&state->id, json_node_unref);
  g_free (state);
}

static ReplyState *
reply_state_new (FoundryJsonrpcDriver *driver,
                 JsonNode             *id)
{
  ReplyState *state;

  g_assert (FOUNDRY_IS_JSONRPC_DRIVER (driver));
  g_assert (id != NULL);

  state = g_new0 (ReplyState, 1);
  state->driver = g_object_ref (driver);
  state->id = json_node_ref (id);

  return state;
}

static void
foundry_acp_connection_set_state (FoundryAcpConnection      *self,
                                  FoundryAcpConnectionState  state)
{
  g_assert (FOUNDRY_IS_ACP_CONNECTION (self));

  if (self->state != state)
    {
      self->state = state;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
    }
}

static JsonNode *
json_node_new_empty_object (void)
{
  JsonNode *node;
  JsonObject *object;

  node = json_node_new (JSON_NODE_OBJECT);
  object = json_object_new ();
  json_node_take_object (node, object);

  return node;
}

static gboolean
json_node_object (JsonNode    *node,
                  JsonObject **object)
{
  if (node == NULL || !JSON_NODE_HOLDS_OBJECT (node))
    return FALSE;

  *object = json_node_get_object (node);

  return *object != NULL;
}

static const char *
json_object_get_string_member_or_null (JsonObject *object,
                                       const char *member)
{
  JsonNode *node;

  g_assert (object != NULL);
  g_assert (member != NULL);

  if (!(node = json_object_get_member (object, member)))
    return NULL;

  if (!JSON_NODE_HOLDS_VALUE (node) || json_node_get_value_type (node) != G_TYPE_STRING)
    return NULL;

  return json_node_get_string (node);
}

static gboolean
json_object_get_uint_member_or_default (JsonObject *object,
                                        const char *member,
                                        guint      *value)
{
  JsonNode *node;
  gint64 v;

  g_assert (object != NULL);
  g_assert (member != NULL);
  g_assert (value != NULL);

  if (!(node = json_object_get_member (object, member)))
    return TRUE;

  if (!JSON_NODE_HOLDS_VALUE (node) || json_node_get_value_type (node) != G_TYPE_INT64)
    return FALSE;

  v = json_node_get_int (node);

  if (v < 0 || v > G_MAXUINT)
    return FALSE;

  *value = v;

  return TRUE;
}

static gboolean
json_object_get_int64_member_or_default (JsonObject *object,
                                         const char *member,
                                         gint64     *value)
{
  JsonNode *node;

  g_assert (object != NULL);
  g_assert (member != NULL);
  g_assert (value != NULL);

  if (!(node = json_object_get_member (object, member)))
    return TRUE;

  if (!JSON_NODE_HOLDS_VALUE (node) || json_node_get_value_type (node) != G_TYPE_INT64)
    return FALSE;

  *value = json_node_get_int (node);

  return TRUE;
}

static char **
json_object_dup_strv_member (JsonObject *object,
                             const char *member)
{
  JsonNode *node;
  JsonArray *array;
  GPtrArray *strv;
  guint length;

  g_assert (object != NULL);
  g_assert (member != NULL);

  if (!(node = json_object_get_member (object, member)))
    return NULL;

  if (!JSON_NODE_HOLDS_ARRAY (node) || !(array = json_node_get_array (node)))
    return NULL;

  strv = g_ptr_array_new_with_free_func (g_free);
  length = json_array_get_length (array);

  for (guint i = 0; i < length; i++)
    {
      JsonNode *element = json_array_get_element (array, i);

      if (!JSON_NODE_HOLDS_VALUE (element) ||
          json_node_get_value_type (element) != G_TYPE_STRING)
        {
          g_ptr_array_unref (strv);
          return NULL;
        }

      g_ptr_array_add (strv, g_strdup (json_node_get_string (element)));
    }

  g_ptr_array_add (strv, NULL);

  return (char **)g_ptr_array_free (strv, FALSE);
}

static char **
json_object_dup_env_member (JsonObject *object,
                            const char *member)
{
  JsonNode *node;
  JsonArray *array;
  GPtrArray *strv;
  guint length;

  g_assert (object != NULL);
  g_assert (member != NULL);

  if (!(node = json_object_get_member (object, member)))
    return NULL;

  if (!JSON_NODE_HOLDS_ARRAY (node) || !(array = json_node_get_array (node)))
    return NULL;

  strv = g_ptr_array_new_with_free_func (g_free);
  length = json_array_get_length (array);

  for (guint i = 0; i < length; i++)
    {
      JsonNode *element = json_array_get_element (array, i);

      if (JSON_NODE_HOLDS_VALUE (element) &&
          json_node_get_value_type (element) == G_TYPE_STRING)
        {
          g_ptr_array_add (strv, g_strdup (json_node_get_string (element)));
          continue;
        }

      if (JSON_NODE_HOLDS_OBJECT (element))
        {
          JsonObject *env = json_node_get_object (element);
          const char *name = json_object_get_string_member_or_null (env, "name");
          const char *value = json_object_get_string_member_or_null (env, "value");

          if (name != NULL && value != NULL)
            {
              g_ptr_array_add (strv, g_strdup_printf ("%s=%s", name, value));
              continue;
            }
        }

      g_ptr_array_unref (strv);
      return NULL;
    }

  g_ptr_array_add (strv, NULL);

  return (char **)g_ptr_array_free (strv, FALSE);
}

static int
foundry_acp_connection_error_to_jsonrpc (const GError *error)
{
  if (error == NULL)
    return FOUNDRY_ACP_ERROR_INTERNAL;

  if (error->domain == FOUNDRY_ACP_ERROR)
    {
      switch (error->code)
        {
        case FOUNDRY_ACP_ERROR_PARSE:
        case FOUNDRY_ACP_ERROR_INVALID_REQUEST:
        case FOUNDRY_ACP_ERROR_METHOD_NOT_FOUND:
        case FOUNDRY_ACP_ERROR_INVALID_PARAMS:
        case FOUNDRY_ACP_ERROR_INTERNAL:
        case FOUNDRY_ACP_ERROR_AUTH_REQUIRED:
        case FOUNDRY_ACP_ERROR_RESOURCE_NOT_FOUND:
          return error->code;

        default:
          break;
        }
    }

  if (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_FOUND)
    return FOUNDRY_ACP_ERROR_RESOURCE_NOT_FOUND;

  if (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_INVALID_DATA)
    return FOUNDRY_ACP_ERROR_INVALID_PARAMS;

  return FOUNDRY_ACP_ERROR_INTERNAL;
}

static DexFuture *
reply_success (DexFuture *completed,
               gpointer   user_data)
{
  ReplyState *state = user_data;
  const GValue *value;
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (state != NULL);

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!G_VALUE_HOLDS (value, JSON_TYPE_NODE))
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INTERNAL,
                                  "ACP reply is not a JSON node");

  reply = json_node_ref (g_value_get_boxed (value));

  return foundry_jsonrpc_driver_reply (state->driver, state->id, reply);
}

static DexFuture *
reply_failure (DexFuture *completed,
               gpointer   user_data)
{
  ReplyState *state = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (state != NULL);

  if (dex_future_get_value (completed, &error))
    return NULL;

  return foundry_jsonrpc_driver_reply_with_error (state->driver,
                                                 state->id,
                                                 foundry_acp_connection_error_to_jsonrpc (error),
                                                 error->message);
}

static DexFuture *
read_text_file_complete (DexFuture *completed,
                         gpointer   user_data)
{
  const GValue *value;
  const char *content;
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (completed));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!G_VALUE_HOLDS_STRING (value))
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INTERNAL,
                                  "File content is not a string");

  content = g_value_get_string (value);

  if (!g_utf8_validate (content, -1, NULL))
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INVALID_PARAMS,
                                  "File content is not valid UTF-8");

  reply = FOUNDRY_JSON_OBJECT_NEW ("content", FOUNDRY_JSON_NODE_PUT_STRING (content));

  return dex_future_new_take_boxed (JSON_TYPE_NODE, g_steal_pointer (&reply));
}

static DexFuture *
empty_object_complete (DexFuture *completed,
                       gpointer   user_data)
{
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (completed));

  if (!dex_future_get_value (completed, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  reply = json_node_new_empty_object ();

  return dex_future_new_take_boxed (JSON_TYPE_NODE, g_steal_pointer (&reply));
}

static DexFuture *
permission_complete (DexFuture *completed,
                     gpointer   user_data)
{
  const GValue *value;
  g_autoptr(FoundryAcpPermissionResponse) response = NULL;
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (completed));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!G_VALUE_HOLDS_OBJECT (value) ||
      !FOUNDRY_IS_ACP_PERMISSION_RESPONSE (g_value_get_object (value)))
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INTERNAL,
                                  "Permission response has unexpected type");

  response = g_object_ref (g_value_get_object (value));
  reply = _foundry_acp_permission_response_to_json (response);

  return dex_future_new_take_boxed (JSON_TYPE_NODE, g_steal_pointer (&reply));
}

static DexFuture *
terminal_create_complete (DexFuture *completed,
                          gpointer   user_data)
{
  TerminalCreateState *state = user_data;
  const GValue *value;
  g_autoptr(FoundryAcpTerminal) terminal = NULL;
  g_autofree char *terminal_id = NULL;
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (completed));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!G_VALUE_HOLDS_OBJECT (value) ||
      !FOUNDRY_IS_ACP_TERMINAL (g_value_get_object (value)))
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INTERNAL,
                                  "Terminal response has unexpected type");

  terminal = g_object_ref (g_value_get_object (value));
  terminal_id = foundry_acp_terminal_dup_id (terminal);

  if (terminal_id == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INTERNAL,
                                  "Terminal did not provide an identifier");

  _foundry_acp_terminal_set_turn_id (terminal, state->turn_id);
  _foundry_acp_terminal_set_command (terminal, state->command);
  _foundry_acp_terminal_set_argv (terminal, (const char * const *)state->argv);
  _foundry_acp_terminal_set_cwd (terminal, state->cwd);
  _foundry_acp_terminal_set_output_byte_limit (terminal, state->output_byte_limit);
  _foundry_acp_terminal_set_started_at (terminal, g_get_real_time ());
  _foundry_acp_terminal_set_state (terminal, FOUNDRY_ACP_TERMINAL_RUNNING);
  _foundry_acp_terminal_clear_exit_status (terminal);
  _foundry_acp_session_add_terminal (state->session, terminal);

  reply = FOUNDRY_JSON_OBJECT_NEW ("terminalId", FOUNDRY_JSON_NODE_PUT_STRING (terminal_id));

  return dex_future_new_take_boxed (JSON_TYPE_NODE, g_steal_pointer (&reply));
}

static DexFuture *
terminal_output_complete (DexFuture *completed,
                          gpointer   user_data)
{
  TerminalMethodState *state = user_data;
  const GValue *value;
  g_autoptr(FoundryAcpTerminalOutput) output = NULL;
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (completed));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!G_VALUE_HOLDS_OBJECT (value) ||
      !FOUNDRY_IS_ACP_TERMINAL_OUTPUT (g_value_get_object (value)))
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INTERNAL,
                                  "Terminal output has unexpected type");

  output = g_object_ref (g_value_get_object (value));
  reply = _foundry_acp_terminal_output_to_json (output);

  if (state->terminal != NULL)
    _foundry_acp_terminal_apply_output (state->terminal, output);

  if (state->project_client != NULL &&
      foundry_acp_terminal_output_has_exit_status (output))
    dex_await (foundry_acp_project_client_refresh_changed_files (state->project_client,
                                                                 state->session),
               NULL);

  return dex_future_new_take_boxed (JSON_TYPE_NODE, g_steal_pointer (&reply));
}

static DexFuture *
event_complete (DexFuture *completed,
                gpointer   user_data)
{
  FoundryAcpEvent *event = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (FOUNDRY_IS_ACP_EVENT (event));

  if (dex_future_get_value (completed, &error))
    {
      _foundry_acp_event_set_state (event, FOUNDRY_ACP_EVENT_COMPLETED);
      return dex_ref (completed);
    }

  _foundry_acp_event_set_state (event, FOUNDRY_ACP_EVENT_FAILED);
  _foundry_acp_event_set_body (event, error->message);

  return dex_future_new_for_error (g_steal_pointer (&error));
}

static void
file_event_set_content_metadata (FoundryAcpEvent *event,
                                 const char      *path,
                                 const char      *content)
{
  g_autofree char *body = NULL;
  g_autofree char *sha256 = NULL;
  g_autoptr(GChecksum) checksum = NULL;
  g_autoptr(JsonNode) raw = NULL;
  gsize len;

  g_assert (FOUNDRY_IS_ACP_EVENT (event));
  g_assert (path != NULL);
  g_assert (content != NULL);

  len = strlen (content);
  checksum = g_checksum_new (G_CHECKSUM_SHA256);
  g_checksum_update (checksum, (const guchar *)content, len);
  sha256 = g_strdup (g_checksum_get_string (checksum));
  body = g_strdup_printf ("%s\nBytes: %" G_GSIZE_FORMAT "\nSHA256: %s", path, len, sha256);
  raw = FOUNDRY_JSON_OBJECT_NEW ("path", FOUNDRY_JSON_NODE_PUT_STRING (path),
                                 "byteCount", FOUNDRY_JSON_NODE_PUT_INT (len),
                                 "sha256", FOUNDRY_JSON_NODE_PUT_STRING (sha256));

  _foundry_acp_event_set_body (event, body);
  _foundry_acp_event_set_raw (event, raw);
}

static DexFuture *
file_read_event_complete (DexFuture *completed,
                          gpointer   user_data)
{
  FoundryAcpEvent *event = user_data;
  const GValue *value;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (FOUNDRY_IS_ACP_EVENT (event));

  if ((value = dex_future_get_value (completed, &error)))
    {
      path = foundry_acp_event_dup_body (event);

      if (G_VALUE_HOLDS_STRING (value) && path != NULL)
        file_event_set_content_metadata (event, path, g_value_get_string (value));

      _foundry_acp_event_set_state (event, FOUNDRY_ACP_EVENT_COMPLETED);
      return dex_ref (completed);
    }

  _foundry_acp_event_set_state (event, FOUNDRY_ACP_EVENT_FAILED);
  _foundry_acp_event_set_body (event, error->message);

  return dex_future_new_for_error (g_steal_pointer (&error));
}

static DexFuture *
permission_response_event_complete (DexFuture *completed,
                                    gpointer   user_data)
{
  FoundryAcpSession *session = user_data;
  g_autoptr(FoundryAcpEvent) event = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *session_id = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));

  session_id = foundry_acp_session_dup_id (session);

  if (dex_future_get_value (completed, &error))
    {
      event = _foundry_acp_event_new_full (session_id,
                                           _foundry_acp_session_get_current_turn_id (session),
                                           NULL,
                                           NULL,
                                           FOUNDRY_ACP_EVENT_PERMISSION_RESPONSE,
                                           "permission-response",
                                           "Permission response",
                                           NULL,
                                           FOUNDRY_ACP_EVENT_COMPLETED,
                                           NULL);
      _foundry_acp_session_set_active_permission_request (session, NULL);
      _foundry_acp_session_append_event (session, event);
      return dex_ref (completed);
    }

  event = _foundry_acp_event_new_full (session_id,
                                       _foundry_acp_session_get_current_turn_id (session),
                                       NULL,
                                       NULL,
                                       FOUNDRY_ACP_EVENT_PERMISSION_RESPONSE,
                                       "permission-response",
                                       "Permission response failed",
                                       error->message,
                                       FOUNDRY_ACP_EVENT_FAILED,
                                       NULL);
  _foundry_acp_session_set_active_permission_request (session, NULL);
  _foundry_acp_session_append_event (session, event);

  return dex_future_new_for_error (g_steal_pointer (&error));
}

static DexFuture *
terminal_create_cleanup (DexFuture *completed,
                         gpointer   user_data)
{
  g_assert (DEX_IS_FUTURE (completed));

  return dex_ref (completed);
}

static gboolean
terminal_method_is_exit (const char *method)
{
  return g_strcmp0 (method, "terminal/wait_for_exit") == 0 ||
         g_strcmp0 (method, "terminal/kill") == 0;
}

static DexFuture *
terminal_method_complete (DexFuture *completed,
                          gpointer   user_data)
{
  TerminalMethodState *state = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (state != NULL);

  if (dex_future_get_value (completed, &error))
    {
      if (state->terminal != NULL)
        {
          if (g_strcmp0 (state->method, "terminal/wait_for_exit") == 0)
            {
              _foundry_acp_terminal_set_exited_at (state->terminal, g_get_real_time ());
              _foundry_acp_terminal_set_state (state->terminal, FOUNDRY_ACP_TERMINAL_EXITED);
            }
          else if (g_strcmp0 (state->method, "terminal/kill") == 0)
            {
              _foundry_acp_terminal_set_exited_at (state->terminal, g_get_real_time ());
              _foundry_acp_terminal_set_state (state->terminal, FOUNDRY_ACP_TERMINAL_CANCELLED);
            }
          else if (g_strcmp0 (state->method, "terminal/release") == 0)
            _foundry_acp_session_remove_terminal (state->session, state->terminal_id);
        }

      if (state->project_client != NULL && terminal_method_is_exit (state->method))
        dex_await (foundry_acp_project_client_refresh_changed_files (state->project_client,
                                                                     state->session),
                   NULL);

      return dex_ref (completed);
    }

  if (state->terminal != NULL)
    _foundry_acp_terminal_set_state (state->terminal, FOUNDRY_ACP_TERMINAL_FAILED);

  return dex_future_new_for_error (g_steal_pointer (&error));
}

static FoundryAcpSession *
lookup_session_from_params (FoundryAcpConnection  *self,
                            JsonObject            *object,
                            const char           **session_id)
{
  const char *id;

  g_assert (FOUNDRY_IS_ACP_CONNECTION (self));
  g_assert (object != NULL);
  g_assert (session_id != NULL);

  if (!(id = json_object_get_string_member_or_null (object, "sessionId")))
    return NULL;

  *session_id = id;

  return _foundry_acp_connection_lookup_session (self, id);
}

static DexFuture *
dispatch_read_text_file (FoundryAcpConnection *self,
                         JsonObject           *object)
{
  FoundryAcpSession *session;
  const char *session_id = NULL;
  const char *path;
  guint line = 0;
  guint limit = 0;
  DexFuture *future;

  g_assert (FOUNDRY_IS_ACP_CONNECTION (self));
  g_assert (object != NULL);

  session = lookup_session_from_params (self, object, &session_id);
  path = json_object_get_string_member_or_null (object, "path");

  if (session_id == NULL || path == NULL ||
      !json_object_get_uint_member_or_default (object, "line", &line) ||
      !json_object_get_uint_member_or_default (object, "limit", &limit))
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INVALID_PARAMS,
                                  "Invalid params for fs/read_text_file");

  if (session == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_RESOURCE_NOT_FOUND,
                                  "No such session `%s`", session_id);

  {
    g_autoptr(FoundryAcpEvent) event = NULL;
    g_autofree char *title = NULL;

    title = g_strdup_printf ("Read %s", path);
    event = _foundry_acp_event_new_full (session_id,
                                         _foundry_acp_session_get_current_turn_id (session),
                                         NULL,
                                         NULL,
                                         FOUNDRY_ACP_EVENT_FILE_READ,
                                         "fs/read_text_file",
                                         title,
                                         path,
                                         FOUNDRY_ACP_EVENT_RUNNING,
                                         NULL);
    _foundry_acp_session_append_event (session, event);

    future = foundry_acp_client_read_text_file (self->client, session, path, line, limit);
    future = dex_future_finally (future,
                                 file_read_event_complete,
                                 g_object_ref (event),
                                 g_object_unref);
  }

  return dex_future_then (future, read_text_file_complete, NULL, NULL);
}

static DexFuture *
dispatch_write_text_file (FoundryAcpConnection *self,
                          JsonObject           *object)
{
  FoundryAcpSession *session;
  const char *session_id = NULL;
  const char *path;
  const char *content;
  DexFuture *future;

  g_assert (FOUNDRY_IS_ACP_CONNECTION (self));
  g_assert (object != NULL);

  session = lookup_session_from_params (self, object, &session_id);
  path = json_object_get_string_member_or_null (object, "path");
  content = json_object_get_string_member_or_null (object, "content");

  if (session_id == NULL || path == NULL || content == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INVALID_PARAMS,
                                  "Invalid params for fs/write_text_file");

  if (session == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_RESOURCE_NOT_FOUND,
                                  "No such session `%s`", session_id);

  {
    g_autoptr(FoundryAcpEvent) event = NULL;
    g_autofree char *title = NULL;

    title = g_strdup_printf ("Write %s", path);
    event = _foundry_acp_event_new_full (session_id,
                                         _foundry_acp_session_get_current_turn_id (session),
                                         NULL,
                                         NULL,
                                         FOUNDRY_ACP_EVENT_FILE_WRITE,
                                         "fs/write_text_file",
                                         title,
                                         path,
                                         FOUNDRY_ACP_EVENT_RUNNING,
                                         NULL);
    file_event_set_content_metadata (event, path, content);
    _foundry_acp_session_append_event (session, event);

    future = foundry_acp_client_write_text_file (self->client, session, path, content);
    future = dex_future_finally (future,
                                 event_complete,
                                 g_object_ref (event),
                                 g_object_unref);
  }

  return dex_future_then (future, empty_object_complete, NULL, NULL);
}

static DexFuture *
dispatch_request_permission (FoundryAcpConnection *self,
                             JsonNode             *params,
                             JsonObject           *object)
{
  g_autoptr(FoundryAcpPermissionRequest) request = NULL;
  FoundryAcpSession *session;
  const char *session_id = NULL;
  DexFuture *future;

  g_assert (FOUNDRY_IS_ACP_CONNECTION (self));
  g_assert (params != NULL);
  g_assert (object != NULL);

  session = lookup_session_from_params (self, object, &session_id);

  if (session_id == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INVALID_PARAMS,
                                  "Invalid params for session/request_permission");

  if (session == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_RESOURCE_NOT_FOUND,
                                  "No such session `%s`", session_id);

  request = _foundry_acp_permission_request_new (session_id, params);

  {
    g_autoptr(FoundryAcpEvent) event = NULL;

    event = _foundry_acp_event_new_full (session_id,
                                         _foundry_acp_session_get_current_turn_id (session),
                                         NULL,
                                         NULL,
                                         FOUNDRY_ACP_EVENT_PERMISSION_REQUEST,
                                         "session/request_permission",
                                         "Permission request",
                                         NULL,
                                         FOUNDRY_ACP_EVENT_PENDING,
                                         params);
    _foundry_acp_session_set_active_permission_request (session, request);
    _foundry_acp_session_append_event (session, event);
  }

  future = foundry_acp_client_request_permission (self->client, session, request);
  future = dex_future_finally (future,
                               permission_response_event_complete,
                               g_object_ref (session),
                               g_object_unref);

  return dex_future_then (future, permission_complete, NULL, NULL);
}

static DexFuture *
dispatch_terminal_create (FoundryAcpConnection *self,
                          JsonObject           *object)
{
  FoundryAcpSession *session;
  TerminalCreateState *create_state;
  const char *session_id = NULL;
  const char *command;
  const char *cwd;
  StrvState *state;
  gint64 output_byte_limit = -1;
  DexFuture *future;

  g_assert (FOUNDRY_IS_ACP_CONNECTION (self));
  g_assert (object != NULL);

  state = g_new0 (StrvState, 1);
  create_state = NULL;
  session = lookup_session_from_params (self, object, &session_id);
  command = json_object_get_string_member_or_null (object, "command");
  cwd = json_object_get_string_member_or_null (object, "cwd");
  state->argv = json_object_dup_strv_member (object, "args");
  state->environ = json_object_dup_env_member (object, "env");

  if (session_id == NULL || command == NULL ||
      (json_object_has_member (object, "args") && state->argv == NULL) ||
      (json_object_has_member (object, "env") && state->environ == NULL) ||
      !json_object_get_int64_member_or_default (object, "outputByteLimit", &output_byte_limit))
    {
      strv_state_free (state);
      return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                    FOUNDRY_ACP_ERROR_INVALID_PARAMS,
                                    "Invalid params for terminal/create");
    }

  if (session == NULL)
    {
      strv_state_free (state);
      return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                    FOUNDRY_ACP_ERROR_RESOURCE_NOT_FOUND,
                                    "No such session `%s`", session_id);
    }

  {
    g_autoptr(FoundryAcpEvent) event = NULL;
    g_autofree char *title = NULL;

    title = g_strdup_printf ("Run %s", command);
    event = _foundry_acp_event_new_full (session_id,
                                         _foundry_acp_session_get_current_turn_id (session),
                                         NULL,
                                         NULL,
                                         FOUNDRY_ACP_EVENT_TERMINAL_CREATED,
                                         "terminal/create",
                                         title,
                                         cwd,
                                         FOUNDRY_ACP_EVENT_RUNNING,
                                         NULL);
    _foundry_acp_session_append_event (session, event);

    create_state = g_new0 (TerminalCreateState, 1);
    create_state->session = g_object_ref (session);
    create_state->argv = g_strdupv (state->argv);
    create_state->turn_id = g_strdup (_foundry_acp_session_get_current_turn_id (session));
    create_state->command = g_strdup (command);
    create_state->cwd = g_strdup (cwd);
    create_state->output_byte_limit = output_byte_limit;

    future = foundry_acp_client_create_terminal (self->client,
                                                 session,
                                                 command,
                                                 (const char * const *)state->argv,
                                                 cwd,
                                                 (const char * const *)state->environ,
                                                 output_byte_limit);
    future = dex_future_finally (future,
                                 event_complete,
                                 g_object_ref (event),
                                 g_object_unref);
  }
  future = dex_future_then (future,
                            terminal_create_complete,
                            create_state,
                            (GDestroyNotify)terminal_create_state_free);
  future = dex_future_finally (future,
                               terminal_create_cleanup,
                               state,
                               (GDestroyNotify)strv_state_free);

  return future;
}

static DexFuture *
dispatch_terminal_id_method (FoundryAcpConnection *self,
                             JsonObject           *object,
                             const char           *method)
{
  FoundryAcpSession *session;
  TerminalMethodState *state;
  const char *session_id = NULL;
  const char *terminal_id;
  DexFuture *future;

  g_assert (FOUNDRY_IS_ACP_CONNECTION (self));
  g_assert (object != NULL);
  g_assert (method != NULL);

  session = lookup_session_from_params (self, object, &session_id);
  terminal_id = json_object_get_string_member_or_null (object, "terminalId");

  if (session_id == NULL || terminal_id == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INVALID_PARAMS,
                                  "Invalid params for %s", method);

  if (session == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_RESOURCE_NOT_FOUND,
                                  "No such session `%s`", session_id);

  state = g_new0 (TerminalMethodState, 1);
  state->session = g_object_ref (session);
  state->terminal = _foundry_acp_session_lookup_terminal (session, terminal_id);
  state->terminal_id = g_strdup (terminal_id);
  state->method = g_strdup (method);

  if (FOUNDRY_IS_ACP_PROJECT_CLIENT (self->client))
    state->project_client = g_object_ref (FOUNDRY_ACP_PROJECT_CLIENT (self->client));

  if (g_strcmp0 (method, "terminal/output") == 0)
    {
      g_autoptr(FoundryAcpEvent) event = NULL;
      g_autofree char *title = NULL;

      title = g_strdup_printf ("Terminal output %s", terminal_id);
      event = _foundry_acp_event_new_full (session_id,
                                           _foundry_acp_session_get_current_turn_id (session),
                                           NULL,
                                           NULL,
                                           FOUNDRY_ACP_EVENT_TERMINAL_OUTPUT,
                                           "terminal/output",
                                           title,
                                           terminal_id,
                                           FOUNDRY_ACP_EVENT_COMPLETED,
                                           NULL);
      _foundry_acp_session_append_event (session, event);

      future = foundry_acp_client_terminal_output (self->client, session, terminal_id);
      return dex_future_then (future,
                              terminal_output_complete,
                              state,
                              (GDestroyNotify)terminal_method_state_free);
    }

  if (g_strcmp0 (method, "terminal/wait_for_exit") == 0)
    {
      g_autoptr(FoundryAcpEvent) event = NULL;
      g_autofree char *title = NULL;

      title = g_strdup_printf ("Terminal exited %s", terminal_id);
      event = _foundry_acp_event_new_full (session_id,
                                           _foundry_acp_session_get_current_turn_id (session),
                                           NULL,
                                           NULL,
                                           FOUNDRY_ACP_EVENT_TERMINAL_EXITED,
                                           "terminal/wait_for_exit",
                                           title,
                                           terminal_id,
                                           FOUNDRY_ACP_EVENT_COMPLETED,
                                           NULL);
      _foundry_acp_session_append_event (session, event);
      future = foundry_acp_client_terminal_wait_for_exit (self->client, session, terminal_id);
    }
  else if (g_strcmp0 (method, "terminal/kill") == 0)
    {
      g_autoptr(FoundryAcpEvent) event = NULL;
      g_autofree char *title = NULL;

      title = g_strdup_printf ("Kill terminal %s", terminal_id);
      event = _foundry_acp_event_new_full (session_id,
                                           _foundry_acp_session_get_current_turn_id (session),
                                           NULL,
                                           NULL,
                                           FOUNDRY_ACP_EVENT_TERMINAL_EXITED,
                                           "terminal/kill",
                                           title,
                                           terminal_id,
                                           FOUNDRY_ACP_EVENT_CANCELLED,
                                           NULL);
      _foundry_acp_session_append_event (session, event);
      future = foundry_acp_client_terminal_kill (self->client, session, terminal_id);
    }
  else
    {
      g_autoptr(FoundryAcpEvent) event = NULL;
      g_autofree char *title = NULL;

      title = g_strdup_printf ("Release terminal %s", terminal_id);
      event = _foundry_acp_event_new_full (session_id,
                                           _foundry_acp_session_get_current_turn_id (session),
                                           NULL,
                                           NULL,
                                           FOUNDRY_ACP_EVENT_TERMINAL_RELEASED,
                                           "terminal/release",
                                           title,
                                           terminal_id,
                                           FOUNDRY_ACP_EVENT_COMPLETED,
                                           NULL);
      _foundry_acp_session_append_event (session, event);
      future = foundry_acp_client_terminal_release (self->client, session, terminal_id);
    }

  future = dex_future_finally (future,
                               terminal_method_complete,
                               state,
                               (GDestroyNotify)terminal_method_state_free);

  return dex_future_then (future, empty_object_complete, NULL, NULL);
}

static DexFuture *
foundry_acp_connection_dispatch_method (FoundryAcpConnection *self,
                                        const char           *method,
                                        JsonNode             *params)
{
  JsonObject *object = NULL;

  g_assert (FOUNDRY_IS_ACP_CONNECTION (self));
  g_assert (method != NULL);

  if (self->client == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_METHOD_NOT_FOUND,
                                  "ACP client is not available");

  if (!json_node_object (params, &object))
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INVALID_PARAMS,
                                  "ACP request params must be an object");

  if (g_strcmp0 (method, "fs/read_text_file") == 0)
    return dispatch_read_text_file (self, object);

  if (g_strcmp0 (method, "fs/write_text_file") == 0)
    return dispatch_write_text_file (self, object);

  if (g_strcmp0 (method, "session/request_permission") == 0)
    return dispatch_request_permission (self, params, object);

  if (g_strcmp0 (method, "terminal/create") == 0)
    return dispatch_terminal_create (self, object);

  if (g_strcmp0 (method, "terminal/output") == 0 ||
      g_strcmp0 (method, "terminal/wait_for_exit") == 0 ||
      g_strcmp0 (method, "terminal/kill") == 0 ||
      g_strcmp0 (method, "terminal/release") == 0)
    return dispatch_terminal_id_method (self, object, method);

  return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                FOUNDRY_ACP_ERROR_METHOD_NOT_FOUND,
                                "Unknown ACP method `%s`", method);
}

static gboolean
foundry_acp_connection_handle_method_call (FoundryAcpConnection *self,
                                           const char           *method,
                                           JsonNode             *params,
                                           JsonNode             *id,
                                           FoundryJsonrpcDriver *driver)
{
  DexFuture *future;

  g_assert (FOUNDRY_IS_ACP_CONNECTION (self));
  g_assert (FOUNDRY_IS_JSONRPC_DRIVER (driver));
  g_assert (method != NULL);
  g_assert (id != NULL);

  future = foundry_acp_connection_dispatch_method (self, method, params);
  future = dex_future_then (future,
                            reply_success,
                            reply_state_new (driver, id),
                            (GDestroyNotify)reply_state_free);
  future = dex_future_catch (future,
                             reply_failure,
                             reply_state_new (driver, id),
                             (GDestroyNotify)reply_state_free);
  dex_future_disown (future);

  return TRUE;
}

static void
foundry_acp_connection_handle_session_update (FoundryAcpConnection *self,
                                              JsonNode             *params)
{
  g_autoptr(FoundryAcpSessionUpdate) update = NULL;
  g_autoptr(FoundryAcpSession) session_ref = NULL;
  const char *session_id = NULL;
  JsonObject *object = NULL;
  FoundryAcpSession *session;

  g_assert (FOUNDRY_IS_ACP_CONNECTION (self));

  if (self->client == NULL || !json_node_object (params, &object))
    return;

  session_id = json_object_get_string_member_or_null (object, "sessionId");

  if (session_id == NULL)
    return;

  if (!(session = _foundry_acp_connection_lookup_session (self, session_id)))
    return;

  session_ref = g_object_ref (session);
  update = _foundry_acp_session_update_new_from_json (session_id, params);
  _foundry_acp_session_append_update (session, update);

  dex_future_disown (foundry_acp_client_session_update (self->client, session_ref, update));
}

static void
foundry_acp_connection_handle_notification (FoundryAcpConnection *self,
                                            const char           *method,
                                            JsonNode             *params,
                                            FoundryJsonrpcDriver *driver)
{
  g_assert (FOUNDRY_IS_ACP_CONNECTION (self));
  g_assert (FOUNDRY_IS_JSONRPC_DRIVER (driver));
  g_assert (method != NULL);

  if (g_strcmp0 (method, "session/update") == 0)
    foundry_acp_connection_handle_session_update (self, params);
}

static void
foundry_acp_connection_connect_driver (FoundryAcpConnection *self)
{
  g_assert (FOUNDRY_IS_ACP_CONNECTION (self));
  g_assert (FOUNDRY_IS_JSONRPC_DRIVER (self->driver));

  g_signal_connect_object (self->driver,
                           "handle-method-call",
                           G_CALLBACK (foundry_acp_connection_handle_method_call),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->driver,
                           "handle-notification",
                           G_CALLBACK (foundry_acp_connection_handle_notification),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
foundry_acp_connection_dispose (GObject *object)
{
  FoundryAcpConnection *self = (FoundryAcpConnection *)object;

  if (self->driver != NULL)
    foundry_jsonrpc_driver_stop (self->driver);

  g_clear_object (&self->driver);
  g_clear_object (&self->context);
  g_clear_object (&self->agent);
  g_clear_object (&self->client);
  g_clear_object (&self->pipeline);
  g_clear_pointer (&self->sessions, g_hash_table_unref);
  g_clear_pointer (&self->agent_name, g_free);
  g_clear_pointer (&self->agent_title, g_free);
  g_clear_pointer (&self->agent_version, g_free);

  G_OBJECT_CLASS (foundry_acp_connection_parent_class)->dispose (object);
}

static void
foundry_acp_connection_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  FoundryAcpConnection *self = FOUNDRY_ACP_CONNECTION (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_enum (value, self->state);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_acp_connection_class_init (FoundryAcpConnectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_acp_connection_dispose;
  object_class->get_property = foundry_acp_connection_get_property;

  properties[PROP_STATE] =
    g_param_spec_enum ("state", NULL, NULL,
                       FOUNDRY_TYPE_ACP_CONNECTION_STATE,
                       FOUNDRY_ACP_CONNECTION_NEW,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_acp_connection_init (FoundryAcpConnection *self)
{
  self->sessions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->state = FOUNDRY_ACP_CONNECTION_NEW;
  self->protocol_version = FOUNDRY_ACP_PROTOCOL_VERSION;
}

/**
 * foundry_acp_connection_new:
 * @context: a [class@Foundry.Context]
 * @agent: a [class@Foundry.AcpAgent]
 * @client: a [iface@Foundry.AcpClient]
 *
 * Creates a connection object for @agent.
 *
 * Returns: (transfer full): a [class@Foundry.AcpConnection]
 *
 * Since: 1.2
 */
FoundryAcpConnection *
foundry_acp_connection_new (FoundryContext   *context,
                            FoundryAcpAgent  *agent,
                            FoundryAcpClient *client)
{
  FoundryAcpConnection *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (FOUNDRY_IS_ACP_AGENT (agent), NULL);
  g_return_val_if_fail (FOUNDRY_IS_ACP_CLIENT (client), NULL);

  self = g_object_new (FOUNDRY_TYPE_ACP_CONNECTION, NULL);
  self->context = g_object_ref (context);
  self->agent = g_object_ref (agent);
  self->client = g_object_ref (client);

  return self;
}

/**
 * foundry_acp_connection_new_for_pipeline:
 * @context: a [class@Foundry.Context]
 * @agent: a [class@Foundry.AcpAgent]
 * @client: a [iface@Foundry.AcpClient]
 * @pipeline: (nullable): a [class@Foundry.BuildPipeline] associated with the connection
 *
 * Creates a connection object for @agent.
 *
 * Returns: (transfer full): a [class@Foundry.AcpConnection]
 *
 * Since: 1.2
 */
FoundryAcpConnection *
foundry_acp_connection_new_for_pipeline (FoundryContext       *context,
                                         FoundryAcpAgent      *agent,
                                         FoundryAcpClient     *client,
                                         FoundryBuildPipeline *pipeline)
{
  FoundryAcpConnection *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (FOUNDRY_IS_ACP_AGENT (agent), NULL);
  g_return_val_if_fail (FOUNDRY_IS_ACP_CLIENT (client), NULL);
  g_return_val_if_fail (pipeline == NULL || FOUNDRY_IS_BUILD_PIPELINE (pipeline), NULL);

  self = foundry_acp_connection_new (context, agent, client);
  self->pipeline = pipeline ? g_object_ref (pipeline) : NULL;

  return self;
}

FoundryAcpConnection *
_foundry_acp_connection_new_for_stream (FoundryContext   *context,
                                        GIOStream        *stream,
                                        FoundryAcpClient *client)
{
  FoundryAcpConnection *self;

  g_return_val_if_fail (context == NULL || FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (G_IS_IO_STREAM (stream), NULL);
  g_return_val_if_fail (client == NULL || FOUNDRY_IS_ACP_CLIENT (client), NULL);

  self = g_object_new (FOUNDRY_TYPE_ACP_CONNECTION, NULL);
  self->context = context ? g_object_ref (context) : NULL;
  self->client = client ? g_object_ref (client) : NULL;
  self->driver = foundry_jsonrpc_driver_new (stream, FOUNDRY_JSONRPC_STYLE_LF);

  foundry_acp_connection_connect_driver (self);

  return self;
}

DexFuture *
_foundry_acp_connection_call (FoundryAcpConnection *self,
                              const char           *method,
                              JsonNode             *params)
{
  dex_return_error_if_fail (FOUNDRY_IS_ACP_CONNECTION (self));
  dex_return_error_if_fail (method != NULL);

  if (self->driver == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_TRANSPORT_CLOSED,
                                  "ACP connection is not started");

  return foundry_jsonrpc_driver_call (self->driver, method, params);
}

DexFuture *
_foundry_acp_connection_notify (FoundryAcpConnection *self,
                                const char           *method,
                                JsonNode             *params)
{
  dex_return_error_if_fail (FOUNDRY_IS_ACP_CONNECTION (self));
  dex_return_error_if_fail (method != NULL);

  if (self->driver == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_TRANSPORT_CLOSED,
                                  "ACP connection is not started");

  return foundry_jsonrpc_driver_notify (self->driver, method, params);
}

FoundryAcpSession *
_foundry_acp_connection_lookup_session (FoundryAcpConnection *self,
                                        const char           *session_id)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_CONNECTION (self), NULL);
  g_return_val_if_fail (session_id != NULL, NULL);

  return g_hash_table_lookup (self->sessions, session_id);
}

void
_foundry_acp_connection_add_session (FoundryAcpConnection *self,
                                     FoundryAcpSession    *session)
{
  const char *session_id;

  g_return_if_fail (FOUNDRY_IS_ACP_CONNECTION (self));
  g_return_if_fail (FOUNDRY_IS_ACP_SESSION (session));

  session_id = _foundry_acp_session_get_id (session);

  g_hash_table_replace (self->sessions, g_strdup (session_id), g_object_ref (session));
}

void
_foundry_acp_connection_remove_session (FoundryAcpConnection *self,
                                        FoundryAcpSession    *session)
{
  const char *session_id;

  g_return_if_fail (FOUNDRY_IS_ACP_CONNECTION (self));
  g_return_if_fail (FOUNDRY_IS_ACP_SESSION (session));

  session_id = _foundry_acp_session_get_id (session);

  g_hash_table_remove (self->sessions, session_id);
}

FoundryAcpClient *
_foundry_acp_connection_dup_client (FoundryAcpConnection *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_CONNECTION (self), NULL);

  if (self->client == NULL)
    return NULL;

  return g_object_ref (self->client);
}

void
_foundry_acp_connection_fail (FoundryAcpConnection *self,
                              const char           *message)
{
  GHashTableIter iter;
  gpointer value;

  g_return_if_fail (FOUNDRY_IS_ACP_CONNECTION (self));

  if (message == NULL)
    message = "ACP connection failed";

  foundry_acp_connection_set_state (self, FOUNDRY_ACP_CONNECTION_FAILED);

  g_hash_table_iter_init (&iter, self->sessions);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      FoundryAcpSession *session = value;
      g_autoptr(FoundryAcpEvent) event = NULL;
      g_autofree char *session_id = NULL;

      session_id = foundry_acp_session_dup_id (session);
      event = _foundry_acp_event_new_full (session_id,
                                           _foundry_acp_session_get_current_turn_id (session),
                                           NULL,
                                           NULL,
                                           FOUNDRY_ACP_EVENT_ERROR,
                                           "connection-failed",
                                           "ACP connection failed",
                                           message,
                                           FOUNDRY_ACP_EVENT_FAILED,
                                           NULL);
      _foundry_acp_session_append_event (session, event);
    }
}

/**
 * foundry_acp_connection_start:
 * @self: a [class@Foundry.AcpConnection]
 *
 * Starts the connection transport.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_connection_start (FoundryAcpConnection *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_ACP_CONNECTION (self));

  if (self->started)
    return dex_future_new_true ();

  if (self->driver != NULL)
    {
      foundry_jsonrpc_driver_start (self->driver);
      self->started = TRUE;
      foundry_acp_connection_set_state (self, FOUNDRY_ACP_CONNECTION_READY);
      return dex_future_new_true ();
    }

  return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                FOUNDRY_ACP_ERROR_INVALID_REQUEST,
                                "ACP connection has no transport");
}

static DexFuture *
foundry_acp_connection_initialize_fiber (FoundryAcpConnection            *self,
                                         const char                      *client_name,
                                         const char                      *client_title,
                                         const char                      *client_version,
                                         FoundryAcpClientCapabilityFlags  client_capabilities)
{
  g_autoptr(JsonNode) params = NULL;
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(JsonObject) capabilities = NULL;
  g_autoptr(JsonObject) fs = NULL;
  g_autoptr(JsonObject) client_info = NULL;
  JsonObject *reply_object;

  g_assert (FOUNDRY_IS_ACP_CONNECTION (self));

  capabilities = json_object_new ();

  if ((client_capabilities &
       (FOUNDRY_ACP_CLIENT_CAPABILITY_FS_READ_TEXT_FILE |
        FOUNDRY_ACP_CLIENT_CAPABILITY_FS_WRITE_TEXT_FILE)) != 0)
    {
      fs = json_object_new ();

      if ((client_capabilities & FOUNDRY_ACP_CLIENT_CAPABILITY_FS_READ_TEXT_FILE) != 0)
        json_object_set_boolean_member (fs, "readTextFile", TRUE);

      if ((client_capabilities & FOUNDRY_ACP_CLIENT_CAPABILITY_FS_WRITE_TEXT_FILE) != 0)
        json_object_set_boolean_member (fs, "writeTextFile", TRUE);

      json_object_set_object_member (capabilities, "fs", g_steal_pointer (&fs));
    }

  if ((client_capabilities & FOUNDRY_ACP_CLIENT_CAPABILITY_TERMINAL) != 0)
    json_object_set_boolean_member (capabilities, "terminal", TRUE);

  client_info = json_object_new ();
  json_object_set_string_member (client_info, "name", client_name);
  json_object_set_string_member (client_info, "title", client_title);
  json_object_set_string_member (client_info, "version", client_version);

  params = FOUNDRY_JSON_OBJECT_NEW ("protocolVersion",
                                    FOUNDRY_JSON_NODE_PUT_INT (FOUNDRY_ACP_PROTOCOL_VERSION),
                                    "clientCapabilities", "{",
                                    "}",
                                    "clientInfo", "{",
                                    "}");
  reply_object = json_node_get_object (params);
  json_object_set_object_member (reply_object, "clientCapabilities",
                                 g_steal_pointer (&capabilities));
  json_object_set_object_member (reply_object, "clientInfo", g_steal_pointer (&client_info));

  foundry_acp_connection_set_state (self, FOUNDRY_ACP_CONNECTION_INITIALIZING);

  if (!(reply = dex_await_boxed (_foundry_acp_connection_call (self, "initialize", params),
                                 &error)))
    {
      foundry_acp_connection_set_state (self, FOUNDRY_ACP_CONNECTION_FAILED);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (JSON_NODE_HOLDS_OBJECT (reply))
    {
      JsonObject *object = json_node_get_object (reply);
      JsonNode *auth_methods;
      JsonObject *agent_info;

      if (json_object_has_member (object, "protocolVersion"))
        self->protocol_version = json_object_get_int_member (object, "protocolVersion");

      if (json_object_has_member (object, "agentInfo") &&
          (agent_info = json_object_get_object_member (object, "agentInfo")))
        {
          const char *name = json_object_get_string_member_or_null (agent_info, "name");
          const char *title = json_object_get_string_member_or_null (agent_info, "title");
          const char *version = json_object_get_string_member_or_null (agent_info, "version");

          g_set_str (&self->agent_name, name);
          g_set_str (&self->agent_title, title);
          g_set_str (&self->agent_version, version);
        }

      auth_methods = json_object_get_member (object, "authMethods");

      if (auth_methods != NULL &&
          JSON_NODE_HOLDS_ARRAY (auth_methods) &&
          json_array_get_length (json_node_get_array (auth_methods)) > 0)
        foundry_acp_connection_set_state (self, FOUNDRY_ACP_CONNECTION_AUTH_REQUIRED);
      else
        foundry_acp_connection_set_state (self, FOUNDRY_ACP_CONNECTION_READY);
    }
  else
    {
      foundry_acp_connection_set_state (self, FOUNDRY_ACP_CONNECTION_READY);
    }

  self->client_capabilities = client_capabilities;

  return dex_future_new_true ();
}

/**
 * foundry_acp_connection_initialize:
 * @self: a [class@Foundry.AcpConnection]
 * @client_name: stable client identifier
 * @client_title: user-visible client title
 * @client_version: client version
 * @client_capabilities: enabled client capability flags
 *
 * Negotiates ACP protocol capabilities with the agent.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_connection_initialize (FoundryAcpConnection            *self,
                                   const char                      *client_name,
                                   const char                      *client_title,
                                   const char                      *client_version,
                                   FoundryAcpClientCapabilityFlags  client_capabilities)
{
  dex_return_error_if_fail (FOUNDRY_IS_ACP_CONNECTION (self));
  dex_return_error_if_fail (client_name != NULL);
  dex_return_error_if_fail (client_title != NULL);
  dex_return_error_if_fail (client_version != NULL);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (foundry_acp_connection_initialize_fiber),
                                  5,
                                  FOUNDRY_TYPE_ACP_CONNECTION, self,
                                  G_TYPE_STRING, client_name,
                                  G_TYPE_STRING, client_title,
                                  G_TYPE_STRING, client_version,
                                  FOUNDRY_TYPE_ACP_CLIENT_CAPABILITY_FLAGS, client_capabilities);
}

/**
 * foundry_acp_connection_authenticate:
 * @self: a [class@Foundry.AcpConnection]
 * @method_id: the ACP authentication method identifier
 *
 * Requests authentication using @method_id.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_connection_authenticate (FoundryAcpConnection *self,
                                     const char           *method_id)
{
  g_autoptr(JsonNode) params = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_CONNECTION (self));
  dex_return_error_if_fail (method_id != NULL);

  params = FOUNDRY_JSON_OBJECT_NEW ("methodId", FOUNDRY_JSON_NODE_PUT_STRING (method_id));

  return _foundry_acp_connection_call (self, "authenticate", params);
}

static DexFuture *
foundry_acp_connection_await_fiber (FoundryAcpConnection *self)
{
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_ACP_CONNECTION (self));

  if (self->driver != NULL &&
      !dex_await (foundry_jsonrpc_driver_await (self->driver), &error))
    {
      if (self->state != FOUNDRY_ACP_CONNECTION_CLOSING &&
          self->state != FOUNDRY_ACP_CONNECTION_CLOSED)
        foundry_acp_connection_set_state (self, FOUNDRY_ACP_CONNECTION_FAILED);

      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  foundry_acp_connection_set_state (self, FOUNDRY_ACP_CONNECTION_CLOSED);

  return dex_future_new_true ();
}

/**
 * foundry_acp_connection_await:
 * @self: a [class@Foundry.AcpConnection]
 *
 * Waits for the ACP connection to close.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_connection_await (FoundryAcpConnection *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_ACP_CONNECTION (self));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (foundry_acp_connection_await_fiber),
                                  1,
                                  FOUNDRY_TYPE_ACP_CONNECTION, self);
}

static DexFuture *
foundry_acp_connection_new_session_fiber (FoundryAcpConnection *self,
                                          const char           *cwd)
{
  g_autoptr(FoundryAcpSession) session = NULL;
  g_autoptr(JsonNode) params = NULL;
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(JsonNode) mcp_servers = NULL;
  g_autoptr(JsonArray) mcp_servers_array = NULL;
  g_autoptr(GFile) project_directory = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *resolved_cwd = NULL;
  const char *session_id = NULL;

  g_assert (FOUNDRY_IS_ACP_CONNECTION (self));

  if (cwd != NULL)
    resolved_cwd = g_strdup (cwd);
  else if (self->context != NULL &&
           (project_directory = foundry_context_dup_project_directory (self->context)))
    resolved_cwd = g_file_get_path (project_directory);

  if (resolved_cwd == NULL || !g_path_is_absolute (resolved_cwd))
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INVALID_PARAMS,
                                  "ACP sessions require an absolute cwd");

  mcp_servers = json_node_new (JSON_NODE_ARRAY);
  mcp_servers_array = json_array_new ();
  json_node_set_array (mcp_servers, mcp_servers_array);

  params = FOUNDRY_JSON_OBJECT_NEW ("cwd", FOUNDRY_JSON_NODE_PUT_STRING (resolved_cwd),
                                    "mcpServers", FOUNDRY_JSON_NODE_PUT_NODE (mcp_servers));

  if (!(reply = dex_await_boxed (_foundry_acp_connection_call (self, "session/new", params),
                                 &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (JSON_NODE_HOLDS_OBJECT (reply))
    {
      JsonObject *object = json_node_get_object (reply);

      session_id = json_object_get_string_member_or_null (object, "sessionId");
    }

  if (session_id == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INVALID_REQUEST,
                                  "ACP agent did not return a sessionId");

  session = _foundry_acp_session_new (self, session_id);
  _foundry_acp_session_set_state (session, FOUNDRY_ACP_SESSION_IDLE);
  _foundry_acp_connection_add_session (self, session);

  return dex_future_new_take_object (g_steal_pointer (&session));
}

/**
 * foundry_acp_connection_new_session:
 * @self: a [class@Foundry.AcpConnection]
 * @cwd: (nullable): an absolute working directory, or %NULL for the project root
 *
 * Creates a new ACP session using `session/new`.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.AcpSession]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_connection_new_session (FoundryAcpConnection *self,
                                    const char           *cwd)
{
  dex_return_error_if_fail (FOUNDRY_IS_ACP_CONNECTION (self));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (foundry_acp_connection_new_session_fiber),
                                  2,
                                  FOUNDRY_TYPE_ACP_CONNECTION, self,
                                  G_TYPE_STRING, cwd);
}

/**
 * foundry_acp_connection_close:
 * @self: a [class@Foundry.AcpConnection]
 *
 * Closes the ACP transport.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_connection_close (FoundryAcpConnection *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_ACP_CONNECTION (self));

  foundry_acp_connection_set_state (self, FOUNDRY_ACP_CONNECTION_CLOSING);

  if (self->driver != NULL)
    foundry_jsonrpc_driver_stop (self->driver);

  foundry_acp_connection_set_state (self, FOUNDRY_ACP_CONNECTION_CLOSED);

  return dex_future_new_true ();
}

/**
 * foundry_acp_connection_get_state:
 * @self: a [class@Foundry.AcpConnection]
 *
 * Returns: the connection state
 *
 * Since: 1.2
 */
FoundryAcpConnectionState
foundry_acp_connection_get_state (FoundryAcpConnection *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_CONNECTION (self), FOUNDRY_ACP_CONNECTION_CLOSED);

  return self->state;
}

/**
 * foundry_acp_connection_dup_agent_name:
 * @self: a [class@Foundry.AcpConnection]
 *
 * Returns: (transfer full) (nullable): the negotiated agent name
 *
 * Since: 1.2
 */
char *
foundry_acp_connection_dup_agent_name (FoundryAcpConnection *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_CONNECTION (self), NULL);

  return g_strdup (self->agent_name);
}

/**
 * foundry_acp_connection_dup_agent_title:
 * @self: a [class@Foundry.AcpConnection]
 *
 * Returns: (transfer full) (nullable): the negotiated agent title
 *
 * Since: 1.2
 */
char *
foundry_acp_connection_dup_agent_title (FoundryAcpConnection *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_CONNECTION (self), NULL);

  return g_strdup (self->agent_title);
}

/**
 * foundry_acp_connection_dup_agent_version:
 * @self: a [class@Foundry.AcpConnection]
 *
 * Returns: (transfer full) (nullable): the negotiated agent version
 *
 * Since: 1.2
 */
char *
foundry_acp_connection_dup_agent_version (FoundryAcpConnection *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_CONNECTION (self), NULL);

  return g_strdup (self->agent_version);
}
