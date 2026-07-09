/* test-acp-gtk.c
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

#include <foundry.h>
#include <gtk/gtk.h>

static GMainLoop *main_loop;
static const char *dirpath;
static FoundryContext *context;
static FoundryBuildPipeline *pipeline;
static FoundryAcpConnection *connection;
static FoundryAcpSession *current_session;
static GObject *current_client;
static GtkWindow *main_window;
static GtkDropDown *agent_drop_down;
static GtkNoSelection *session_selection;
static GtkNoSelection *event_selection;
static GtkNoSelection *terminal_selection;
static GtkNoSelection *changed_file_selection;
static GtkStack *send_stack;
static GtkTextBuffer *prompt_buffer;
static GListStore *messages;
static guint opening_session;

typedef struct _TestAcpMessage
{
  GObject parent_instance;
  char *title;
  char *body;
} TestAcpMessage;

typedef struct _TestAcpMessageClass
{
  GObjectClass parent_class;
} TestAcpMessageClass;

GType test_acp_message_get_type (void);

G_DEFINE_FINAL_TYPE (TestAcpMessage, test_acp_message, G_TYPE_OBJECT)

G_DEFINE_AUTOPTR_CLEANUP_FUNC (TestAcpMessage, g_object_unref)

enum {
  PROP_MESSAGE_0,
  PROP_MESSAGE_TITLE,
  PROP_MESSAGE_BODY,
  N_MESSAGE_PROPS
};

static GParamSpec *message_props[N_MESSAGE_PROPS];

typedef struct _TestAcpClient
{
  GObject parent_instance;
  GListStore *messages;
  FoundryAcpProjectClient *project_client;
  TestAcpMessage *agent_message;
} TestAcpClient;

typedef struct _TestAcpClientClass
{
  GObjectClass parent_class;
} TestAcpClientClass;

GType test_acp_client_get_type (void);

static void test_acp_client_iface_init (FoundryAcpClientInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (TestAcpClient, test_acp_client, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (FOUNDRY_TYPE_ACP_CLIENT,
                                                      test_acp_client_iface_init))

typedef struct _PermissionChoice
{
  DexPromise *promise;
  GtkAlertDialog *dialog;
  GListModel *options;
} PermissionChoice;

static void
permission_choice_free (PermissionChoice *state)
{
  dex_clear (&state->promise);
  g_clear_object (&state->dialog);
  g_clear_object (&state->options);
  g_free (state);
}

static void
permission_choice_done (GtkAlertDialog *dialog,
                        GAsyncResult   *result,
                        gpointer        user_data)
{
  PermissionChoice *state = user_data;
  g_autoptr(FoundryAcpPermissionResponse) response = NULL;
  g_autoptr(GError) error = NULL;
  int choice;

  g_assert (GTK_IS_ALERT_DIALOG (dialog));
  g_assert (state != NULL);
  g_assert (DEX_IS_PROMISE (state->promise));
  g_assert (G_IS_LIST_MODEL (state->options));

  choice = gtk_alert_dialog_choose_finish (dialog, result, &error);

  if (error != NULL || choice < 0 || choice >= g_list_model_get_n_items (state->options))
    {
      response = foundry_acp_permission_response_new_cancelled ();
      dex_promise_resolve_object (state->promise, g_steal_pointer (&response));
      permission_choice_free (state);
      return;
    }

  {
    g_autoptr(FoundryAcpPermissionOption) option = NULL;
    g_autofree char *option_id = NULL;

    option = g_list_model_get_item (state->options, choice);
    option_id = foundry_acp_permission_option_dup_id (option);
    response = foundry_acp_permission_response_new_selected (option_id);
  }

  dex_promise_resolve_object (state->promise, g_steal_pointer (&response));
  permission_choice_free (state);
}

static char *
update_kind_to_title (const char *kind)
{
  char **parts = NULL;
  char *ret;
  GString *str;

  if (kind == NULL || kind[0] == 0)
    return g_strdup ("Update");

  if (g_strcmp0 (kind, "agent_message_chunk") == 0 ||
      g_strcmp0 (kind, "agent_message") == 0)
    return g_strdup ("Codex");

  if (g_strcmp0 (kind, "tool_call") == 0)
    return g_strdup ("Tool call");

  if (g_strcmp0 (kind, "tool_call_update") == 0)
    return g_strdup ("Tool update");

  if (g_strcmp0 (kind, "available_tools") == 0 ||
      g_strcmp0 (kind, "tool_availability_update") == 0)
    return g_strdup ("Tools");

  parts = g_strsplit (kind, "_", -1);
  str = g_string_new (NULL);

  for (guint i = 0; parts[i] != NULL; i++)
    {
      char *part = parts[i];

      if (part[0] == 0)
        continue;

      if (str->len > 0)
        g_string_append_c (str, ' ');

      part[0] = g_ascii_toupper (part[0]);
      g_string_append (str, part);
    }

  ret = g_string_free (str, FALSE);
  g_strfreev (parts);

  return ret;
}

static const char *
update_kind_to_fallback_title (FoundryAcpSessionUpdateKind kind)
{
  switch (kind)
    {
    case FOUNDRY_ACP_SESSION_UPDATE_MESSAGE_CHUNK:
    case FOUNDRY_ACP_SESSION_UPDATE_MESSAGE:
      return "Codex";

    case FOUNDRY_ACP_SESSION_UPDATE_STEP:
      return "Step";

    case FOUNDRY_ACP_SESSION_UPDATE_TOOL_CALL:
      return "Tool Call";

    case FOUNDRY_ACP_SESSION_UPDATE_TOOL_UPDATE:
      return "Tool Update";

    case FOUNDRY_ACP_SESSION_UPDATE_TOOL_RESULT:
      return "Tool Result";

    case FOUNDRY_ACP_SESSION_UPDATE_TERMINAL_CREATED:
      return "Terminal Created";

    case FOUNDRY_ACP_SESSION_UPDATE_TERMINAL_OUTPUT:
      return "Terminal Output";

    case FOUNDRY_ACP_SESSION_UPDATE_TERMINAL_EXITED:
      return "Terminal Exited";

    case FOUNDRY_ACP_SESSION_UPDATE_FILE_READ:
      return "File Read";

    case FOUNDRY_ACP_SESSION_UPDATE_FILE_WRITE:
      return "File Write";

    case FOUNDRY_ACP_SESSION_UPDATE_FILE_PATCH:
      return "File Patch";

    case FOUNDRY_ACP_SESSION_UPDATE_FILE_CREATED:
      return "File Created";

    case FOUNDRY_ACP_SESSION_UPDATE_FILE_DELETED:
      return "File Deleted";

    case FOUNDRY_ACP_SESSION_UPDATE_PROGRESS:
      return "Progress";

    case FOUNDRY_ACP_SESSION_UPDATE_ERROR:
      return "Error";

    case FOUNDRY_ACP_SESSION_UPDATE_UNKNOWN:
    default:
      return "Update";
    }
}

static void
append_field (GString    *str,
              const char *label,
              const char *value)
{
  g_assert (str != NULL);
  g_assert (label != NULL);

  if (value == NULL || value[0] == 0)
    return;

  if (str->len > 0)
    g_string_append_c (str, '\n');

  g_string_append_printf (str, "%s: %s", label, value);
}

static void
append_int_field (GString    *str,
                  const char *label,
                  int         value)
{
  g_autofree char *text = NULL;

  g_assert (str != NULL);
  g_assert (label != NULL);

  text = g_strdup_printf ("%d", value);
  append_field (str, label, text);
}

static void
append_int64_field (GString    *str,
                    const char *label,
                    gint64      value)
{
  g_autofree char *text = NULL;

  g_assert (str != NULL);
  g_assert (label != NULL);

  if (value == 0)
    return;

  text = g_strdup_printf ("%" G_GINT64_FORMAT, value);
  append_field (str, label, text);
}

static char *
enum_value_dup_nick (GType type,
                     int   value)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;
  char *ret = NULL;

  enum_class = g_type_class_ref (type);
  enum_value = g_enum_get_value (enum_class, value);

  if (enum_value != NULL)
    ret = g_strdup (enum_value->value_nick);

  g_type_class_unref (enum_class);

  return ret ? ret : g_strdup ("unknown");
}

static char *
session_update_dup_body (FoundryAcpSessionUpdate *update)
{
  g_autoptr(GPtrArray) content_blocks = NULL;
  g_autofree char *command = NULL;
  g_autofree char *cwd = NULL;
  g_autofree char *edit_summary = NULL;
  g_autofree char *error_domain = NULL;
  g_autofree char *error_message = NULL;
  g_autofree char *kind = NULL;
  g_autofree char *old_path = NULL;
  g_autofree char *path = NULL;
  g_autofree char *progress = NULL;
  g_autofree char *terminal_id = NULL;
  g_autofree char *text = NULL;
  g_autofree char *tool_call_id = NULL;
  g_autofree char *tool_name = NULL;
  g_autofree char *tool_status = NULL;
  g_autofree char *tool_title = NULL;
  GString *str;
  gint64 byte_count;
  guint begin_line;
  guint end_line;
  int error_code;
  int exit_status;

  g_assert (update != NULL);

  str = g_string_new (NULL);

  text = foundry_acp_session_update_dup_text (update);
  append_field (str, "Text", text);

  progress = foundry_acp_session_update_dup_progress_text (update);
  if (g_strcmp0 (progress, text) != 0)
    append_field (str, "Progress", progress);

  tool_title = foundry_acp_session_update_dup_tool_title (update);
  tool_name = foundry_acp_session_update_dup_tool_name (update);
  tool_call_id = foundry_acp_session_update_dup_tool_call_id (update);
  tool_status = foundry_acp_session_update_dup_tool_status (update);
  append_field (str, "Tool", tool_title);
  append_field (str, "Tool Name", tool_name);
  append_field (str, "Tool Call", tool_call_id);
  append_field (str, "Status", tool_status);

  path = foundry_acp_session_update_dup_path (update);
  old_path = foundry_acp_session_update_dup_old_path (update);
  edit_summary = foundry_acp_session_update_dup_edit_summary (update);
  append_field (str, "Path", path);
  append_field (str, "Old Path", old_path);

  if (foundry_acp_session_update_get_line_range (update, &begin_line, &end_line))
    {
      g_autofree char *range = NULL;

      if (begin_line == end_line)
        range = g_strdup_printf ("%u", begin_line);
      else
        range = g_strdup_printf ("%u-%u", begin_line, end_line);

      append_field (str, "Lines", range);
    }

  if (foundry_acp_session_update_get_byte_count (update, &byte_count))
    {
      g_autofree char *bytes = g_format_size (byte_count);

      append_field (str, "Bytes", bytes);
    }

  append_field (str, "Edit", edit_summary);

  terminal_id = foundry_acp_session_update_dup_terminal_id (update);
  command = foundry_acp_session_update_dup_command (update);
  cwd = foundry_acp_session_update_dup_cwd (update);
  append_field (str, "Terminal", terminal_id);
  append_field (str, "Command", command);
  append_field (str, "Directory", cwd);

  if (foundry_acp_session_update_get_terminal_exit_status (update, &exit_status))
    append_int_field (str, "Exit Status", exit_status);

  error_message = foundry_acp_session_update_dup_error_message (update);
  error_domain = foundry_acp_session_update_dup_error_domain (update);
  append_field (str, "Error", error_message);
  append_field (str, "Error Domain", error_domain);

  if (foundry_acp_session_update_get_error_code (update, &error_code))
    append_int_field (str, "Error Code", error_code);

  content_blocks = foundry_acp_session_update_dup_content_blocks (update);

  for (guint i = 0; i < content_blocks->len; i++)
    {
      FoundryAcpContentBlock *block = g_ptr_array_index (content_blocks, i);
      g_autofree char *block_name = NULL;
      g_autofree char *block_text = NULL;
      g_autofree char *block_uri = NULL;

      block_uri = foundry_acp_content_block_dup_uri (block);
      block_name = foundry_acp_content_block_dup_name (block);
      block_text = foundry_acp_content_block_dup_text (block);

      if (block_uri != NULL)
        append_field (str, block_name ? block_name : "Resource", block_uri);
      else if (block_text != NULL && g_strcmp0 (block_text, text) != 0)
        append_field (str, "Content", block_text);
    }

  if (str->len == 0)
    {
      kind = foundry_acp_session_update_dup_kind (update);
      append_field (str, "Kind", kind);
    }

  return g_string_free (str, FALSE);
}

static const char *
changed_file_kind_to_string (FoundryAcpChangedFileKind kind)
{
  switch (kind)
    {
    case FOUNDRY_ACP_CHANGED_FILE_CREATED:
      return "Created";

    case FOUNDRY_ACP_CHANGED_FILE_MODIFIED:
      return "Modified";

    case FOUNDRY_ACP_CHANGED_FILE_DELETED:
      return "Deleted";

    case FOUNDRY_ACP_CHANGED_FILE_PATCHED:
      return "Patched";

    case FOUNDRY_ACP_CHANGED_FILE_UNKNOWN:
    default:
      return "Changed";
    }
}

static char *
changed_file_flags_dup_string (FoundryAcpChangedFileFlags flags)
{
  GString *str;

  str = g_string_new (NULL);

  if ((flags & FOUNDRY_ACP_CHANGED_FILE_STAGED) != 0)
    g_string_append (str, "staged");

  if ((flags & FOUNDRY_ACP_CHANGED_FILE_UNSTAGED) != 0)
    {
      if (str->len > 0)
        g_string_append (str, ", ");

      g_string_append (str, "unstaged");
    }

  if ((flags & FOUNDRY_ACP_CHANGED_FILE_UNTRACKED) != 0)
    {
      if (str->len > 0)
        g_string_append (str, ", ");

      g_string_append (str, "untracked");
    }

  return g_string_free (str, FALSE);
}

static char *
changed_files_dup_body (GListModel *changed_files)
{
  GString *str;
  guint n_items;

  g_assert (G_IS_LIST_MODEL (changed_files));

  n_items = g_list_model_get_n_items (changed_files);

  if (n_items == 0)
    return NULL;

  str = g_string_new (NULL);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryAcpChangedFile) changed_file = NULL;
      g_autofree char *flags_text = NULL;
      g_autofree char *path = NULL;
      FoundryAcpChangedFileFlags flags;
      FoundryAcpChangedFileKind kind;

      changed_file = g_list_model_get_item (changed_files, i);
      path = foundry_acp_changed_file_dup_path (changed_file);
      kind = foundry_acp_changed_file_get_kind (changed_file);
      flags = foundry_acp_changed_file_get_flags (changed_file);
      flags_text = changed_file_flags_dup_string (flags);

      if (str->len > 0)
        g_string_append_c (str, '\n');

      if (flags_text[0] != 0)
        g_string_append_printf (str,
                                "%s: %s (%s)",
                                changed_file_kind_to_string (kind),
                                path,
                                flags_text);
      else
        g_string_append_printf (str,
                                "%s: %s",
                                changed_file_kind_to_string (kind),
                                path);
    }

  return g_string_free (str, FALSE);
}

static char *
event_dup_title (FoundryAcpEvent *event)
{
  g_autofree char *title = NULL;
  g_autofree char *raw_kind = NULL;

  g_assert (FOUNDRY_IS_ACP_EVENT (event));

  title = foundry_acp_event_dup_title (event);

  if (title != NULL && title[0] != 0)
    return g_steal_pointer (&title);

  raw_kind = foundry_acp_event_dup_raw_kind (event);

  if (raw_kind != NULL && raw_kind[0] != 0)
    return g_steal_pointer (&raw_kind);

  return enum_value_dup_nick (FOUNDRY_TYPE_ACP_EVENT_KIND,
                              foundry_acp_event_get_kind (event));
}

static char *
event_dup_body (FoundryAcpEvent *event)
{
  g_autofree char *body = NULL;
  g_autofree char *event_id = NULL;
  g_autofree char *parent_event_id = NULL;
  g_autofree char *raw_json = NULL;
  g_autofree char *state = NULL;
  g_autofree char *turn_id = NULL;
  g_autoptr(JsonNode) raw = NULL;
  GString *str;

  g_assert (FOUNDRY_IS_ACP_EVENT (event));

  str = g_string_new (NULL);

  body = foundry_acp_event_dup_body (event);
  append_field (str, "Body", body);

  turn_id = foundry_acp_event_dup_turn_id (event);
  event_id = foundry_acp_event_dup_event_id (event);
  parent_event_id = foundry_acp_event_dup_parent_event_id (event);
  append_field (str, "Turn", turn_id);
  append_field (str, "Event", event_id);
  append_field (str, "Parent", parent_event_id);
  state = enum_value_dup_nick (FOUNDRY_TYPE_ACP_EVENT_STATE,
                               foundry_acp_event_get_state (event));
  append_field (str, "State", state);

  raw = foundry_acp_event_dup_raw (event);

  if (raw != NULL)
    {
      raw_json = json_to_string (raw, TRUE);
      append_field (str, "Raw", raw_json);
    }

  return g_string_free (str, FALSE);
}

static char *
terminal_dup_title (FoundryAcpTerminal *terminal)
{
  g_autofree char *command = NULL;
  g_autofree char *id = NULL;

  g_assert (FOUNDRY_IS_ACP_TERMINAL (terminal));

  command = foundry_acp_terminal_dup_command (terminal);

  if (command != NULL && command[0] != 0)
    return g_steal_pointer (&command);

  id = foundry_acp_terminal_dup_id (terminal);

  return g_strdup_printf ("Terminal %s", id ? id : "");
}

static char *
terminal_dup_body (FoundryAcpTerminal *terminal)
{
  g_autofree char *argv_text = NULL;
  g_autofree char *cwd = NULL;
  g_autofree char *exit_signal = NULL;
  g_autofree char *latest_output = NULL;
  g_autofree char *scrollback = NULL;
  g_autofree char *state = NULL;
  g_autofree char *turn_id = NULL;
  char **argv = NULL;
  GString *str;

  g_assert (FOUNDRY_IS_ACP_TERMINAL (terminal));

  str = g_string_new (NULL);

  state = enum_value_dup_nick (FOUNDRY_TYPE_ACP_TERMINAL_STATE,
                               foundry_acp_terminal_get_state (terminal));
  append_field (str, "State", state);

  argv = foundry_acp_terminal_dup_argv (terminal);

  if (argv != NULL)
    argv_text = g_strjoinv (" ", argv);

  cwd = foundry_acp_terminal_dup_cwd (terminal);
  turn_id = foundry_acp_terminal_dup_turn_id (terminal);
  latest_output = foundry_acp_terminal_dup_latest_output (terminal);
  scrollback = foundry_acp_terminal_dup_scrollback (terminal);
  exit_signal = foundry_acp_terminal_dup_exit_signal (terminal);
  append_field (str, "Arguments", argv_text);
  append_field (str, "Directory", cwd);
  append_field (str, "Turn", turn_id);
  append_int64_field (str, "Created", foundry_acp_terminal_get_created_at (terminal));
  append_int64_field (str, "Started", foundry_acp_terminal_get_started_at (terminal));
  append_int64_field (str, "Exited", foundry_acp_terminal_get_exited_at (terminal));
  append_int64_field (str,
                      "Output Limit",
                      foundry_acp_terminal_get_output_byte_limit (terminal));
  append_field (str, "Output", latest_output);
  append_field (str, "Scrollback", scrollback);

  if (foundry_acp_terminal_get_truncated (terminal))
    append_field (str, "Truncated", "yes");

  if (foundry_acp_terminal_has_exit_status (terminal))
    append_int_field (str, "Exit Status", foundry_acp_terminal_get_exit_status (terminal));

  append_field (str, "Exit Signal", exit_signal);
  g_clear_pointer (&argv, g_strfreev);

  return g_string_free (str, FALSE);
}

static char *
changed_file_dup_title (FoundryAcpChangedFile *changed_file)
{
  g_autofree char *path = NULL;

  g_assert (FOUNDRY_IS_ACP_CHANGED_FILE (changed_file));

  path = foundry_acp_changed_file_dup_path (changed_file);

  return g_strdup_printf
    ("%s: %s",
     changed_file_kind_to_string (foundry_acp_changed_file_get_kind (changed_file)),
     path ? path : "");
}

static char *
changed_file_dup_body (FoundryAcpChangedFile *changed_file)
{
  g_autofree char *flags = NULL;
  g_autofree char *last_event_id = NULL;
  GString *str;

  g_assert (FOUNDRY_IS_ACP_CHANGED_FILE (changed_file));

  str = g_string_new (NULL);
  flags = changed_file_flags_dup_string (foundry_acp_changed_file_get_flags (changed_file));
  last_event_id = foundry_acp_changed_file_dup_last_event_id (changed_file);
  append_field (str, "Flags", flags);
  append_field (str, "Last Event", last_event_id);

  return g_string_free (str, FALSE);
}

static void
test_acp_message_set_title (TestAcpMessage *self,
                            const char     *title)
{
  g_assert (test_acp_message_get_type () == G_OBJECT_TYPE (self));

  if (g_set_str (&self->title, title ? title : ""))
    g_object_notify_by_pspec (G_OBJECT (self), message_props[PROP_MESSAGE_TITLE]);
}

static void
test_acp_message_set_body (TestAcpMessage *self,
                           const char     *body)
{
  g_assert (test_acp_message_get_type () == G_OBJECT_TYPE (self));

  if (g_set_str (&self->body, body ? body : ""))
    g_object_notify_by_pspec (G_OBJECT (self), message_props[PROP_MESSAGE_BODY]);
}

static void
test_acp_message_append_body (TestAcpMessage *self,
                              const char     *text)
{
  g_autofree char *body = NULL;

  g_assert (test_acp_message_get_type () == G_OBJECT_TYPE (self));

  if (text == NULL || text[0] == 0)
    return;

  if (self->body == NULL || self->body[0] == 0)
    body = g_strdup (text);
  else
    body = g_strconcat (self->body, text, NULL);

  test_acp_message_set_body (self, body);
}

static void
test_acp_message_finalize (GObject *object)
{
  TestAcpMessage *self = (TestAcpMessage *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->body, g_free);

  G_OBJECT_CLASS (test_acp_message_parent_class)->finalize (object);
}

static void
test_acp_message_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  TestAcpMessage *self = (TestAcpMessage *)object;

  switch (prop_id)
    {
    case PROP_MESSAGE_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_MESSAGE_BODY:
      g_value_set_string (value, self->body);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_acp_message_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  TestAcpMessage *self = (TestAcpMessage *)object;

  switch (prop_id)
    {
    case PROP_MESSAGE_TITLE:
      test_acp_message_set_title (self, g_value_get_string (value));
      break;

    case PROP_MESSAGE_BODY:
      test_acp_message_set_body (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_acp_message_class_init (TestAcpMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = test_acp_message_finalize;
  object_class->get_property = test_acp_message_get_property;
  object_class->set_property = test_acp_message_set_property;

  message_props[PROP_MESSAGE_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  message_props[PROP_MESSAGE_BODY] =
    g_param_spec_string ("body", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_MESSAGE_PROPS, message_props);
}

static void
test_acp_message_init (TestAcpMessage *self)
{
}

static TestAcpMessage *
test_acp_message_new (const char *title,
                      const char *body)
{
  return g_object_new (test_acp_message_get_type (),
                       "title", title,
                       "body", body,
                       NULL);
}

static DexFuture *
test_acp_client_session_update (FoundryAcpClient        *client,
                                FoundryAcpSession       *acp_session,
                                FoundryAcpSessionUpdate *update)
{
  TestAcpClient *self = (TestAcpClient *)client;
  g_autoptr(TestAcpMessage) message = NULL;
  FoundryAcpSessionUpdateKind update_kind;
  g_autofree char *body = NULL;
  g_autofree char *kind = NULL;
  g_autofree char *title = NULL;
  g_autofree char *text = NULL;

  g_assert (test_acp_client_get_type () == G_OBJECT_TYPE (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (acp_session));
  g_assert (update != NULL);

  kind = foundry_acp_session_update_dup_kind (update);
  text = foundry_acp_session_update_dup_text (update);
  update_kind = foundry_acp_session_update_get_update_kind (update);

  if (update_kind == FOUNDRY_ACP_SESSION_UPDATE_MESSAGE_CHUNK)
    {
      if (self->agent_message == NULL)
        {
          self->agent_message = test_acp_message_new ("Codex", NULL);
          g_list_store_append (self->messages, self->agent_message);
        }

      test_acp_message_append_body (self->agent_message, text);

      return dex_future_new_true ();
    }

  g_clear_object (&self->agent_message);

  body = session_update_dup_body (update);

  if (body == NULL || body[0] == 0)
    return dex_future_new_true ();

  title = update_kind_to_title (kind);

  if (title == NULL || g_strcmp0 (title, "Update") == 0)
    {
      g_clear_pointer (&title, g_free);
      title = g_strdup (update_kind_to_fallback_title (update_kind));
    }

  message = test_acp_message_new (title, body);
  g_list_store_append (self->messages, message);

  return dex_future_new_true ();
}

static DexFuture *
test_acp_client_request_permission (FoundryAcpClient            *client,
                                    FoundryAcpSession           *acp_session,
                                    FoundryAcpPermissionRequest *request)
{
  TestAcpClient *self = (TestAcpClient *)client;
  PermissionChoice *state;
  DexPromise *promise;
  g_autoptr(GListModel) options = NULL;
  g_autoptr(TestAcpMessage) message = NULL;
  g_autofree char *title = NULL;
  g_autofree char *description = NULL;
  g_autofree char *default_option = NULL;
  g_autoptr(GPtrArray) labels = NULL;
  guint n_options;
  int default_button = -1;

  g_assert (test_acp_client_get_type () == G_OBJECT_TYPE (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (acp_session));
  g_assert (request != NULL);

  title = foundry_acp_permission_request_dup_title (request);
  description = foundry_acp_permission_request_dup_description (request);
  default_option = foundry_acp_permission_request_dup_default_option (request);
  options = foundry_acp_permission_request_list_options (request);
  n_options = g_list_model_get_n_items (options);

  message = test_acp_message_new (title ? title : "Permission request",
                                  description ? description : "The agent requested permission.");
  g_list_store_append (self->messages, message);

  if (n_options == 0)
    {
      g_autoptr(FoundryAcpPermissionResponse) response = NULL;

      response = foundry_acp_permission_response_new_cancelled ();

      return dex_future_new_take_object (g_steal_pointer (&response));
    }

  labels = g_ptr_array_new_with_free_func (g_free);

  for (guint i = 0; i < n_options; i++)
    {
      g_autoptr(FoundryAcpPermissionOption) option = NULL;
      g_autofree char *option_id = NULL;
      g_autofree char *label = NULL;

      option = g_list_model_get_item (options, i);
      option_id = foundry_acp_permission_option_dup_id (option);
      label = foundry_acp_permission_option_dup_label (option);

      if (default_option != NULL && g_str_equal (default_option, option_id))
        default_button = i;

      g_ptr_array_add (labels, g_strdup (label ? label : option_id));
    }

  g_ptr_array_add (labels, g_strdup ("Cancel"));
  g_ptr_array_add (labels, NULL);

  promise = dex_promise_new ();
  state = g_new0 (PermissionChoice, 1);
  state->promise = dex_ref (promise);
  state->dialog = gtk_alert_dialog_new ("%s", title ? title : "Permission request");
  state->options = g_object_ref (options);

  gtk_alert_dialog_set_modal (state->dialog, TRUE);
  gtk_alert_dialog_set_detail (state->dialog, description);
  gtk_alert_dialog_set_buttons (state->dialog, (const char * const *)labels->pdata);
  gtk_alert_dialog_set_cancel_button (state->dialog, n_options);
  gtk_alert_dialog_set_default_button (state->dialog, default_button);
  gtk_alert_dialog_choose (state->dialog,
                           main_window,
                           NULL,
                           (GAsyncReadyCallback)permission_choice_done,
                           state);

  return DEX_FUTURE (promise);
}

static DexFuture *
test_acp_client_read_text_file (FoundryAcpClient  *client,
                                FoundryAcpSession *acp_session,
                                const char        *path,
                                guint              line,
                                guint              limit)
{
  TestAcpClient *self = (TestAcpClient *)client;

  dex_return_error_if_fail (test_acp_client_get_type () == G_OBJECT_TYPE (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (acp_session));
  dex_return_error_if_fail (path != NULL);

  if (self->project_client == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_UNSUPPORTED,
                                  "No project client is available");

  return foundry_acp_client_read_text_file (FOUNDRY_ACP_CLIENT (self->project_client),
                                            acp_session,
                                            path,
                                            line,
                                            limit);
}

static DexFuture *
test_acp_client_write_text_file (FoundryAcpClient  *client,
                                 FoundryAcpSession *acp_session,
                                 const char        *path,
                                 const char        *content)
{
  TestAcpClient *self = (TestAcpClient *)client;

  dex_return_error_if_fail (test_acp_client_get_type () == G_OBJECT_TYPE (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (acp_session));
  dex_return_error_if_fail (path != NULL);
  dex_return_error_if_fail (content != NULL);

  if (self->project_client == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_UNSUPPORTED,
                                  "No project client is available");

  return foundry_acp_client_write_text_file (FOUNDRY_ACP_CLIENT (self->project_client),
                                             acp_session,
                                             path,
                                             content);
}

static DexFuture *
test_acp_client_refresh_changed_files (TestAcpClient     *self,
                                       FoundryAcpSession *acp_session)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GListModel) changed_files = NULL;
  g_autoptr(TestAcpMessage) message = NULL;
  g_autofree char *body = NULL;

  dex_return_error_if_fail (test_acp_client_get_type () == G_OBJECT_TYPE (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (acp_session));

  if (self->project_client == NULL)
    return dex_future_new_true ();

  if (!dex_await (foundry_acp_project_client_refresh_changed_files (self->project_client,
                                                                    acp_session),
                  &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  changed_files = foundry_acp_session_list_changed_files (acp_session);
  body = changed_files_dup_body (changed_files);

  if (body != NULL)
    {
      message = test_acp_message_new ("Changed Files", body);
      g_list_store_append (self->messages, message);
    }

  return dex_future_new_true ();
}

static DexFuture *
test_acp_client_create_terminal (FoundryAcpClient   *client,
                                 FoundryAcpSession  *acp_session,
                                 const char         *command,
                                 const char * const *argv,
                                 const char         *cwd,
                                 const char * const *environ,
                                 gssize              output_byte_limit)
{
  TestAcpClient *self = (TestAcpClient *)client;

  dex_return_error_if_fail (test_acp_client_get_type () == G_OBJECT_TYPE (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (acp_session));
  dex_return_error_if_fail (command != NULL);

  if (self->project_client == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_UNSUPPORTED,
                                  "No project client is available");

  return foundry_acp_client_create_terminal (FOUNDRY_ACP_CLIENT (self->project_client),
                                             acp_session,
                                             command,
                                             argv,
                                             cwd,
                                             environ,
                                             output_byte_limit);
}

static DexFuture *
test_acp_client_terminal_output (FoundryAcpClient  *client,
                                 FoundryAcpSession *acp_session,
                                 const char        *terminal_id)
{
  TestAcpClient *self = (TestAcpClient *)client;

  dex_return_error_if_fail (test_acp_client_get_type () == G_OBJECT_TYPE (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (acp_session));
  dex_return_error_if_fail (terminal_id != NULL);

  if (self->project_client == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_UNSUPPORTED,
                                  "No project client is available");

  return foundry_acp_client_terminal_output (FOUNDRY_ACP_CLIENT (self->project_client),
                                             acp_session,
                                             terminal_id);
}

static DexFuture *
test_acp_client_terminal_wait_for_exit_fiber (TestAcpClient     *self,
                                             FoundryAcpSession *acp_session,
                                             const char        *terminal_id)
{
  FoundryAcpClient *project_client = FOUNDRY_ACP_CLIENT (self->project_client);
  g_autoptr(GError) error = NULL;

  g_assert (test_acp_client_get_type () == G_OBJECT_TYPE (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (acp_session));
  g_assert (terminal_id != NULL);

  if (!dex_await (foundry_acp_client_terminal_wait_for_exit (project_client,
                                                            acp_session,
                                                            terminal_id),
                  &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return test_acp_client_refresh_changed_files (self, acp_session);
}

static DexFuture *
test_acp_client_terminal_wait_for_exit (FoundryAcpClient  *client,
                                        FoundryAcpSession *acp_session,
                                        const char        *terminal_id)
{
  TestAcpClient *self = (TestAcpClient *)client;

  dex_return_error_if_fail (test_acp_client_get_type () == G_OBJECT_TYPE (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (acp_session));
  dex_return_error_if_fail (terminal_id != NULL);

  if (self->project_client == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_UNSUPPORTED,
                                  "No project client is available");

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (test_acp_client_terminal_wait_for_exit_fiber),
                                  3,
                                  test_acp_client_get_type (), self,
                                  FOUNDRY_TYPE_ACP_SESSION, acp_session,
                                  G_TYPE_STRING, terminal_id);
}

static DexFuture *
test_acp_client_terminal_kill_fiber (TestAcpClient     *self,
                                     FoundryAcpSession *acp_session,
                                     const char        *terminal_id)
{
  FoundryAcpClient *project_client = FOUNDRY_ACP_CLIENT (self->project_client);
  g_autoptr(GError) error = NULL;

  g_assert (test_acp_client_get_type () == G_OBJECT_TYPE (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (acp_session));
  g_assert (terminal_id != NULL);

  if (!dex_await (foundry_acp_client_terminal_kill (project_client,
                                                   acp_session,
                                                   terminal_id),
                  &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return test_acp_client_refresh_changed_files (self, acp_session);
}

static DexFuture *
test_acp_client_terminal_kill (FoundryAcpClient  *client,
                               FoundryAcpSession *acp_session,
                               const char        *terminal_id)
{
  TestAcpClient *self = (TestAcpClient *)client;

  dex_return_error_if_fail (test_acp_client_get_type () == G_OBJECT_TYPE (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (acp_session));
  dex_return_error_if_fail (terminal_id != NULL);

  if (self->project_client == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_UNSUPPORTED,
                                  "No project client is available");

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (test_acp_client_terminal_kill_fiber),
                                  3,
                                  test_acp_client_get_type (), self,
                                  FOUNDRY_TYPE_ACP_SESSION, acp_session,
                                  G_TYPE_STRING, terminal_id);
}

static DexFuture *
test_acp_client_terminal_release (FoundryAcpClient  *client,
                                  FoundryAcpSession *acp_session,
                                  const char        *terminal_id)
{
  TestAcpClient *self = (TestAcpClient *)client;

  dex_return_error_if_fail (test_acp_client_get_type () == G_OBJECT_TYPE (self));
  dex_return_error_if_fail (FOUNDRY_IS_ACP_SESSION (acp_session));
  dex_return_error_if_fail (terminal_id != NULL);

  if (self->project_client == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_UNSUPPORTED,
                                  "No project client is available");

  return foundry_acp_client_terminal_release (FOUNDRY_ACP_CLIENT (self->project_client),
                                              acp_session,
                                              terminal_id);
}

static void
test_acp_client_iface_init (FoundryAcpClientInterface *iface)
{
  iface->session_update = test_acp_client_session_update;
  iface->request_permission = test_acp_client_request_permission;
  iface->read_text_file = test_acp_client_read_text_file;
  iface->write_text_file = test_acp_client_write_text_file;
  iface->create_terminal = test_acp_client_create_terminal;
  iface->terminal_output = test_acp_client_terminal_output;
  iface->terminal_wait_for_exit = test_acp_client_terminal_wait_for_exit;
  iface->terminal_kill = test_acp_client_terminal_kill;
  iface->terminal_release = test_acp_client_terminal_release;
}

static void
test_acp_client_finalize (GObject *object)
{
  TestAcpClient *self = (TestAcpClient *)object;

  g_clear_object (&self->agent_message);
  g_clear_object (&self->project_client);
  g_clear_object (&self->messages);

  G_OBJECT_CLASS (test_acp_client_parent_class)->finalize (object);
}

static void
test_acp_client_class_init (TestAcpClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = test_acp_client_finalize;
}

static void
test_acp_client_init (TestAcpClient *self)
{
  self->messages = g_object_ref (messages);

  if (context != NULL)
    self->project_client = foundry_acp_project_client_new (context);
}

static void
test_acp_client_reset_agent_message (TestAcpClient *self)
{
  g_return_if_fail (test_acp_client_get_type () == G_OBJECT_TYPE (self));

  g_clear_object (&self->agent_message);
}

static const char *
session_state_to_stack_child (FoundryAcpSession *acp_session)
{
  FoundryAcpSessionState state;

  if (acp_session == NULL)
    return "send";

  state = foundry_acp_session_get_state (acp_session);

  if (state == FOUNDRY_ACP_SESSION_RUNNING ||
      state == FOUNDRY_ACP_SESSION_CANCELLING)
    return "stop";

  return "send";
}

static void
sync_send_stack (void)
{
  gtk_stack_set_visible_child_name (send_stack, session_state_to_stack_child (current_session));
}

static void
setup_message_row (GtkSignalListItemFactory *factory,
                   GtkListItem              *item)
{
  GtkWidget *box;
  GtkWidget *title;
  GtkWidget *body;

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "spacing", 3,
                      "margin-top", 6,
                      "margin-bottom", 6,
                      "margin-start", 6,
                      "margin-end", 6,
                      NULL);
  title = g_object_new (GTK_TYPE_LABEL,
                        "xalign", 0.f,
                        "ellipsize", PANGO_ELLIPSIZE_END,
                        NULL);
  body = g_object_new (GTK_TYPE_LABEL,
                       "xalign", 0.f,
                       "wrap", TRUE,
                       "wrap-mode", PANGO_WRAP_WORD_CHAR,
                       "selectable", TRUE,
                       NULL);
  gtk_widget_add_css_class (title, "heading");
  gtk_box_append (GTK_BOX (box), title);
  gtk_box_append (GTK_BOX (box), body);
  gtk_list_item_set_child (item, box);
}

static void
bind_message_row (GtkSignalListItemFactory *factory,
                  GtkListItem              *item)
{
  TestAcpMessage *message = gtk_list_item_get_item (item);
  GtkWidget *box = gtk_list_item_get_child (item);
  GtkWidget *title = gtk_widget_get_first_child (box);
  GtkWidget *body = gtk_widget_get_next_sibling (title);
  GBinding *binding;

  binding = g_object_bind_property (message, "title",
                                    title, "label",
                                    G_BINDING_SYNC_CREATE);
  g_object_set_data_full (G_OBJECT (item),
                          "title-binding",
                          binding,
                          g_object_unref);

  binding = g_object_bind_property (message, "body",
                                    body, "label",
                                    G_BINDING_SYNC_CREATE);
  g_object_set_data_full (G_OBJECT (item),
                          "body-binding",
                          binding,
                          g_object_unref);
}

static void
unbind_message_row (GtkSignalListItemFactory *factory,
                    GtkListItem              *item)
{
  g_object_set_data (G_OBJECT (item), "title-binding", NULL);
  g_object_set_data (G_OBJECT (item), "body-binding", NULL);
}

static void
bind_event_row (GtkSignalListItemFactory *factory,
                GtkListItem              *item)
{
  FoundryAcpEvent *event = gtk_list_item_get_item (item);
  GtkWidget *box = gtk_list_item_get_child (item);
  GtkWidget *title = gtk_widget_get_first_child (box);
  GtkWidget *body = gtk_widget_get_next_sibling (title);
  g_autofree char *body_text = NULL;
  g_autofree char *title_text = NULL;

  title_text = event_dup_title (event);
  body_text = event_dup_body (event);

  gtk_label_set_label (GTK_LABEL (title), title_text);
  gtk_label_set_label (GTK_LABEL (body), body_text);
}

static void
bind_terminal_row (GtkSignalListItemFactory *factory,
                   GtkListItem              *item)
{
  FoundryAcpTerminal *terminal = gtk_list_item_get_item (item);
  GtkWidget *box = gtk_list_item_get_child (item);
  GtkWidget *title = gtk_widget_get_first_child (box);
  GtkWidget *body = gtk_widget_get_next_sibling (title);
  g_autofree char *body_text = NULL;
  g_autofree char *title_text = NULL;

  title_text = terminal_dup_title (terminal);
  body_text = terminal_dup_body (terminal);

  gtk_label_set_label (GTK_LABEL (title), title_text);
  gtk_label_set_label (GTK_LABEL (body), body_text);
}

static void
bind_changed_file_row (GtkSignalListItemFactory *factory,
                       GtkListItem              *item)
{
  FoundryAcpChangedFile *changed_file = gtk_list_item_get_item (item);
  GtkWidget *box = gtk_list_item_get_child (item);
  GtkWidget *title = gtk_widget_get_first_child (box);
  GtkWidget *body = gtk_widget_get_next_sibling (title);
  g_autofree char *body_text = NULL;
  g_autofree char *title_text = NULL;

  title_text = changed_file_dup_title (changed_file);
  body_text = changed_file_dup_body (changed_file);

  gtk_label_set_label (GTK_LABEL (title), title_text);
  gtk_label_set_label (GTK_LABEL (body), body_text);
}

static void
set_session_models (FoundryAcpSession *acp_session)
{
  g_autoptr(GListModel) events = NULL;
  g_autoptr(GListModel) terminals = NULL;
  g_autoptr(GListModel) changed_files = NULL;

  if (acp_session != NULL)
    {
      events = foundry_acp_session_list_events (acp_session);
      terminals = foundry_acp_session_list_active_terminals (acp_session);
      changed_files = foundry_acp_session_list_changed_files (acp_session);
    }

  if (event_selection != NULL)
    gtk_no_selection_set_model (event_selection, events ? g_object_ref (events) : NULL);

  if (terminal_selection != NULL)
    gtk_no_selection_set_model (terminal_selection, terminals ? g_object_ref (terminals) : NULL);

  if (changed_file_selection != NULL)
    gtk_no_selection_set_model (changed_file_selection,
                                changed_files ? g_object_ref (changed_files) : NULL);
}

static DexFuture *
open_session_fiber (gpointer user_data)
{
  g_autoptr(FoundryAcpAgent) agent = user_data;
  g_autoptr(FoundryAcpClient) client = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_ACP_AGENT (agent));

  opening_session = FALSE;

  g_clear_object (&current_session);
  g_clear_object (&current_client);
  set_session_models (NULL);

  if (connection != NULL)
    {
      dex_await (foundry_acp_connection_close (connection), NULL);
      g_clear_object (&connection);
    }

  g_list_store_remove_all (messages);
  sync_send_stack ();

  client = g_object_new (test_acp_client_get_type (), NULL);
  current_client = g_object_ref (G_OBJECT (client));

  if (!(current_session = dex_await_object (foundry_acp_agent_open_session (agent,
                                                                            client,
                                                                            pipeline),
                                            &error)))
    {
      g_autoptr(TestAcpMessage) message = NULL;

      message = test_acp_message_new ("Error", error->message);
      g_list_store_append (messages, message);
      g_clear_object (&connection);
    }
  else
    {
      connection = foundry_acp_session_dup_connection (current_session);
      set_session_models (current_session);
    }

  sync_send_stack ();

  return NULL;
}

static gboolean
open_selected_agent (void)
{
  GListModel *model;
  guint selected;
  g_autoptr(FoundryAcpAgent) agent = NULL;

  selected = gtk_drop_down_get_selected (agent_drop_down);

  if (selected == GTK_INVALID_LIST_POSITION)
    return FALSE;

  if (!(model = gtk_drop_down_get_model (agent_drop_down)))
    return FALSE;

  if (!(agent = g_list_model_get_item (model, selected)))
    return FALSE;

  if (opening_session)
    return TRUE;

  opening_session = TRUE;
  dex_future_disown (dex_scheduler_spawn (NULL, 0,
                                          open_session_fiber,
                                          g_steal_pointer (&agent),
                                          g_object_unref));
  return TRUE;
}

static void
on_selected_agent_changed (GtkDropDown *drop_down,
                           GParamSpec  *pspec)
{
  open_selected_agent ();
}

static DexFuture *
send_prompt_fiber (gpointer user_data)
{
  g_autoptr(FoundryAcpContentBlock) block = NULL;
  g_autoptr(FoundryAcpPromptResult) result = NULL;
  g_autoptr(TestAcpMessage) message = NULL;
  g_autoptr(GError) error = NULL;
  const char *text = user_data;
  FoundryAcpContentBlock *blocks[1];
  DexFuture *future;

  g_assert (text != NULL);

  if (current_session == NULL)
    return NULL;

  block = foundry_acp_content_block_new_text (text);
  blocks[0] = block;

  if (current_client != NULL)
    test_acp_client_reset_agent_message ((TestAcpClient *)current_client);

  message = test_acp_message_new ("You", text);
  g_list_store_append (messages, message);

  future = foundry_acp_session_prompt (current_session, blocks, G_N_ELEMENTS (blocks));
  sync_send_stack ();

  if (!(result = dex_await_object (future, &error)))
    {
      g_autoptr(TestAcpMessage) error_message = NULL;

      error_message = test_acp_message_new ("Error", error->message);
      g_list_store_append (messages, error_message);
    }
  else if (current_client != NULL)
    {
      if (!dex_await (test_acp_client_refresh_changed_files ((TestAcpClient *)current_client,
                                                            current_session),
                      &error))
        {
          g_autoptr(TestAcpMessage) error_message = NULL;

          error_message = test_acp_message_new ("Changed Files Error", error->message);
          g_list_store_append (messages, error_message);
        }
    }

  sync_send_stack ();

  return NULL;
}

static void
send_prompt (void)
{
  GtkTextIter begin;
  GtkTextIter end;
  g_autofree char *text = NULL;

  gtk_text_buffer_get_bounds (prompt_buffer, &begin, &end);
  text = gtk_text_buffer_get_text (prompt_buffer, &begin, &end, FALSE);

  if (text == NULL || text[0] == 0)
    return;

  if (current_session == NULL)
    {
      if (!open_selected_agent ())
        return;

      return;
    }

  gtk_text_buffer_set_text (prompt_buffer, "", -1);
  dex_future_disown (dex_scheduler_spawn (NULL, 0,
                                          send_prompt_fiber,
                                          g_steal_pointer (&text),
                                          g_free));
}

static DexFuture *
cancel_session_fiber (gpointer user_data)
{
  g_autoptr(GError) error = NULL;

  if (current_session != NULL && !dex_await (foundry_acp_session_cancel (current_session), &error))
    {
      g_autoptr(TestAcpMessage) message = NULL;

      message = test_acp_message_new ("Error", error->message);
      g_list_store_append (messages, message);
    }

  sync_send_stack ();

  return NULL;
}

static void
cancel_session (void)
{
  if (current_session == NULL)
    return;

  dex_future_disown (dex_scheduler_spawn (NULL, 0,
                                          cancel_session_fiber,
                                          NULL,
                                          NULL));
  sync_send_stack ();
}

static DexFuture *
main_fiber (gpointer data)
{
  g_autoptr(FoundryAcpManager) acp_manager = NULL;
  g_autoptr(FoundryBuildManager) build_manager = NULL;
  g_autoptr(GtkListItemFactory) changed_file_factory = NULL;
  g_autoptr(GtkListItemFactory) event_factory = NULL;
  g_autoptr(GtkListItemFactory) message_factory = NULL;
  g_autoptr(GtkListItemFactory) terminal_factory = NULL;
  g_autoptr(GtkExpression) expression = NULL;
  g_autoptr(GListModel) agents = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  GtkScrolledWindow *changed_file_scroller;
  GtkScrolledWindow *event_scroller;
  GtkScrolledWindow *session_scroller;
  GtkScrolledWindow *terminal_scroller;
  GtkScrolledWindow *prompt_scroller;
  GtkStackSwitcher *monitor_switcher;
  GtkTextView *prompt_view;
  GtkListView *changed_file_view;
  GtkListView *event_view;
  GtkListView *list_view;
  GtkListView *terminal_view;
  GtkWindow *window;
  GtkButton *send_button;
  GtkButton *stop_button;
  GtkBox *prompt_box;
  GtkStack *monitor_stack;
  GtkBox *box;

  dex_await (foundry_init (), NULL);

  if (!(path = dex_await_string (foundry_context_discover (dirpath, NULL), &error)))
    g_error ("%s", error->message);

  if (!(context = dex_await_object (foundry_context_new (path,
                                                         dirpath,
                                                         FOUNDRY_CONTEXT_FLAGS_NONE,
                                                         NULL),
                                    &error)))
    g_error ("%s", error->message);

  build_manager = foundry_context_dup_build_manager (context);

  if (!(pipeline = dex_await_object (foundry_build_manager_load_pipeline (build_manager),
                                    &error)))
    g_error ("%s", error->message);

  acp_manager = foundry_context_dup_acp_manager (context);

  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (acp_manager)), &error))
    g_error ("%s", error->message);

  agents = foundry_acp_manager_list_agents (acp_manager);
  messages = g_list_store_new (test_acp_message_get_type ());

  window = g_object_new (GTK_TYPE_WINDOW,
                         "default-width", 720,
                         "default-height", 640,
                         "title", "ACP",
                         NULL);
  main_window = window;
  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "spacing", 6,
                      "margin-top", 6,
                      "margin-bottom", 6,
                      "margin-start", 6,
                      "margin-end", 6,
                      NULL);
  gtk_window_set_child (window, GTK_WIDGET (box));

  expression = gtk_property_expression_new (FOUNDRY_TYPE_ACP_AGENT, NULL, "name");
  agent_drop_down = g_object_new (GTK_TYPE_DROP_DOWN,
                                  "model", agents,
                                  "expression", expression,
                                  NULL);
  gtk_box_append (box, GTK_WIDGET (agent_drop_down));

  monitor_stack = g_object_new (GTK_TYPE_STACK,
                                "vexpand", TRUE,
                                NULL);
  monitor_switcher = g_object_new (GTK_TYPE_STACK_SWITCHER,
                                   "stack", monitor_stack,
                                   NULL);
  gtk_box_append (box, GTK_WIDGET (monitor_switcher));
  gtk_box_append (box, GTK_WIDGET (monitor_stack));

  session_scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                   "vexpand", TRUE,
                                   NULL);
  gtk_stack_add_titled (monitor_stack, GTK_WIDGET (session_scroller), "transcript", "Transcript");

  message_factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (message_factory, "setup", G_CALLBACK (setup_message_row), NULL);
  g_signal_connect (message_factory, "bind", G_CALLBACK (bind_message_row), NULL);
  g_signal_connect (message_factory, "unbind", G_CALLBACK (unbind_message_row), NULL);

  session_selection = gtk_no_selection_new (g_object_ref (G_LIST_MODEL (messages)));
  list_view = g_object_new (GTK_TYPE_LIST_VIEW,
                            "factory", message_factory,
                            "model", session_selection,
                            NULL);
  gtk_scrolled_window_set_child (session_scroller, GTK_WIDGET (list_view));

  event_scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                 "vexpand", TRUE,
                                 NULL);
  gtk_stack_add_titled (monitor_stack, GTK_WIDGET (event_scroller), "events", "Events");

  event_factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (event_factory, "setup", G_CALLBACK (setup_message_row), NULL);
  g_signal_connect (event_factory, "bind", G_CALLBACK (bind_event_row), NULL);

  event_selection = gtk_no_selection_new (NULL);
  event_view = g_object_new (GTK_TYPE_LIST_VIEW,
                             "factory", event_factory,
                             "model", event_selection,
                             NULL);
  gtk_scrolled_window_set_child (event_scroller, GTK_WIDGET (event_view));

  terminal_scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                    "vexpand", TRUE,
                                    NULL);
  gtk_stack_add_titled (monitor_stack, GTK_WIDGET (terminal_scroller), "terminals", "Terminals");

  terminal_factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (terminal_factory, "setup", G_CALLBACK (setup_message_row), NULL);
  g_signal_connect (terminal_factory, "bind", G_CALLBACK (bind_terminal_row), NULL);

  terminal_selection = gtk_no_selection_new (NULL);
  terminal_view = g_object_new (GTK_TYPE_LIST_VIEW,
                                "factory", terminal_factory,
                                "model", terminal_selection,
                                NULL);
  gtk_scrolled_window_set_child (terminal_scroller, GTK_WIDGET (terminal_view));

  changed_file_scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                        "vexpand", TRUE,
                                        NULL);
  gtk_stack_add_titled (monitor_stack,
                        GTK_WIDGET (changed_file_scroller),
                        "changed-files",
                        "Changed Files");

  changed_file_factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (changed_file_factory, "setup", G_CALLBACK (setup_message_row), NULL);
  g_signal_connect (changed_file_factory, "bind", G_CALLBACK (bind_changed_file_row), NULL);

  changed_file_selection = gtk_no_selection_new (NULL);
  changed_file_view = g_object_new (GTK_TYPE_LIST_VIEW,
                                    "factory", changed_file_factory,
                                    "model", changed_file_selection,
                                    NULL);
  gtk_scrolled_window_set_child (changed_file_scroller, GTK_WIDGET (changed_file_view));

  prompt_box = g_object_new (GTK_TYPE_BOX,
                             "orientation", GTK_ORIENTATION_HORIZONTAL,
                             "spacing", 6,
                             "valign", GTK_ALIGN_FILL,
                             NULL);
  gtk_box_append (box, GTK_WIDGET (prompt_box));

  prompt_scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                  "hexpand", TRUE,
                                  "min-content-height", 80,
                                  NULL);
  gtk_box_append (prompt_box, GTK_WIDGET (prompt_scroller));

  prompt_view = g_object_new (GTK_TYPE_TEXT_VIEW,
                              "wrap-mode", GTK_WRAP_WORD_CHAR,
                              NULL);
  prompt_buffer = gtk_text_view_get_buffer (prompt_view);
  gtk_scrolled_window_set_child (prompt_scroller, GTK_WIDGET (prompt_view));

  send_stack = g_object_new (GTK_TYPE_STACK,
                             "valign", GTK_ALIGN_FILL,
                             NULL);
  gtk_box_append (prompt_box, GTK_WIDGET (send_stack));

  send_button = g_object_new (GTK_TYPE_BUTTON,
                              "label", "Send",
                              "valign", GTK_ALIGN_FILL,
                              NULL);
  gtk_widget_add_css_class (GTK_WIDGET (send_button), "suggested-action");
  gtk_stack_add_named (send_stack, GTK_WIDGET (send_button), "send");

  stop_button = g_object_new (GTK_TYPE_BUTTON,
                              "label", "Stop",
                              "valign", GTK_ALIGN_FILL,
                              NULL);
  gtk_widget_add_css_class (GTK_WIDGET (stop_button), "destructive-action");
  gtk_stack_add_named (send_stack, GTK_WIDGET (stop_button), "stop");
  gtk_stack_set_visible_child_name (send_stack, "send");

  g_signal_connect (agent_drop_down,
                    "notify::selected",
                    G_CALLBACK (on_selected_agent_changed),
                    NULL);
  g_signal_connect_swapped (send_button, "clicked", G_CALLBACK (send_prompt), NULL);
  g_signal_connect_swapped (stop_button, "clicked", G_CALLBACK (cancel_session), NULL);
  g_signal_connect_swapped (window,
                            "close-request",
                            G_CALLBACK (g_main_loop_quit),
                            main_loop);

  gtk_window_present (window);

  if (g_list_model_get_n_items (agents) > 0)
    open_selected_agent ();

  return NULL;
}

int
main (int   argc,
      char *argv[])
{
  dex_init ();

  if (argc != 2)
    {
      g_printerr ("usage: %s PROJECT_DIR\n", argv[0]);
      return 1;
    }

  dirpath = argv[1];

  gtk_init ();

  main_loop = g_main_loop_new (NULL, FALSE);
  dex_future_disown (dex_scheduler_spawn (NULL, 8*1024*1024, main_fiber, NULL, NULL));
  g_main_loop_run (main_loop);

  g_clear_object (&current_session);
  g_clear_object (&connection);
  g_clear_object (&changed_file_selection);
  g_clear_object (&terminal_selection);
  g_clear_object (&event_selection);
  g_clear_object (&session_selection);
  g_clear_object (&pipeline);
  g_clear_object (&context);
  g_clear_object (&messages);
  g_clear_pointer (&main_loop, g_main_loop_unref);

  return 0;
}
