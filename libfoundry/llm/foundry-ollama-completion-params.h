/* foundry-ollama-completion-params.h
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

#include "foundry-llm-completion-params.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_OLLAMA_COMPLETION_PARAMS (foundry_ollama_completion_params_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (FoundryOllamaCompletionParams, foundry_ollama_completion_params, FOUNDRY, OLLAMA_COMPLETION_PARAMS, FoundryLlmCompletionParams)

FOUNDRY_AVAILABLE_IN_ALL
FoundryOllamaCompletionParams *foundry_ollama_completion_params_new         (void);
FOUNDRY_AVAILABLE_IN_ALL
char                          *foundry_ollama_completion_params_dup_suffix  (FoundryOllamaCompletionParams *self);
FOUNDRY_AVAILABLE_IN_ALL
void                           foundry_ollama_completion_params_set_suffix  (FoundryOllamaCompletionParams *self,
                                                                             const char                    *suffix);
FOUNDRY_AVAILABLE_IN_ALL
char                          *foundry_ollama_completion_params_dup_system  (FoundryOllamaCompletionParams *self);
FOUNDRY_AVAILABLE_IN_ALL
void                           foundry_ollama_completion_params_set_system  (FoundryOllamaCompletionParams *self,
                                                                             const char                    *system);
FOUNDRY_AVAILABLE_IN_ALL
char                          *foundry_ollama_completion_params_dup_context (FoundryOllamaCompletionParams *self);
FOUNDRY_AVAILABLE_IN_ALL
void                           foundry_ollama_completion_params_set_context (FoundryOllamaCompletionParams *self,
                                                                             const char                    *context);
FOUNDRY_AVAILABLE_IN_ALL
gboolean                       foundry_ollama_completion_params_get_raw     (FoundryOllamaCompletionParams *self);
FOUNDRY_AVAILABLE_IN_ALL
void                           foundry_ollama_completion_params_set_raw     (FoundryOllamaCompletionParams *self,
                                                                             gboolean                       raw);

G_END_DECLS
