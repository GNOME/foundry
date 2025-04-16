/* foundry-dap-initialize-request.c
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

#include "foundry-dap-initialize-request.h"
#include "foundry-dap-request-private.h"

struct _FoundryDapInitializeRequest
{
  FoundryDapRequest parent_instance;
  char *client_id;
  char *client_name;
  char *adapter_id;
  char *locale;
  char *path_format;
  guint lines_start_at_one : 1;
  guint columns_start_at_one : 1;
  guint supports_variable_type : 1;
  guint supports_variable_paging : 1;
  guint supports_run_in_terminal_request : 1;
  guint supports_memory_references : 1;
  guint supports_progress_reporting : 1;
  guint supports_invalidated_event : 1;
  guint supports_memory_event : 1;
  guint supports_args_can_be_interpreted_by_shell : 1;
  guint supports_start_debugging_request : 1;
  guint supports_ansistyling : 1;
};

struct _FoundryDapInitializeRequestClass
{
  FoundryDapRequestClass parent_class;
};

enum {
  PROP_0,
  PROP_CLIENT_ID,
  PROP_CLIENT_NAME,
  PROP_ADAPTER_ID,
  PROP_LOCALE,
  PROP_LINES_START_AT_ONE,
  PROP_COLUMNS_START_AT_ONE,
  PROP_PATH_FORMAT,
  PROP_SUPPORTS_VARIABLE_TYPE,
  PROP_SUPPORTS_VARIABLE_PAGING,
  PROP_SUPPORTS_RUN_IN_TERMINAL_REQUEST,
  PROP_SUPPORTS_MEMORY_REFERENCES,
  PROP_SUPPORTS_PROGRESS_REPORTING,
  PROP_SUPPORTS_INVALIDATED_EVENT,
  PROP_SUPPORTS_MEMORY_EVENT,
  PROP_SUPPORTS_ARGS_CAN_BE_INTERPRETED_BY_SHELL,
  PROP_SUPPORTS_START_DEBUGGING_REQUEST,
  PROP_SUPPORTS_ANSISTYLING,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryDapInitializeRequest, foundry_dap_initialize_request, FOUNDRY_TYPE_DAP_REQUEST)

static GParamSpec *properties[N_PROPS];

static gboolean
foundry_dap_initialize_request_serialize (FoundryDapProtocolMessage  *message,
                                          JsonObject                 *object,
                                          GError                    **error)
{
  FoundryDapInitializeRequest *self = FOUNDRY_DAP_INITIALIZE_REQUEST (message);

  json_object_set_string_member (object, "command", "initialize");

  if (self->client_id)
    json_object_set_string_member (object, "clientID", self->client_id);

  if (self->client_name)
    json_object_set_string_member (object, "clientName", self->client_name);

  if (self->adapter_id)
    json_object_set_string_member (object, "adapterID", self->adapter_id);

  if (self->locale)
    json_object_set_string_member (object, "locale", self->locale);

  if (self->path_format)
    json_object_set_string_member (object, "pathFormat", self->path_format);

  json_object_set_boolean_member (object, "linesStartAt1", self->lines_start_at_one);
  json_object_set_boolean_member (object, "columnsStartAt1", self->columns_start_at_one);
  json_object_set_boolean_member (object, "supportsVariableType", self->supports_variable_type);
  json_object_set_boolean_member (object, "supportsVariablePaging", self->supports_variable_paging);
  json_object_set_boolean_member (object, "supportsRunInTerminalRequest", self->supports_run_in_terminal_request);
  json_object_set_boolean_member (object, "supportsMemoryReferences", self->supports_memory_references);
  json_object_set_boolean_member (object, "supportsProgressReporting", self->supports_progress_reporting);
  json_object_set_boolean_member (object, "supportsInvalidatedEvent", self->supports_invalidated_event);
  json_object_set_boolean_member (object, "supportsMemoryEvent", self->supports_memory_event);
  json_object_set_boolean_member (object, "supportsArgsCanBeInterpretedByShell", self->supports_args_can_be_interpreted_by_shell);
  json_object_set_boolean_member (object, "supportsStartDebuggingRequest", self->supports_start_debugging_request);
  json_object_set_boolean_member (object, "supportsANSIStyling", self->supports_ansistyling);

  return FOUNDRY_DAP_PROTOCOL_MESSAGE_CLASS (foundry_dap_initialize_request_parent_class)->serialize (message, object, error);
}

static void
foundry_dap_initialize_request_finalize (GObject *object)
{
  FoundryDapInitializeRequest *self = (FoundryDapInitializeRequest *)object;

  g_clear_pointer (&self->client_id, g_free);
  g_clear_pointer (&self->client_name, g_free);
  g_clear_pointer (&self->adapter_id, g_free);
  g_clear_pointer (&self->locale, g_free);
  g_clear_pointer (&self->path_format, g_free);

  G_OBJECT_CLASS (foundry_dap_initialize_request_parent_class)->finalize (object);
}

static void
foundry_dap_initialize_request_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  FoundryDapInitializeRequest *self = FOUNDRY_DAP_INITIALIZE_REQUEST (object);

  switch (prop_id)
    {
    case PROP_CLIENT_ID:
      g_value_set_string (value, foundry_dap_initialize_request_get_client_id (self));
      break;

    case PROP_CLIENT_NAME:
      g_value_set_string (value, foundry_dap_initialize_request_get_client_name (self));
      break;

    case PROP_ADAPTER_ID:
      g_value_set_string (value, foundry_dap_initialize_request_get_adapter_id (self));
      break;

    case PROP_LOCALE:
      g_value_set_string (value, foundry_dap_initialize_request_get_locale (self));
      break;

    case PROP_LINES_START_AT_ONE:
      g_value_set_boolean (value, foundry_dap_initialize_request_get_lines_start_at_one (self));
      break;

    case PROP_COLUMNS_START_AT_ONE:
      g_value_set_boolean (value, foundry_dap_initialize_request_get_columns_start_at_one (self));
      break;

    case PROP_PATH_FORMAT:
      g_value_set_string (value, foundry_dap_initialize_request_get_path_format (self));
      break;

    case PROP_SUPPORTS_VARIABLE_TYPE:
      g_value_set_boolean (value, foundry_dap_initialize_request_get_supports_variable_type (self));
      break;

    case PROP_SUPPORTS_VARIABLE_PAGING:
      g_value_set_boolean (value, foundry_dap_initialize_request_get_supports_variable_paging (self));
      break;

    case PROP_SUPPORTS_RUN_IN_TERMINAL_REQUEST:
      g_value_set_boolean (value, foundry_dap_initialize_request_get_supports_run_in_terminal_request (self));
      break;

    case PROP_SUPPORTS_MEMORY_REFERENCES:
      g_value_set_boolean (value, foundry_dap_initialize_request_get_supports_memory_references (self));
      break;

    case PROP_SUPPORTS_PROGRESS_REPORTING:
      g_value_set_boolean (value, foundry_dap_initialize_request_get_supports_progress_reporting (self));
      break;

    case PROP_SUPPORTS_INVALIDATED_EVENT:
      g_value_set_boolean (value, foundry_dap_initialize_request_get_supports_invalidated_event (self));
      break;

    case PROP_SUPPORTS_MEMORY_EVENT:
      g_value_set_boolean (value, foundry_dap_initialize_request_get_supports_memory_event (self));
      break;

    case PROP_SUPPORTS_ARGS_CAN_BE_INTERPRETED_BY_SHELL:
      g_value_set_boolean (value, foundry_dap_initialize_request_get_supports_args_can_be_interpreted_by_shell (self));
      break;

    case PROP_SUPPORTS_START_DEBUGGING_REQUEST:
      g_value_set_boolean (value, foundry_dap_initialize_request_get_supports_start_debugging_request (self));
      break;

    case PROP_SUPPORTS_ANSISTYLING:
      g_value_set_boolean (value, foundry_dap_initialize_request_get_supports_ansistyling (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_dap_initialize_request_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  FoundryDapInitializeRequest *self = FOUNDRY_DAP_INITIALIZE_REQUEST (object);

  switch (prop_id)
    {
    case PROP_CLIENT_ID:
      foundry_dap_initialize_request_set_client_id (self, g_value_get_string (value));
      break;

    case PROP_CLIENT_NAME:
      foundry_dap_initialize_request_set_client_name (self, g_value_get_string (value));
      break;

    case PROP_ADAPTER_ID:
      foundry_dap_initialize_request_set_adapter_id (self, g_value_get_string (value));
      break;

    case PROP_LOCALE:
      foundry_dap_initialize_request_set_locale (self, g_value_get_string (value));
      break;

    case PROP_LINES_START_AT_ONE:
      foundry_dap_initialize_request_set_lines_start_at_one (self, g_value_get_boolean (value));
      break;

    case PROP_COLUMNS_START_AT_ONE:
      foundry_dap_initialize_request_set_columns_start_at_one (self, g_value_get_boolean (value));
      break;

    case PROP_PATH_FORMAT:
      foundry_dap_initialize_request_set_path_format (self, g_value_get_string (value));
      break;

    case PROP_SUPPORTS_VARIABLE_TYPE:
      foundry_dap_initialize_request_set_supports_variable_type (self, g_value_get_boolean (value));
      break;

    case PROP_SUPPORTS_VARIABLE_PAGING:
      foundry_dap_initialize_request_set_supports_variable_paging (self, g_value_get_boolean (value));
      break;

    case PROP_SUPPORTS_RUN_IN_TERMINAL_REQUEST:
      foundry_dap_initialize_request_set_supports_run_in_terminal_request (self, g_value_get_boolean (value));
      break;

    case PROP_SUPPORTS_MEMORY_REFERENCES:
      foundry_dap_initialize_request_set_supports_memory_references (self, g_value_get_boolean (value));
      break;

    case PROP_SUPPORTS_PROGRESS_REPORTING:
      foundry_dap_initialize_request_set_supports_progress_reporting (self, g_value_get_boolean (value));
      break;

    case PROP_SUPPORTS_INVALIDATED_EVENT:
      foundry_dap_initialize_request_set_supports_invalidated_event (self, g_value_get_boolean (value));
      break;

    case PROP_SUPPORTS_MEMORY_EVENT:
      foundry_dap_initialize_request_set_supports_memory_event (self, g_value_get_boolean (value));
      break;

    case PROP_SUPPORTS_ARGS_CAN_BE_INTERPRETED_BY_SHELL:
      foundry_dap_initialize_request_set_supports_args_can_be_interpreted_by_shell (self, g_value_get_boolean (value));
      break;

    case PROP_SUPPORTS_START_DEBUGGING_REQUEST:
      foundry_dap_initialize_request_set_supports_start_debugging_request (self, g_value_get_boolean (value));
      break;

    case PROP_SUPPORTS_ANSISTYLING:
      foundry_dap_initialize_request_set_supports_ansistyling (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_dap_initialize_request_class_init (FoundryDapInitializeRequestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryDapProtocolMessageClass *protocol_message_class = FOUNDRY_DAP_PROTOCOL_MESSAGE_CLASS (klass);

  object_class->finalize = foundry_dap_initialize_request_finalize;
  object_class->get_property = foundry_dap_initialize_request_get_property;
  object_class->set_property = foundry_dap_initialize_request_set_property;

  protocol_message_class->serialize = foundry_dap_initialize_request_serialize;

  properties[PROP_CLIENT_ID] =
    g_param_spec_string ("client-id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_CLIENT_NAME] =
    g_param_spec_string ("client-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ADAPTER_ID] =
    g_param_spec_string ("adapter-id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_LOCALE] =
    g_param_spec_string ("locale", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_LINES_START_AT_ONE] =
    g_param_spec_boolean ("lines-start-at-one", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_COLUMNS_START_AT_ONE] =
    g_param_spec_boolean ("columns-start-at-one", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_PATH_FORMAT] =
    g_param_spec_string ("path-format", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SUPPORTS_VARIABLE_TYPE] =
    g_param_spec_boolean ("supports-variable-type", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SUPPORTS_VARIABLE_PAGING] =
    g_param_spec_boolean ("supports-variable-paging", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SUPPORTS_RUN_IN_TERMINAL_REQUEST] =
    g_param_spec_boolean ("supports-run-in-terminal-request", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SUPPORTS_MEMORY_REFERENCES] =
    g_param_spec_boolean ("supports-memory-references", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SUPPORTS_PROGRESS_REPORTING] =
    g_param_spec_boolean ("supports-progress-reporting", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SUPPORTS_INVALIDATED_EVENT] =
    g_param_spec_boolean ("supports-invalidated-event", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SUPPORTS_MEMORY_EVENT] =
    g_param_spec_boolean ("supports-memory-event", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SUPPORTS_ARGS_CAN_BE_INTERPRETED_BY_SHELL] =
    g_param_spec_boolean ("supports-args-can-be-interpreted-by-shell", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SUPPORTS_START_DEBUGGING_REQUEST] =
    g_param_spec_boolean ("supports-start-debugging-request", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SUPPORTS_ANSISTYLING] =
    g_param_spec_boolean ("supports-ansistyling", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_dap_initialize_request_init (FoundryDapInitializeRequest *self)
{
}

const char *
foundry_dap_initialize_request_get_client_id (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), NULL);

  return self->client_id;
}

const char *
foundry_dap_initialize_request_get_client_name (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), NULL);

  return self->client_name;
}

const char *
foundry_dap_initialize_request_get_adapter_id (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), NULL);

  return self->adapter_id;
}

const char *
foundry_dap_initialize_request_get_locale (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), NULL);

  return self->locale;
}

gboolean
foundry_dap_initialize_request_get_lines_start_at_one (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), FALSE);

  return self->lines_start_at_one;
}

gboolean
foundry_dap_initialize_request_get_columns_start_at_one (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), FALSE);

  return self->columns_start_at_one;
}

const char *
foundry_dap_initialize_request_get_path_format (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), NULL);

  return self->path_format;
}

gboolean
foundry_dap_initialize_request_get_supports_variable_type (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), FALSE);

  return self->supports_variable_type;
}

gboolean
foundry_dap_initialize_request_get_supports_variable_paging (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), FALSE);

  return self->supports_variable_paging;
}

gboolean
foundry_dap_initialize_request_get_supports_run_in_terminal_request (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), FALSE);

  return self->supports_run_in_terminal_request;
}

gboolean
foundry_dap_initialize_request_get_supports_memory_references (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), FALSE);

  return self->supports_memory_references;
}

gboolean
foundry_dap_initialize_request_get_supports_progress_reporting (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), FALSE);

  return self->supports_progress_reporting;
}

gboolean
foundry_dap_initialize_request_get_supports_invalidated_event (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), FALSE);

  return self->supports_invalidated_event;
}

gboolean
foundry_dap_initialize_request_get_supports_memory_event (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), FALSE);

  return self->supports_memory_event;
}

gboolean
foundry_dap_initialize_request_get_supports_args_can_be_interpreted_by_shell (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), FALSE);

  return self->supports_args_can_be_interpreted_by_shell;
}

gboolean
foundry_dap_initialize_request_get_supports_start_debugging_request (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), FALSE);

  return self->supports_start_debugging_request;
}

gboolean
foundry_dap_initialize_request_get_supports_ansistyling (FoundryDapInitializeRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self), FALSE);

  return self->supports_ansistyling;
}

void
foundry_dap_initialize_request_set_client_id (FoundryDapInitializeRequest *self,
																							const char                  *client_id)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (g_set_str (&self->client_id, client_id))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CLIENT_ID]);
}

void
foundry_dap_initialize_request_set_client_name (FoundryDapInitializeRequest *self,
																								const char                  *client_name)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (g_set_str (&self->client_name, client_name))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CLIENT_NAME]);
}

void
foundry_dap_initialize_request_set_adapter_id (FoundryDapInitializeRequest *self,
																							 const char                  *adapter_id)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (g_set_str (&self->adapter_id, adapter_id))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ADAPTER_ID]);
}

void
foundry_dap_initialize_request_set_locale (FoundryDapInitializeRequest *self,
																					 const char                  *locale)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (g_set_str (&self->locale, locale))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOCALE]);
}

void
foundry_dap_initialize_request_set_lines_start_at_one (FoundryDapInitializeRequest *self,
																											 gboolean                     lines_start_at_one)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (self->lines_start_at_one != lines_start_at_one)
    {
      self->lines_start_at_one = !!lines_start_at_one;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LINES_START_AT_ONE]);
    }
}

void
foundry_dap_initialize_request_set_columns_start_at_one (FoundryDapInitializeRequest *self,
																												 gboolean                     columns_start_at_one)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (self->columns_start_at_one != columns_start_at_one)
    {
      self->columns_start_at_one = !!columns_start_at_one;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLUMNS_START_AT_ONE]);
    }
}

void
foundry_dap_initialize_request_set_path_format (FoundryDapInitializeRequest *self,
																								const char                  *path_format)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (g_set_str (&self->path_format, path_format))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH_FORMAT]);
}

void
foundry_dap_initialize_request_set_supports_variable_type (FoundryDapInitializeRequest *self,
																													 gboolean                     supports_variable_type)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (self->supports_variable_type != supports_variable_type)
    {
      self->supports_variable_type = !!supports_variable_type;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUPPORTS_VARIABLE_TYPE]);
    }
}

void
foundry_dap_initialize_request_set_supports_variable_paging (FoundryDapInitializeRequest *self,
																														 gboolean                     supports_variable_paging)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (self->supports_variable_paging != supports_variable_paging)
    {
      self->supports_variable_paging = !!supports_variable_paging;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUPPORTS_VARIABLE_PAGING]);
    }
}

void
foundry_dap_initialize_request_set_supports_run_in_terminal_request (FoundryDapInitializeRequest *self,
																																		 gboolean                     supports_run_in_terminal_request)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (self->supports_run_in_terminal_request != supports_run_in_terminal_request)
    {
      self->supports_run_in_terminal_request = !!supports_run_in_terminal_request;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUPPORTS_RUN_IN_TERMINAL_REQUEST]);
    }
}

void
foundry_dap_initialize_request_set_supports_memory_references (FoundryDapInitializeRequest *self,
																															 gboolean                     supports_memory_references)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (self->supports_memory_references != supports_memory_references)
    {
      self->supports_memory_references = !!supports_memory_references;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUPPORTS_MEMORY_REFERENCES]);
    }
}

void
foundry_dap_initialize_request_set_supports_progress_reporting (FoundryDapInitializeRequest *self,
																																gboolean                     supports_progress_reporting)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (self->supports_progress_reporting != supports_progress_reporting)
    {
      self->supports_progress_reporting = !!supports_progress_reporting;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUPPORTS_PROGRESS_REPORTING]);
    }
}

void
foundry_dap_initialize_request_set_supports_invalidated_event (FoundryDapInitializeRequest *self,
																															 gboolean                     supports_invalidated_event)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (self->supports_invalidated_event != supports_invalidated_event)
    {
      self->supports_invalidated_event = !!supports_invalidated_event;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUPPORTS_INVALIDATED_EVENT]);
    }
}

void
foundry_dap_initialize_request_set_supports_memory_event (FoundryDapInitializeRequest *self,
																													gboolean                     supports_memory_event)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (self->supports_memory_event != supports_memory_event)
    {
      self->supports_memory_event = !!supports_memory_event;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUPPORTS_MEMORY_EVENT]);
    }
}

void
foundry_dap_initialize_request_set_supports_args_can_be_interpreted_by_shell (FoundryDapInitializeRequest *self,
																																							gboolean                     supports_args_can_be_interpreted_by_shell)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (self->supports_args_can_be_interpreted_by_shell != supports_args_can_be_interpreted_by_shell)
    {
      self->supports_args_can_be_interpreted_by_shell = !!supports_args_can_be_interpreted_by_shell;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUPPORTS_ARGS_CAN_BE_INTERPRETED_BY_SHELL]);
    }
}

void
foundry_dap_initialize_request_set_supports_start_debugging_request (FoundryDapInitializeRequest *self,
																																		 gboolean                     supports_start_debugging_request)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (self->supports_start_debugging_request != supports_start_debugging_request)
    {
      self->supports_start_debugging_request = !!supports_start_debugging_request;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUPPORTS_START_DEBUGGING_REQUEST]);
    }
}

void
foundry_dap_initialize_request_set_supports_ansistyling (FoundryDapInitializeRequest *self,
                                                         gboolean                     supports_ansistyling)
{
  g_return_if_fail (FOUNDRY_IS_DAP_INITIALIZE_REQUEST (self));

  if (self->supports_ansistyling != supports_ansistyling)
    {
      self->supports_ansistyling = !!supports_ansistyling;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUPPORTS_ANSISTYLING]);
    }
}

FoundryDapRequest *
foundry_dap_initialize_request_new (const char *adapter_id)
{
  return g_object_new (FOUNDRY_TYPE_DAP_INITIALIZE_REQUEST,
                       "adapter-id", adapter_id,
                       NULL);
}
