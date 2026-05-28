/* foundry-acp-permission-request.c
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

#include "foundry-acp-permission-option-private.h"
#include "foundry-acp-permission-request-private.h"

struct _FoundryAcpPermissionRequest
{
  GObject parent_instance;

  char *session_id;
  char *request_id;
  char *tool_call_id;
  char *title;
  char *description;
  char *tool_name;
  char *command;
  char *path;
  char *default_option;
  char *risk_level;
  GListStore *options;
  JsonNode *raw;
};

G_DEFINE_FINAL_TYPE (FoundryAcpPermissionRequest, foundry_acp_permission_request, G_TYPE_OBJECT)

static void
foundry_acp_permission_request_finalize (GObject *object)
{
  FoundryAcpPermissionRequest *self = (FoundryAcpPermissionRequest *)object;

  g_clear_pointer (&self->session_id, g_free);
  g_clear_pointer (&self->request_id, g_free);
  g_clear_pointer (&self->tool_call_id, g_free);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->tool_name, g_free);
  g_clear_pointer (&self->command, g_free);
  g_clear_pointer (&self->path, g_free);
  g_clear_pointer (&self->default_option, g_free);
  g_clear_pointer (&self->risk_level, g_free);
  g_clear_object (&self->options);
  g_clear_pointer (&self->raw, json_node_unref);

  G_OBJECT_CLASS (foundry_acp_permission_request_parent_class)->finalize (object);
}

static void
foundry_acp_permission_request_class_init (FoundryAcpPermissionRequestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_acp_permission_request_finalize;
}

static void
foundry_acp_permission_request_init (FoundryAcpPermissionRequest *self)
{
}

static const char *
get_optional_string_member (JsonObject *object,
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

static JsonObject *
get_optional_object_member (JsonObject *object,
                            const char *member)
{
  JsonNode *node;

  g_assert (object != NULL);
  g_assert (member != NULL);

  if (!(node = json_object_get_member (object, member)))
    return NULL;

  if (!JSON_NODE_HOLDS_OBJECT (node))
    return NULL;

  return json_node_get_object (node);
}

static char *
dup_string_member (JsonObject  *object,
                   const char **members)
{
  JsonObject *child;
  const char *ret;

  g_assert (object != NULL);
  g_assert (members != NULL);

  for (guint i = 0; members[i] != NULL; i++)
    {
      if ((ret = get_optional_string_member (object, members[i])))
        return g_strdup (ret);
    }

  for (guint i = 0; members[i] != NULL; i++)
    {
      if ((child = get_optional_object_member (object, members[i])))
        {
          for (guint j = 0; members[j] != NULL; j++)
            {
              if ((ret = get_optional_string_member (child, members[j])))
                return g_strdup (ret);
            }
        }
    }

  return NULL;
}

FoundryAcpPermissionRequest *
_foundry_acp_permission_request_new (const char *session_id,
                                     JsonNode   *node)
{
  FoundryAcpPermissionRequest *self;

  g_return_val_if_fail (node != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_ACP_PERMISSION_REQUEST, NULL);
  self->session_id = g_strdup (session_id);
  self->options = g_list_store_new (FOUNDRY_TYPE_ACP_PERMISSION_OPTION);
  self->raw = json_node_ref (node);

  if (JSON_NODE_HOLDS_OBJECT (node))
    {
      JsonObject *object = json_node_get_object (node);
      JsonNode *options_node;
      const char *request_id_members[] = { "requestId", "request_id", "id", NULL };
      const char *tool_call_id_members[] = { "toolCallId", "tool_call_id", "callId", NULL };
      const char *title_members[] = { "title", "label", "permission", NULL };
      const char *description_members[] = { "description", "body", "message", NULL };
      const char *tool_name_members[] = { "toolName", "tool_name", "name", "tool", NULL };
      const char *command_members[] = { "command", "cmd", "tool", NULL };
      const char *path_members[] = { "path", "file", "target", "resource", NULL };
      const char *default_option_members[] = { "defaultOption", "default_option", "recommended", NULL };
      const char *risk_level_members[] = { "riskLevel", "risk_level", "risk", NULL };

      self->request_id = dup_string_member (object, request_id_members);
      self->tool_call_id = dup_string_member (object, tool_call_id_members);
      self->title = dup_string_member (object, title_members);
      self->description = dup_string_member (object, description_members);
      self->tool_name = dup_string_member (object, tool_name_members);
      self->command = dup_string_member (object, command_members);
      self->path = dup_string_member (object, path_members);
      self->default_option = dup_string_member (object, default_option_members);
      self->risk_level = dup_string_member (object, risk_level_members);

      if ((options_node = json_object_get_member (object, "options")) &&
          JSON_NODE_HOLDS_ARRAY (options_node))
        {
          JsonArray *options = json_node_get_array (options_node);
          guint length = json_array_get_length (options);

          for (guint i = 0; i < length; i++)
            {
              g_autoptr(FoundryAcpPermissionOption) option = NULL;
              JsonNode *element = json_array_get_element (options, i);

              if ((option = _foundry_acp_permission_option_new_from_json (element)))
                g_list_store_append (self->options, option);
            }
        }
    }

  return self;
}

/**
 * foundry_acp_permission_request_dup_session_id:
 * @self: a [class@Foundry.AcpPermissionRequest]
 *
 * Returns: (transfer full) (nullable): the session identifier
 *
 * Since: 1.2
 */
char *
foundry_acp_permission_request_dup_session_id (FoundryAcpPermissionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_REQUEST (self), NULL);

  return g_strdup (self->session_id);
}

/**
 * foundry_acp_permission_request_dup_request_id:
 * @self: a [class@Foundry.AcpPermissionRequest]
 *
 * Returns: (transfer full) (nullable): the request identifier
 *
 * Since: 1.2
 */
char *
foundry_acp_permission_request_dup_request_id (FoundryAcpPermissionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_REQUEST (self), NULL);

  return g_strdup (self->request_id);
}

/**
 * foundry_acp_permission_request_dup_tool_call_id:
 * @self: a [class@Foundry.AcpPermissionRequest]
 *
 * Returns: (transfer full) (nullable): the associated tool call identifier
 *
 * Since: 1.2
 */
char *
foundry_acp_permission_request_dup_tool_call_id (FoundryAcpPermissionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_REQUEST (self), NULL);

  return g_strdup (self->tool_call_id);
}

/**
 * foundry_acp_permission_request_dup_title:
 * @self: a [class@Foundry.AcpPermissionRequest]
 *
 * Returns: (transfer full) (nullable): the request title
 *
 * Since: 1.2
 */
char *
foundry_acp_permission_request_dup_title (FoundryAcpPermissionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_REQUEST (self), NULL);

  return g_strdup (self->title);
}

/**
 * foundry_acp_permission_request_dup_description:
 * @self: a [class@Foundry.AcpPermissionRequest]
 *
 * Returns: (transfer full) (nullable): the request description
 *
 * Since: 1.2
 */
char *
foundry_acp_permission_request_dup_description (FoundryAcpPermissionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_REQUEST (self), NULL);

  return g_strdup (self->description);
}

/**
 * foundry_acp_permission_request_dup_tool_name:
 * @self: a [class@Foundry.AcpPermissionRequest]
 *
 * Returns: (transfer full) (nullable): the tool name
 *
 * Since: 1.2
 */
char *
foundry_acp_permission_request_dup_tool_name (FoundryAcpPermissionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_REQUEST (self), NULL);

  return g_strdup (self->tool_name);
}

/**
 * foundry_acp_permission_request_dup_command:
 * @self: a [class@Foundry.AcpPermissionRequest]
 *
 * Returns: (transfer full) (nullable): the command when provided
 *
 * Since: 1.2
 */
char *
foundry_acp_permission_request_dup_command (FoundryAcpPermissionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_REQUEST (self), NULL);

  return g_strdup (self->command);
}

/**
 * foundry_acp_permission_request_dup_path:
 * @self: a [class@Foundry.AcpPermissionRequest]
 *
 * Returns: (transfer full) (nullable): the path or target resource when provided
 *
 * Since: 1.2
 */
char *
foundry_acp_permission_request_dup_path (FoundryAcpPermissionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_REQUEST (self), NULL);

  return g_strdup (self->path);
}

/**
 * foundry_acp_permission_request_dup_default_option:
 * @self: a [class@Foundry.AcpPermissionRequest]
 *
 * Returns: (transfer full) (nullable): the default or recommended option
 *
 * Since: 1.2
 */
char *
foundry_acp_permission_request_dup_default_option (FoundryAcpPermissionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_REQUEST (self), NULL);

  return g_strdup (self->default_option);
}

/**
 * foundry_acp_permission_request_dup_risk_level:
 * @self: a [class@Foundry.AcpPermissionRequest]
 *
 * Returns: (transfer full) (nullable): the risk level when provided
 *
 * Since: 1.2
 */
char *
foundry_acp_permission_request_dup_risk_level (FoundryAcpPermissionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_REQUEST (self), NULL);

  return g_strdup (self->risk_level);
}

/**
 * foundry_acp_permission_request_list_options:
 * @self: a [class@Foundry.AcpPermissionRequest]
 *
 * Lists structured permission options.
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of
 *   [class@Foundry.AcpPermissionOption]
 *
 * Since: 1.2
 */
GListModel *
foundry_acp_permission_request_list_options (FoundryAcpPermissionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_REQUEST (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->options));
}

/**
 * foundry_acp_permission_request_dup_raw:
 * @self: a [class@Foundry.AcpPermissionRequest]
 *
 * Returns: (transfer full): the raw request JSON
 *
 * Since: 1.2
 */
JsonNode *
foundry_acp_permission_request_dup_raw (FoundryAcpPermissionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_REQUEST (self), NULL);

  return json_node_ref (self->raw);
}

JsonNode *
_foundry_acp_permission_request_dup_raw (FoundryAcpPermissionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_REQUEST (self), NULL);

  return foundry_acp_permission_request_dup_raw (self);
}
