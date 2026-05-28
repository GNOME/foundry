/* foundry-acp-session-update.c
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

#include "foundry-acp-content-block.h"
#include "foundry-acp-session-update-private.h"

struct _FoundryAcpSessionUpdate
{
  GObject parent_instance;

  char *session_id;
  char *kind;
  char *text;
  GPtrArray *content_blocks;
  FoundryAcpSessionUpdateKind update_kind;
  JsonNode *raw;
};

G_DEFINE_FINAL_TYPE (FoundryAcpSessionUpdate, foundry_acp_session_update, G_TYPE_OBJECT)

static void
foundry_acp_session_update_finalize (GObject *object)
{
  FoundryAcpSessionUpdate *self = (FoundryAcpSessionUpdate *)object;

  g_clear_pointer (&self->session_id, g_free);
  g_clear_pointer (&self->kind, g_free);
  g_clear_pointer (&self->text, g_free);
  g_clear_pointer (&self->content_blocks, g_ptr_array_unref);
  g_clear_pointer (&self->raw, json_node_unref);

  G_OBJECT_CLASS (foundry_acp_session_update_parent_class)->finalize (object);
}

static void
foundry_acp_session_update_class_init (FoundryAcpSessionUpdateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_acp_session_update_finalize;
}

static void
foundry_acp_session_update_init (FoundryAcpSessionUpdate *self)
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

static gboolean
get_optional_int_member (JsonObject *object,
                         const char *member,
                         gint64     *value)
{
  JsonNode *node;

  g_assert (object != NULL);
  g_assert (member != NULL);

  if (!(node = json_object_get_member (object, member)))
    return FALSE;

  if (!JSON_NODE_HOLDS_VALUE (node))
    return FALSE;

  if (json_node_get_value_type (node) != G_TYPE_INT64 &&
      json_node_get_value_type (node) != G_TYPE_INT &&
      json_node_get_value_type (node) != G_TYPE_UINT &&
      json_node_get_value_type (node) != G_TYPE_UINT64 &&
      json_node_get_value_type (node) != G_TYPE_DOUBLE)
    return FALSE;

  if (value != NULL)
    *value = json_node_get_int (node);

  return TRUE;
}

static const char *
get_nested_string_member (JsonObject  *object,
                          const char **members)
{
  JsonObject *child;
  const char *ret;

  g_assert (object != NULL);
  g_assert (members != NULL);

  for (guint i = 0; members[i] != NULL; i++)
    {
      if ((ret = get_optional_string_member (object, members[i])))
        return ret;
    }

  for (guint i = 0; members[i] != NULL; i++)
    {
      if ((child = get_optional_object_member (object, members[i])))
        {
          for (guint j = 0; members[j] != NULL; j++)
            {
              if ((ret = get_optional_string_member (child, members[j])))
                return ret;
            }
        }
    }

  return NULL;
}

static gboolean
get_nested_int_member (JsonObject  *object,
                       const char **members,
                       gint64      *value)
{
  JsonObject *child;

  g_assert (object != NULL);
  g_assert (members != NULL);

  for (guint i = 0; members[i] != NULL; i++)
    {
      if (get_optional_int_member (object, members[i], value))
        return TRUE;
    }

  for (guint i = 0; members[i] != NULL; i++)
    {
      if ((child = get_optional_object_member (object, members[i])))
        {
          for (guint j = 0; members[j] != NULL; j++)
            {
              if (get_optional_int_member (child, members[j], value))
                return TRUE;
            }
        }
    }

  return FALSE;
}

static const char *
get_content_block_text (JsonObject *object)
{
  JsonNode *node;
  JsonObject *content;
  JsonArray *array;

  g_assert (object != NULL);

  if (!(node = json_object_get_member (object, "content")))
    return NULL;

  if (JSON_NODE_HOLDS_VALUE (node) && json_node_get_value_type (node) == G_TYPE_STRING)
    return json_node_get_string (node);

  if (JSON_NODE_HOLDS_ARRAY (node))
    {
      array = json_node_get_array (node);

      for (guint i = 0; i < json_array_get_length (array); i++)
        {
          JsonNode *element = json_array_get_element (array, i);

          if (JSON_NODE_HOLDS_VALUE (element) &&
              json_node_get_value_type (element) == G_TYPE_STRING)
            return json_node_get_string (element);

          if (JSON_NODE_HOLDS_OBJECT (element) &&
              (content = json_node_get_object (element)))
            {
              const char *text = get_optional_string_member (content, "text");

              if (text != NULL)
                return text;
            }
        }

      return NULL;
    }

  if (!JSON_NODE_HOLDS_OBJECT (node) || !(content = json_node_get_object (node)))
    return NULL;

  return get_optional_string_member (content, "text");
}

static FoundryAcpSessionUpdateKind
get_update_kind (const char *kind)
{
  g_autofree char *folded = NULL;

  if (kind == NULL)
    return FOUNDRY_ACP_SESSION_UPDATE_UNKNOWN;

  folded = g_ascii_strdown (kind, -1);

  if (strstr (folded, "error") != NULL)
    return FOUNDRY_ACP_SESSION_UPDATE_ERROR;

  if (strstr (folded, "message") != NULL && strstr (folded, "chunk") != NULL)
    return FOUNDRY_ACP_SESSION_UPDATE_MESSAGE_CHUNK;

  if (strstr (folded, "message") != NULL)
    return FOUNDRY_ACP_SESSION_UPDATE_MESSAGE;

  if (strstr (folded, "thought") != NULL ||
      strstr (folded, "reasoning") != NULL ||
      strstr (folded, "step") != NULL)
    return FOUNDRY_ACP_SESSION_UPDATE_STEP;

  if (strstr (folded, "tool") != NULL && strstr (folded, "result") != NULL)
    return FOUNDRY_ACP_SESSION_UPDATE_TOOL_RESULT;

  if (strstr (folded, "tool") != NULL &&
      (strstr (folded, "update") != NULL || strstr (folded, "delta") != NULL))
    return FOUNDRY_ACP_SESSION_UPDATE_TOOL_UPDATE;

  if (strstr (folded, "tool") != NULL)
    return FOUNDRY_ACP_SESSION_UPDATE_TOOL_CALL;

  if (strstr (folded, "terminal") != NULL && strstr (folded, "exit") != NULL)
    return FOUNDRY_ACP_SESSION_UPDATE_TERMINAL_EXITED;

  if (strstr (folded, "terminal") != NULL && strstr (folded, "output") != NULL)
    return FOUNDRY_ACP_SESSION_UPDATE_TERMINAL_OUTPUT;

  if (strstr (folded, "terminal") != NULL)
    return FOUNDRY_ACP_SESSION_UPDATE_TERMINAL_CREATED;

  if (strstr (folded, "file") != NULL && strstr (folded, "read") != NULL)
    return FOUNDRY_ACP_SESSION_UPDATE_FILE_READ;

  if (strstr (folded, "file") != NULL &&
      (strstr (folded, "write") != NULL || strstr (folded, "edit") != NULL))
    return FOUNDRY_ACP_SESSION_UPDATE_FILE_WRITE;

  if (strstr (folded, "file") != NULL && strstr (folded, "patch") != NULL)
    return FOUNDRY_ACP_SESSION_UPDATE_FILE_PATCH;

  if (strstr (folded, "file") != NULL && strstr (folded, "create") != NULL)
    return FOUNDRY_ACP_SESSION_UPDATE_FILE_CREATED;

  if (strstr (folded, "file") != NULL && strstr (folded, "delete") != NULL)
    return FOUNDRY_ACP_SESSION_UPDATE_FILE_DELETED;

  if (strstr (folded, "progress") != NULL || strstr (folded, "status") != NULL)
    return FOUNDRY_ACP_SESSION_UPDATE_PROGRESS;

  return FOUNDRY_ACP_SESSION_UPDATE_UNKNOWN;
}

static FoundryAcpContentBlock *
content_block_new_from_json_object (JsonObject *object)
{
  JsonObject *resource;
  const char *mime_type;
  const char *name;
  const char *text;
  const char *type;
  const char *uri;

  g_assert (object != NULL);

  type = get_optional_string_member (object, "type");
  text = get_optional_string_member (object, "text");
  uri = get_optional_string_member (object, "uri");
  name = get_optional_string_member (object, "name");
  mime_type = get_optional_string_member (object, "mimeType");
  if (mime_type == NULL)
    mime_type = get_optional_string_member (object, "mime_type");

  if (g_strcmp0 (type, "resource") == 0 &&
      (resource = get_optional_object_member (object, "resource")))
    {
      uri = get_optional_string_member (resource, "uri");
      text = get_optional_string_member (resource, "text");
      mime_type = get_optional_string_member (resource, "mimeType");
      if (mime_type == NULL)
        mime_type = get_optional_string_member (resource, "mime_type");

      if (uri != NULL && text != NULL)
        return foundry_acp_content_block_new_text_resource (uri, mime_type, text);
    }

  if (g_strcmp0 (type, "resource_link") == 0 && uri != NULL)
    return foundry_acp_content_block_new_resource_link (uri, name, mime_type);

  if (uri != NULL && text != NULL)
    return foundry_acp_content_block_new_text_resource (uri, mime_type, text);

  if (text != NULL)
    return foundry_acp_content_block_new_text (text);

  return NULL;
}

static void
append_content_block_from_json (GPtrArray *blocks,
                                JsonNode  *node)
{
  FoundryAcpContentBlock *block = NULL;

  g_assert (blocks != NULL);
  g_assert (node != NULL);

  if (JSON_NODE_HOLDS_VALUE (node) && json_node_get_value_type (node) == G_TYPE_STRING)
    block = foundry_acp_content_block_new_text (json_node_get_string (node));
  else if (JSON_NODE_HOLDS_OBJECT (node))
    block = content_block_new_from_json_object (json_node_get_object (node));

  if (block != NULL)
    g_ptr_array_add (blocks, block);
}

static GPtrArray *
parse_content_blocks (JsonObject *object)
{
  GPtrArray *blocks;
  JsonArray *array;
  JsonNode *node;

  g_assert (object != NULL);

  blocks = g_ptr_array_new_with_free_func (g_object_unref);

  if ((node = json_object_get_member (object, "content")))
    {
      if (JSON_NODE_HOLDS_ARRAY (node))
        {
          array = json_node_get_array (node);

          for (guint i = 0; i < json_array_get_length (array); i++)
            append_content_block_from_json (blocks, json_array_get_element (array, i));
        }
      else
        append_content_block_from_json (blocks, node);
    }

  if (blocks->len == 0 && (node = json_object_get_member (object, "delta")))
    append_content_block_from_json (blocks, node);

  if (blocks->len == 0)
    g_clear_pointer (&blocks, g_ptr_array_unref);

  return blocks;
}

FoundryAcpSessionUpdate *
_foundry_acp_session_update_new_from_json (const char *session_id,
                                           JsonNode   *node)
{
  FoundryAcpSessionUpdate *self;
  JsonNode *update_node = node;
  const char *kind = NULL;
  const char *text = NULL;

  g_return_val_if_fail (node != NULL, NULL);

  if (JSON_NODE_HOLDS_OBJECT (node))
    {
      JsonObject *object = json_node_get_object (node);

      if (json_object_has_member (object, "update"))
        update_node = json_object_get_member (object, "update");
    }

  if (update_node != NULL && JSON_NODE_HOLDS_OBJECT (update_node))
    {
      JsonObject *object = json_node_get_object (update_node);

      kind = get_optional_string_member (object, "sessionUpdate");
      if (kind == NULL)
        kind = get_optional_string_member (object, "type");
      if (kind == NULL)
        kind = get_optional_string_member (object, "kind");

      text = get_optional_string_member (object, "text");
      if (text == NULL)
        text = get_optional_string_member (object, "chunk");
      if (text == NULL)
        text = get_content_block_text (object);
    }

  self = g_object_new (FOUNDRY_TYPE_ACP_SESSION_UPDATE, NULL);
  self->session_id = g_strdup (session_id);
  self->kind = g_strdup (kind);
  self->text = g_strdup (text);
  self->update_kind = get_update_kind (kind);
  self->raw = json_node_ref (update_node);

  if (update_node != NULL && JSON_NODE_HOLDS_OBJECT (update_node))
    self->content_blocks = parse_content_blocks (json_node_get_object (update_node));

  return self;
}

/**
 * foundry_acp_session_update_get_update_kind:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Gets the normalized update kind for @self.
 *
 * Returns: a [enum@Foundry.AcpSessionUpdateKind]
 *
 * Since: 1.2
 */
FoundryAcpSessionUpdateKind
foundry_acp_session_update_get_update_kind (FoundryAcpSessionUpdate *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION_UPDATE (self), FOUNDRY_ACP_SESSION_UPDATE_UNKNOWN);

  return self->update_kind;
}

/**
 * foundry_acp_session_update_dup_kind:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full) (nullable): the update variant name
 *
 * Since: 1.2
 */
char *
foundry_acp_session_update_dup_kind (FoundryAcpSessionUpdate *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION_UPDATE (self), NULL);

  return g_strdup (self->kind);
}

/**
 * foundry_acp_session_update_dup_text:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full) (nullable): a text-like payload when present
 *
 * Since: 1.2
 */
char *
foundry_acp_session_update_dup_text (FoundryAcpSessionUpdate *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION_UPDATE (self), NULL);

  return g_strdup (self->text);
}

/**
 * foundry_acp_session_update_dup_content_blocks:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Gets parsed content blocks from the update.
 *
 * Returns: (transfer full) (element-type Foundry.AcpContentBlock): content blocks
 *
 * Since: 1.2
 */
GPtrArray *
foundry_acp_session_update_dup_content_blocks (FoundryAcpSessionUpdate *self)
{
  GPtrArray *copy;

  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION_UPDATE (self), NULL);

  copy = g_ptr_array_new_with_free_func (g_object_unref);

  if (self->content_blocks != NULL)
    {
      for (guint i = 0; i < self->content_blocks->len; i++)
        {
          FoundryAcpContentBlock *block = g_ptr_array_index (self->content_blocks, i);

          g_ptr_array_add (copy, g_object_ref (block));
        }
    }

  return copy;
}

static char *
foundry_acp_session_update_dup_member (FoundryAcpSessionUpdate  *self,
                                       const char              **members)
{
  JsonObject *object;

  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION_UPDATE (self), NULL);
  g_return_val_if_fail (members != NULL, NULL);

  if (self->raw == NULL || !JSON_NODE_HOLDS_OBJECT (self->raw))
    return NULL;

  object = json_node_get_object (self->raw);

  return g_strdup (get_nested_string_member (object, members));
}

static gboolean
foundry_acp_session_update_get_member_int (FoundryAcpSessionUpdate  *self,
                                           const char              **members,
                                           gint64                   *value)
{
  JsonObject *object;

  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION_UPDATE (self), FALSE);
  g_return_val_if_fail (members != NULL, FALSE);

  if (self->raw == NULL || !JSON_NODE_HOLDS_OBJECT (self->raw))
    return FALSE;

  object = json_node_get_object (self->raw);

  return get_nested_int_member (object, members, value);
}

/**
 * foundry_acp_session_update_dup_tool_call_id:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full) (nullable): the tool call identifier
 *
 * Since: 1.2
 */
char *
foundry_acp_session_update_dup_tool_call_id (FoundryAcpSessionUpdate *self)
{
  const char *members[] = { "toolCallId", "tool_call_id", "callId", "id", "toolCall", NULL };

  return foundry_acp_session_update_dup_member (self, members);
}

/**
 * foundry_acp_session_update_dup_tool_name:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full) (nullable): the tool name
 *
 * Since: 1.2
 */
char *
foundry_acp_session_update_dup_tool_name (FoundryAcpSessionUpdate *self)
{
  const char *members[] = { "toolName", "tool_name", "name", "tool", NULL };

  return foundry_acp_session_update_dup_member (self, members);
}

/**
 * foundry_acp_session_update_dup_tool_title:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full) (nullable): a human-facing tool title
 *
 * Since: 1.2
 */
char *
foundry_acp_session_update_dup_tool_title (FoundryAcpSessionUpdate *self)
{
  const char *members[] = { "title", "label", "toolCall", NULL };

  return foundry_acp_session_update_dup_member (self, members);
}

/**
 * foundry_acp_session_update_dup_tool_status:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full) (nullable): the tool status
 *
 * Since: 1.2
 */
char *
foundry_acp_session_update_dup_tool_status (FoundryAcpSessionUpdate *self)
{
  const char *members[] = { "status", "state", "toolCall", NULL };

  return foundry_acp_session_update_dup_member (self, members);
}

/**
 * foundry_acp_session_update_dup_path:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full) (nullable): a file path from the update
 *
 * Since: 1.2
 */
char *
foundry_acp_session_update_dup_path (FoundryAcpSessionUpdate *self)
{
  const char *members[] = { "path", "file", "filePath", "target", NULL };

  return foundry_acp_session_update_dup_member (self, members);
}

/**
 * foundry_acp_session_update_dup_old_path:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full) (nullable): the previous file path from the update
 *
 * Since: 1.2
 */
char *
foundry_acp_session_update_dup_old_path (FoundryAcpSessionUpdate *self)
{
  const char *members[] = { "oldPath", "old_path", "previousPath", "from", NULL };

  return foundry_acp_session_update_dup_member (self, members);
}

/**
 * foundry_acp_session_update_get_line_range:
 * @self: a [class@Foundry.AcpSessionUpdate]
 * @begin_line: (out) (optional): location for the first affected line
 * @end_line: (out) (optional): location for the last affected line
 *
 * Gets the affected line range from @self, if present.
 *
 * Returns: %TRUE if line range data was found
 *
 * Since: 1.2
 */
gboolean
foundry_acp_session_update_get_line_range (FoundryAcpSessionUpdate *self,
                                           guint                   *begin_line,
                                           guint                   *end_line)
{
  const char *begin_members[] = { "beginLine", "startLine", "line", "range", NULL };
  const char *end_members[] = { "endLine", "stopLine", "line", "range", NULL };
  gint64 begin_value = 0;
  gint64 end_value = 0;
  gboolean has_begin;
  gboolean has_end;

  has_begin = foundry_acp_session_update_get_member_int (self, begin_members, &begin_value);
  has_end = foundry_acp_session_update_get_member_int (self, end_members, &end_value);

  if (!has_begin && !has_end)
    return FALSE;

  if (begin_line != NULL)
    *begin_line = MAX (begin_value, 0);

  if (end_line != NULL)
    *end_line = MAX (has_end ? end_value : begin_value, 0);

  return TRUE;
}

/**
 * foundry_acp_session_update_get_byte_count:
 * @self: a [class@Foundry.AcpSessionUpdate]
 * @byte_count: (out) (optional): location for the byte count
 *
 * Gets the byte count from @self, if present.
 *
 * Returns: %TRUE if a byte count was found
 *
 * Since: 1.2
 */
gboolean
foundry_acp_session_update_get_byte_count (FoundryAcpSessionUpdate *self,
                                           gint64                  *byte_count)
{
  const char *members[] = { "byteCount", "byte_count", "bytes", "size", NULL };

  return foundry_acp_session_update_get_member_int (self, members, byte_count);
}

/**
 * foundry_acp_session_update_dup_edit_summary:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full) (nullable): a summary of the file edit
 *
 * Since: 1.2
 */
char *
foundry_acp_session_update_dup_edit_summary (FoundryAcpSessionUpdate *self)
{
  const char *members[] = { "editSummary", "edit_summary", "summary", "diffStat", NULL };

  return foundry_acp_session_update_dup_member (self, members);
}

/**
 * foundry_acp_session_update_dup_terminal_id:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full) (nullable): a terminal identifier from the update
 *
 * Since: 1.2
 */
char *
foundry_acp_session_update_dup_terminal_id (FoundryAcpSessionUpdate *self)
{
  const char *members[] = { "terminalId", "terminal_id", "terminal", NULL };

  return foundry_acp_session_update_dup_member (self, members);
}

/**
 * foundry_acp_session_update_dup_command:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full) (nullable): a command from the update
 *
 * Since: 1.2
 */
char *
foundry_acp_session_update_dup_command (FoundryAcpSessionUpdate *self)
{
  const char *members[] = { "command", "cmd", "terminal", NULL };

  return foundry_acp_session_update_dup_member (self, members);
}

/**
 * foundry_acp_session_update_dup_cwd:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full) (nullable): a working directory from the update
 *
 * Since: 1.2
 */
char *
foundry_acp_session_update_dup_cwd (FoundryAcpSessionUpdate *self)
{
  const char *members[] = { "cwd", "workingDirectory", "terminal", NULL };

  return foundry_acp_session_update_dup_member (self, members);
}

/**
 * foundry_acp_session_update_get_terminal_exit_status:
 * @self: a [class@Foundry.AcpSessionUpdate]
 * @exit_status: (out) (optional): location for the terminal exit status
 *
 * Gets the terminal exit status from @self, if present.
 *
 * Returns: %TRUE if an exit status was found
 *
 * Since: 1.2
 */
gboolean
foundry_acp_session_update_get_terminal_exit_status (FoundryAcpSessionUpdate *self,
                                                     int                     *exit_status)
{
  const char *members[] = { "exitStatus", "exit_status", "exitCode", "exit_code", "terminal", NULL };
  gint64 value = 0;

  if (!foundry_acp_session_update_get_member_int (self, members, &value))
    return FALSE;

  if (exit_status != NULL)
    *exit_status = value;

  return TRUE;
}

/**
 * foundry_acp_session_update_dup_progress_text:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full) (nullable): progress or status text from the update
 *
 * Since: 1.2
 */
char *
foundry_acp_session_update_dup_progress_text (FoundryAcpSessionUpdate *self)
{
  const char *members[] = { "progress", "statusText", "status_text", "status", "message", "terminal", NULL };

  return foundry_acp_session_update_dup_member (self, members);
}

/**
 * foundry_acp_session_update_dup_error_message:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full) (nullable): an error message from the update
 *
 * Since: 1.2
 */
char *
foundry_acp_session_update_dup_error_message (FoundryAcpSessionUpdate *self)
{
  const char *members[] = { "message", "error", "errorMessage", NULL };

  return foundry_acp_session_update_dup_member (self, members);
}

/**
 * foundry_acp_session_update_dup_error_domain:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full) (nullable): the error domain from the update
 *
 * Since: 1.2
 */
char *
foundry_acp_session_update_dup_error_domain (FoundryAcpSessionUpdate *self)
{
  const char *members[] = { "domain", "errorDomain", "error_domain", "error", NULL };

  return foundry_acp_session_update_dup_member (self, members);
}

/**
 * foundry_acp_session_update_get_error_code:
 * @self: a [class@Foundry.AcpSessionUpdate]
 * @error_code: (out) (optional): location for the error code
 *
 * Gets the error code from @self, if present.
 *
 * Returns: %TRUE if an error code was found
 *
 * Since: 1.2
 */
gboolean
foundry_acp_session_update_get_error_code (FoundryAcpSessionUpdate *self,
                                           int                     *error_code)
{
  const char *members[] = { "code", "errorCode", "error_code", "error", NULL };
  gint64 value = 0;

  if (!foundry_acp_session_update_get_member_int (self, members, &value))
    return FALSE;

  if (error_code != NULL)
    *error_code = value;

  return TRUE;
}

/**
 * foundry_acp_session_update_dup_raw:
 * @self: a [class@Foundry.AcpSessionUpdate]
 *
 * Returns: (transfer full): the raw update JSON
 *
 * Since: 1.2
 */
JsonNode *
foundry_acp_session_update_dup_raw (FoundryAcpSessionUpdate *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION_UPDATE (self), NULL);

  return json_node_ref (self->raw);
}

char *
_foundry_acp_session_update_dup_session_id (FoundryAcpSessionUpdate *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION_UPDATE (self), NULL);

  return g_strdup (self->session_id);
}

JsonNode *
_foundry_acp_session_update_dup_raw (FoundryAcpSessionUpdate *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_SESSION_UPDATE (self), NULL);

  return json_node_ref (self->raw);
}
