/* foundry-llm-completion-params.h
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

#pragma once

#include <glib-object.h>

#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_LLM_COMPLETION_PARAMS (foundry_llm_completion_params_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (FoundryLlmCompletionParams, foundry_llm_completion_params, FOUNDRY, LLM_COMPLETION_PARAMS, GObject)

FOUNDRY_AVAILABLE_IN_ALL
FoundryLlmCompletionParams *foundry_llm_completion_params_new        (void);
FOUNDRY_AVAILABLE_IN_ALL
char                       *foundry_llm_completion_params_dup_prompt (FoundryLlmCompletionParams *self);
FOUNDRY_AVAILABLE_IN_ALL
void                        foundry_llm_completion_params_set_prompt (FoundryLlmCompletionParams *self,
                                                                      const char                 *prompt);
FOUNDRY_AVAILABLE_IN_ALL
char                       *foundry_llm_completion_params_dup_suffix (FoundryLlmCompletionParams *self);
FOUNDRY_AVAILABLE_IN_ALL
void                        foundry_llm_completion_params_set_suffix (FoundryLlmCompletionParams *self,
                                                                      const char                 *suffix);
FOUNDRY_AVAILABLE_IN_ALL
char                       *foundry_llm_completion_params_dup_system (FoundryLlmCompletionParams *self);
FOUNDRY_AVAILABLE_IN_ALL
void                        foundry_llm_completion_params_set_system (FoundryLlmCompletionParams *self,
                                                                      const char                 *system);
FOUNDRY_AVAILABLE_IN_ALL
char                       *foundry_llm_completion_params_dup_context (FoundryLlmCompletionParams *self);
FOUNDRY_AVAILABLE_IN_ALL
void                        foundry_llm_completion_params_set_context (FoundryLlmCompletionParams *self,
                                                                       const char                 *context);
FOUNDRY_AVAILABLE_IN_ALL
gboolean                    foundry_llm_completion_params_get_raw     (FoundryLlmCompletionParams *self);
FOUNDRY_AVAILABLE_IN_ALL
void                        foundry_llm_completion_params_set_raw     (FoundryLlmCompletionParams *self,
                                                                       gboolean                    raw);

G_END_DECLS
