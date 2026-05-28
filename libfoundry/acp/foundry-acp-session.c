/* foundry-acp-session.c
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

#include "foundry-acp-changed-file.h"
#include "foundry-acp-client.h"
#include "foundry-acp-connection-private.h"
#include "foundry-acp-content-block-private.h"
#include "foundry-acp-enums.h"
#include "foundry-acp-event-private.h"
#include "foundry-acp-permission-request.h"
#include "foundry-acp-prompt-result-private.h"
#include "foundry-acp-session-private.h"
#include "foundry-acp-session-update-private.h"
#include "foundry-acp-terminal.h"
#include "foundry-json-node.h"

struct _FoundryAcpSession
{
  GObject parent_instance;
  FoundryAcpConnection *connection;
  GListStore *events;
  GListStore *active_terminals;
  GListStore *changed_files;
  FoundryAcpEvent *current_activity;
  FoundryAcpPermissionRequest *active_permission_request;
  char *id;
  char *current_turn_id;
  FoundryAcpSessionState state;
  guint next_turn_id;
};

G_DEFINE_FINAL_TYPE (FoundryAcpSession, foundry_acp_session, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_STATE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_acp_session_finalize (GObject *object)
{
  FoundryAcpSession *self = (FoundryAcpSession *)object;

  g_clear_object (&self->connection);
  g_clear_object (&self->events);
  g_clear_object (&self->active_terminals);
  g_clear_object (&self->changed_files);
  g_clear_object (&self->current_activity);
  g_clear_object (&self->active_permission_request);
  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->current_turn_id, g_free);

  G_OBJECT_CLASS (foundry_acp_session_parent_class)->finalize (object);
}

static void
foundry_acp_session_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundryAcpSession *self = FOUNDRY_ACP_SESSION (object);

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
foundry_acp_session_class_init (FoundryAcpSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_acp_session_finalize;
  object_class->get_property = foundry_acp_session_get_property;

  properties[PROP_STATE] =
    g_param_spec_enum ("state", NULL, NULL,
                       FOUNDRY_TYPE_ACP_SESSION_STATE,
                       FOUNDRY_ACP_SESSION_NEW,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_acp_session_init (FoundryAcpSession *self)
{
  self->state = FOUNDRY_ACP_SESSION_NEW;
  self->events = g_list_store_new (FOUNDRY_TYPE_ACP_EVENT);
  self->active_terminals = g_list_store_new (FOUNDRY_TYPE_ACP_TERMINAL);
  self->changed_files = g_list_store_new (FOUNDRY_TYPE_ACP_CHANGED_FILE);
}

FoundryAcpSession *
_foundry_acp_session_new (FoundryAcpConnection *connection,
                          const char           *id)
{
  FoundryAcpSession *self;

  g_return_val_if_fail (FOUNDRY_IS_ACP_CONNECTION (connection), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_ACP_SESSION, NULL);
  self->connection = g_object_ref (connection);
  self->id = g_strdup (id);

  return self;
}

const char *
_foundry_acp_session_get_id (FoundryAcpSession *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION (self), NULL);

  return self->id;
}

const char *
_foundry_acp_session_get_current_turn_id (FoundryAcpSession *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION (self), NULL);

  return self->current_turn_id;
}

void
_foundry_acp_session_set_state (FoundryAcpSession     *self,
                                FoundryAcpSessionState state)
{
  g_return_if_fail (FOUNDRY_IS_ACP_SESSION (self));

  if (self->state != state)
    {
      self->state = state;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
    }
}

void
_foundry_acp_session_append_event (FoundryAcpSession *self,
                                   FoundryAcpEvent   *event)
{
  g_return_if_fail (FOUNDRY_IS_ACP_SESSION (self));
  g_return_if_fail (FOUNDRY_IS_ACP_EVENT (event));

  {
    g_autofree char *turn_id = NULL;

    turn_id = foundry_acp_event_dup_turn_id (event);

    if (turn_id == NULL && self->current_turn_id != NULL)
      _foundry_acp_event_set_turn_id (event, self->current_turn_id);
  }

  g_list_store_append (self->events, event);

  if (foundry_acp_event_get_state (event) == FOUNDRY_ACP_EVENT_PENDING ||
      foundry_acp_event_get_state (event) == FOUNDRY_ACP_EVENT_RUNNING)
    g_set_object (&self->current_activity, event);
}

static FoundryAcpEventKind
event_kind_from_update_kind (const char *kind)
{
  if (kind == NULL)
    return FOUNDRY_ACP_EVENT_UNKNOWN;

  if (g_str_equal (kind, "agent_message_chunk") ||
      g_str_equal (kind, "message_chunk") ||
      g_str_equal (kind, "text_delta"))
    return FOUNDRY_ACP_EVENT_MESSAGE_CHUNK;

  if (g_str_equal (kind, "agent_message") ||
      g_str_equal (kind, "message"))
    return FOUNDRY_ACP_EVENT_MESSAGE;

  if (g_str_equal (kind, "thinking") ||
      g_str_equal (kind, "thought") ||
      g_str_equal (kind, "step"))
    return FOUNDRY_ACP_EVENT_STEP;

  if (g_str_equal (kind, "tool_call"))
    return FOUNDRY_ACP_EVENT_TOOL_CALL;

  if (g_str_equal (kind, "tool_call_update") ||
      g_str_equal (kind, "tool_update"))
    return FOUNDRY_ACP_EVENT_TOOL_UPDATE;

  if (g_str_equal (kind, "tool_call_result") ||
      g_str_equal (kind, "tool_result"))
    return FOUNDRY_ACP_EVENT_TOOL_RESULT;

  if (g_str_equal (kind, "error"))
    return FOUNDRY_ACP_EVENT_ERROR;

  return FOUNDRY_ACP_EVENT_UNKNOWN;
}

void
_foundry_acp_session_append_update (FoundryAcpSession       *self,
                                    FoundryAcpSessionUpdate *update)
{
  g_autoptr(FoundryAcpEvent) event = NULL;
  g_autoptr(JsonNode) raw = NULL;
  g_autofree char *kind = NULL;
  g_autofree char *text = NULL;
  g_autofree char *title = NULL;
  FoundryAcpEventKind event_kind;

  g_return_if_fail (FOUNDRY_IS_ACP_SESSION (self));
  g_return_if_fail (update != NULL);

  kind = foundry_acp_session_update_dup_kind (update);
  text = foundry_acp_session_update_dup_text (update);
  raw = _foundry_acp_session_update_dup_raw (update);
  event_kind = event_kind_from_update_kind (kind);

  if (kind != NULL)
    title = g_strdup (kind);
  else
    title = g_strdup ("Session update");

  event = _foundry_acp_event_new_full (self->id,
                                       self->current_turn_id,
                                       NULL,
                                       NULL,
                                       event_kind,
                                       kind,
                                       title,
                                       text,
                                       FOUNDRY_ACP_EVENT_COMPLETED,
                                       raw);
  _foundry_acp_session_append_event (self, event);
}

FoundryAcpTerminal *
_foundry_acp_session_lookup_terminal (FoundryAcpSession *self,
                                      const char        *terminal_id)
{
  guint n_items;

  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION (self), NULL);
  g_return_val_if_fail (terminal_id != NULL, NULL);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->active_terminals));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryAcpTerminal) terminal = NULL;
      g_autofree char *id = NULL;

      terminal = g_list_model_get_item (G_LIST_MODEL (self->active_terminals), i);
      id = foundry_acp_terminal_dup_id (terminal);

      if (g_strcmp0 (id, terminal_id) == 0)
        return g_object_ref (terminal);
    }

  return NULL;
}

void
_foundry_acp_session_add_terminal (FoundryAcpSession  *self,
                                   FoundryAcpTerminal *terminal)
{
  g_autofree char *terminal_id = NULL;

  g_return_if_fail (FOUNDRY_IS_ACP_SESSION (self));
  g_return_if_fail (FOUNDRY_IS_ACP_TERMINAL (terminal));

  terminal_id = foundry_acp_terminal_dup_id (terminal);

  if (terminal_id != NULL)
    _foundry_acp_session_remove_terminal (self, terminal_id);

  g_list_store_append (self->active_terminals, terminal);
}

void
_foundry_acp_session_remove_terminal (FoundryAcpSession *self,
                                      const char        *terminal_id)
{
  guint n_items;

  g_return_if_fail (FOUNDRY_IS_ACP_SESSION (self));
  g_return_if_fail (terminal_id != NULL);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->active_terminals));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryAcpTerminal) terminal = NULL;
      g_autofree char *id = NULL;

      terminal = g_list_model_get_item (G_LIST_MODEL (self->active_terminals), i);
      id = foundry_acp_terminal_dup_id (terminal);

      if (g_strcmp0 (id, terminal_id) == 0)
        {
          g_list_store_remove (self->active_terminals, i);
          break;
        }
    }
}

void
_foundry_acp_session_note_changed_file (FoundryAcpSession           *self,
                                        const char                  *path,
                                        FoundryAcpChangedFileKind    kind,
                                        const char                  *last_event_id,
                                        FoundryAcpChangedFileFlags   flags)
{
  g_autoptr(FoundryAcpChangedFile) changed_file = NULL;
  guint n_items;

  g_return_if_fail (FOUNDRY_IS_ACP_SESSION (self));
  g_return_if_fail (path != NULL);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->changed_files));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryAcpChangedFile) item = NULL;
      g_autofree char *item_path = NULL;

      item = g_list_model_get_item (G_LIST_MODEL (self->changed_files), i);
      item_path = foundry_acp_changed_file_dup_path (item);

      if (g_strcmp0 (item_path, path) == 0)
        {
          g_list_store_remove (self->changed_files, i);
          break;
        }
    }

  changed_file = foundry_acp_changed_file_new (path, kind, last_event_id, flags);
  g_list_store_append (self->changed_files, changed_file);
}

void
_foundry_acp_session_set_active_permission_request (FoundryAcpSession           *self,
                                                   FoundryAcpPermissionRequest *request)
{
  g_return_if_fail (FOUNDRY_IS_ACP_SESSION (self));

  if (request != NULL)
    g_object_ref (request);

  g_clear_object (&self->active_permission_request);
  self->active_permission_request = request;
}

/**
 * foundry_acp_session_dup_id:
 * @self: a [class@Foundry.AcpSession]
 *
 * Returns: (transfer full): the ACP session identifier
 *
 * Since: 1.2
 */
char *
foundry_acp_session_dup_id (FoundryAcpSession *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION (self), NULL);

  return g_strdup (self->id);
}

/**
 * foundry_acp_session_get_state:
 * @self: a [class@Foundry.AcpSession]
 *
 * Returns: the session state
 *
 * Since: 1.2
 */
FoundryAcpSessionState
foundry_acp_session_get_state (FoundryAcpSession *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION (self), FOUNDRY_ACP_SESSION_CLOSED);

  return self->state;
}

/**
 * foundry_acp_session_dup_connection:
 * @self: a [class@Foundry.AcpSession]
 *
 * Returns: (transfer full): the owning ACP connection
 *
 * Since: 1.2
 */
FoundryAcpConnection *
foundry_acp_session_dup_connection (FoundryAcpSession *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION (self), NULL);

  return g_object_ref (self->connection);
}

/**
 * foundry_acp_session_list_events:
 * @self: a [class@Foundry.AcpSession]
 *
 * Lists normalized monitoring events for the session.
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of [class@Foundry.AcpEvent]
 *
 * Since: 1.2
 */
GListModel *
foundry_acp_session_list_events (FoundryAcpSession *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->events));
}

/**
 * foundry_acp_session_list_active_terminals:
 * @self: a [class@Foundry.AcpSession]
 *
 * Lists terminal handles known to the session. Terminals remain in the model
 * after exit until they are released by the ACP agent.
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of [class@Foundry.AcpTerminal]
 *
 * Since: 1.2
 */
GListModel *
foundry_acp_session_list_active_terminals (FoundryAcpSession *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->active_terminals));
}

/**
 * foundry_acp_session_list_changed_files:
 * @self: a [class@Foundry.AcpSession]
 *
 * Lists files known to have changed during the session.
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of
 *   [class@Foundry.AcpChangedFile]
 *
 * Since: 1.2
 */
GListModel *
foundry_acp_session_list_changed_files (FoundryAcpSession *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->changed_files));
}

/**
 * foundry_acp_session_dup_current_activity:
 * @self: a [class@Foundry.AcpSession]
 *
 * Returns: (transfer full) (nullable): the current running or blocking event
 *
 * Since: 1.2
 */
FoundryAcpEvent *
foundry_acp_session_dup_current_activity (FoundryAcpSession *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION (self), NULL);

  if (self->current_activity == NULL)
    return NULL;

  return g_object_ref (self->current_activity);
}

/**
 * foundry_acp_session_dup_active_permission_request:
 * @self: a [class@Foundry.AcpSession]
 *
 * Returns: (transfer full) (nullable): the active permission request
 *
 * Since: 1.2
 */
FoundryAcpPermissionRequest *
foundry_acp_session_dup_active_permission_request (FoundryAcpSession *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION (self), NULL);

  if (self->active_permission_request == NULL)
    return NULL;

  return g_object_ref (self->active_permission_request);
}

static JsonNode *
foundry_acp_session_content_to_json (FoundryAcpContentBlock * const *blocks,
                                     guint                           n_blocks)
{
  g_autoptr(JsonArray) array = NULL;
  JsonNode *node;

  node = json_node_new (JSON_NODE_ARRAY);
  array = json_array_new ();

  for (guint i = 0; i < n_blocks; i++)
    json_array_add_element (array, _foundry_acp_content_block_to_json (blocks[i]));

  json_node_set_array (node, array);

  return node;
}

static DexFuture *
foundry_acp_session_prompt_complete (DexFuture *completed,
                                     gpointer   user_data)
{
  FoundryAcpSession *self = user_data;
  const GValue *value;
  g_autoptr(FoundryAcpPromptResult) result = NULL;
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(GError) error = NULL;
  FoundryAcpClient *client = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (FOUNDRY_IS_ACP_SESSION (self));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!G_VALUE_HOLDS (value, JSON_TYPE_NODE))
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INTERNAL,
                                  "Prompt reply is not a JSON node");

  reply = json_node_ref (g_value_get_boxed (value));
  result = _foundry_acp_prompt_result_new_from_json (reply);
  _foundry_acp_session_set_state (self, FOUNDRY_ACP_SESSION_IDLE);

  if (self->connection != NULL &&
      (client = _foundry_acp_connection_dup_client (self->connection)))
    dex_await (foundry_acp_client_refresh_changed_files (client, self), NULL);
  g_clear_object (&client);

  g_clear_pointer (&self->current_turn_id, g_free);
  g_clear_object (&self->current_activity);

  return dex_future_new_take_object (g_steal_pointer (&result));
}

static DexFuture *
foundry_acp_session_prompt_failed (DexFuture *completed,
                                   gpointer   user_data)
{
  FoundryAcpSession *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (FOUNDRY_IS_ACP_SESSION (self));

  _foundry_acp_session_set_state (self, FOUNDRY_ACP_SESSION_IDLE);
  g_clear_pointer (&self->current_turn_id, g_free);
  g_clear_object (&self->current_activity);

  if (dex_future_get_value (completed, &error))
    return dex_future_new_true ();

  return dex_future_new_for_error (g_steal_pointer (&error));
}

/**
 * foundry_acp_session_prompt:
 * @self: a [class@Foundry.AcpSession]
 * @blocks: (array length=n_blocks): content blocks for the prompt
 * @n_blocks: the number of content blocks
 *
 * Sends a prompt turn to the ACP agent.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.AcpPromptResult]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_session_prompt (FoundryAcpSession              *self,
                            FoundryAcpContentBlock * const *blocks,
                            guint                           n_blocks)
{
  g_autoptr(JsonNode) content_node = NULL;
  g_autoptr(JsonNode) params = NULL;
  DexFuture *future;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (self));
  dex_return_error_if_fail (blocks != NULL || n_blocks == 0);

  content_node = foundry_acp_session_content_to_json (blocks, n_blocks);
  params = FOUNDRY_JSON_OBJECT_NEW ("sessionId", FOUNDRY_JSON_NODE_PUT_STRING (self->id),
                                    "prompt", FOUNDRY_JSON_NODE_PUT_NODE (content_node));

  g_free (self->current_turn_id);
  self->current_turn_id = g_strdup_printf ("%u", ++self->next_turn_id);
  _foundry_acp_session_set_state (self, FOUNDRY_ACP_SESSION_RUNNING);

  future = _foundry_acp_connection_call (self->connection, "session/prompt", params);
  future = dex_future_then (future,
                            foundry_acp_session_prompt_complete,
                            g_object_ref (self),
                            g_object_unref);
  future = dex_future_catch (future,
                             foundry_acp_session_prompt_failed,
                             g_object_ref (self),
                             g_object_unref);

  return future;
}

/**
 * foundry_acp_session_cancel:
 * @self: a [class@Foundry.AcpSession]
 *
 * Sends an ACP `session/cancel` notification.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_session_cancel (FoundryAcpSession *self)
{
  g_autoptr(JsonNode) params = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (self));

  params = FOUNDRY_JSON_OBJECT_NEW ("sessionId", FOUNDRY_JSON_NODE_PUT_STRING (self->id));
  _foundry_acp_session_set_state (self, FOUNDRY_ACP_SESSION_CANCELLING);

  return _foundry_acp_connection_notify (self->connection, "session/cancel", params);
}

static DexFuture *
foundry_acp_session_close_complete (DexFuture *completed,
                                    gpointer   user_data)
{
  FoundryAcpSession *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (FOUNDRY_IS_ACP_SESSION (self));

  if (dex_future_get_value (completed, &error))
    {
      _foundry_acp_session_set_state (self, FOUNDRY_ACP_SESSION_CLOSED);
      _foundry_acp_connection_remove_session (self->connection, self);
      return dex_future_new_true ();
    }

  return dex_future_new_for_error (g_steal_pointer (&error));
}

/**
 * foundry_acp_session_close:
 * @self: a [class@Foundry.AcpSession]
 *
 * Requests the agent close the session.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_session_close (FoundryAcpSession *self)
{
  g_autoptr(JsonNode) params = NULL;
  DexFuture *future;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (self));

  params = FOUNDRY_JSON_OBJECT_NEW ("sessionId", FOUNDRY_JSON_NODE_PUT_STRING (self->id));
  future = _foundry_acp_connection_call (self->connection, "session/close", params);

  return dex_future_finally (future,
                             foundry_acp_session_close_complete,
                             g_object_ref (self),
                             g_object_unref);
}

/**
 * foundry_acp_session_set_mode:
 * @self: a [class@Foundry.AcpSession]
 * @mode_id: the ACP mode identifier
 *
 * Requests a session mode change.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_session_set_mode (FoundryAcpSession *self,
                              const char        *mode_id)
{
  g_autoptr(JsonNode) params = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (self));
  dex_return_error_if_fail (mode_id != NULL);

  params = FOUNDRY_JSON_OBJECT_NEW ("sessionId", FOUNDRY_JSON_NODE_PUT_STRING (self->id),
                                    "modeId", FOUNDRY_JSON_NODE_PUT_STRING (mode_id));

  return _foundry_acp_connection_call (self->connection, "session/set_mode", params);
}

/**
 * foundry_acp_session_set_config_option:
 * @self: a [class@Foundry.AcpSession]
 * @config_id: the ACP config option identifier
 * @value_id: the ACP config value identifier
 *
 * Requests a session config option change.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_session_set_config_option (FoundryAcpSession *self,
                                       const char        *config_id,
                                       const char        *value_id)
{
  g_autoptr(JsonNode) params = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (self));
  dex_return_error_if_fail (config_id != NULL);
  dex_return_error_if_fail (value_id != NULL);

  params = FOUNDRY_JSON_OBJECT_NEW ("sessionId", FOUNDRY_JSON_NODE_PUT_STRING (self->id),
                                    "configId", FOUNDRY_JSON_NODE_PUT_STRING (config_id),
                                    "valueId", FOUNDRY_JSON_NODE_PUT_STRING (value_id));

  return _foundry_acp_connection_call (self->connection, "session/set_config_option", params);
}
