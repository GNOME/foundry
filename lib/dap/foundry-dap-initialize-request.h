/*
 * foundry-dap-initialize-request.h
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "foundry-dap-request.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_DAP_INITIALIZE_REQUEST (foundry_dap_initialize_request_get_type())

FOUNDRY_AVAILABLE_IN_ALL
FOUNDRY_DECLARE_INTERNAL_TYPE (FoundryDapInitializeRequest, foundry_dap_initialize_request, FOUNDRY, DAP_INITIALIZE_REQUEST, FoundryDapRequest)

FOUNDRY_AVAILABLE_IN_ALL
FoundryDapRequest *foundry_dap_initialize_request_new                                           (const char                  *adapter_id);
FOUNDRY_AVAILABLE_IN_ALL
const char        *foundry_dap_initialize_request_get_client_id                                 (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_client_id                                 (FoundryDapInitializeRequest *self,
                                                                                                 const char                  *client_id);
FOUNDRY_AVAILABLE_IN_ALL
const char        *foundry_dap_initialize_request_get_client_name                               (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_client_name                               (FoundryDapInitializeRequest *self,
                                                                                                 const char                  *client_name);
FOUNDRY_AVAILABLE_IN_ALL
const char        *foundry_dap_initialize_request_get_adapter_id                                (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_adapter_id                                (FoundryDapInitializeRequest *self,
                                                                                                 const char                  *adapter_id);
FOUNDRY_AVAILABLE_IN_ALL
const char        *foundry_dap_initialize_request_get_locale                                    (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_locale                                    (FoundryDapInitializeRequest *self,
                                                                                                 const char                  *locale);
FOUNDRY_AVAILABLE_IN_ALL
gboolean           foundry_dap_initialize_request_get_lines_start_at_one                        (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_lines_start_at_one                        (FoundryDapInitializeRequest *self,
                                                                                                 gboolean                     lines_start_at_one);
FOUNDRY_AVAILABLE_IN_ALL
gboolean           foundry_dap_initialize_request_get_columns_start_at_one                      (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_columns_start_at_one                      (FoundryDapInitializeRequest *self,
                                                                                                 gboolean                     columns_start_at_one);
FOUNDRY_AVAILABLE_IN_ALL
const char        *foundry_dap_initialize_request_get_path_format                               (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_path_format                               (FoundryDapInitializeRequest *self,
                                                                                                 const char                  *path_format);
FOUNDRY_AVAILABLE_IN_ALL
gboolean           foundry_dap_initialize_request_get_supports_variable_type                    (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_supports_variable_type                    (FoundryDapInitializeRequest *self,
                                                                                                 gboolean                     supports_variable_type);
FOUNDRY_AVAILABLE_IN_ALL
gboolean           foundry_dap_initialize_request_get_supports_variable_paging                  (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_supports_variable_paging                  (FoundryDapInitializeRequest *self,
                                                                                                 gboolean                     supports_variable_paging);
FOUNDRY_AVAILABLE_IN_ALL
gboolean           foundry_dap_initialize_request_get_supports_run_in_terminal_request          (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_supports_run_in_terminal_request          (FoundryDapInitializeRequest *self,
                                                                                                 gboolean                     supports_run_in_terminal_request);
FOUNDRY_AVAILABLE_IN_ALL
gboolean           foundry_dap_initialize_request_get_supports_memory_references                (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_supports_memory_references                (FoundryDapInitializeRequest *self,
                                                                                                 gboolean                     supports_memory_references);
FOUNDRY_AVAILABLE_IN_ALL
gboolean           foundry_dap_initialize_request_get_supports_progress_reporting               (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_supports_progress_reporting               (FoundryDapInitializeRequest *self,
                                                                                                 gboolean                     supports_progress_reporting);
FOUNDRY_AVAILABLE_IN_ALL
gboolean           foundry_dap_initialize_request_get_supports_invalidated_event                (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_supports_invalidated_event                (FoundryDapInitializeRequest *self,
                                                                                                 gboolean                     supports_invalidated_event);
FOUNDRY_AVAILABLE_IN_ALL
gboolean           foundry_dap_initialize_request_get_supports_memory_event                     (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_supports_memory_event                     (FoundryDapInitializeRequest *self,
                                                                                                 gboolean                     supports_memory_event);
FOUNDRY_AVAILABLE_IN_ALL
gboolean           foundry_dap_initialize_request_get_supports_args_can_be_interpreted_by_shell (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_supports_args_can_be_interpreted_by_shell (FoundryDapInitializeRequest *self,
                                                                                                 gboolean                     supports_args_can_be_interpreted_by_shell);
FOUNDRY_AVAILABLE_IN_ALL
gboolean           foundry_dap_initialize_request_get_supports_start_debugging_request          (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_supports_start_debugging_request          (FoundryDapInitializeRequest *self,
                                                                                                 gboolean                     supports_start_debugging_request);
FOUNDRY_AVAILABLE_IN_ALL
gboolean           foundry_dap_initialize_request_get_supports_ansistyling                      (FoundryDapInitializeRequest *self);
FOUNDRY_AVAILABLE_IN_ALL
void               foundry_dap_initialize_request_set_supports_ansistyling                      (FoundryDapInitializeRequest *self,
                                                                                                 gboolean                     supports_ansistyling);

G_END_DECLS
