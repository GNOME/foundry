/* foundry-auth-prompt.h
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

#include "foundry-contextual.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_AUTH_PROMPT (foundry_auth_prompt_get_type())
#define FOUNDRY_TYPE_AUTH_PROMPT_BUILDER (foundry_auth_prompt_builder_get_type())

typedef struct _FoundryAuthPromptBuilder FoundryAuthPromptBuilder;

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (FoundryAuthPrompt, foundry_auth_prompt, FOUNDRY, AUTH_PROMPT, GObject)

FOUNDRY_AVAILABLE_IN_ALL
const char               *foundry_auth_prompt_get_value            (FoundryAuthPrompt         *self,
                                                                    const char                *id);
FOUNDRY_AVAILABLE_IN_ALL
char                    **foundry_auth_prompt_dup_prompts          (FoundryAuthPrompt         *self);
FOUNDRY_AVAILABLE_IN_ALL
char                     *foundry_auth_prompt_dup_title            (FoundryAuthPrompt         *self);
FOUNDRY_AVAILABLE_IN_ALL
char                     *foundry_auth_prompt_dup_subtitle         (FoundryAuthPrompt         *self);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture                *foundry_auth_prompt_query                (FoundryAuthPrompt         *self);
FOUNDRY_AVAILABLE_IN_ALL
char                     *foundry_auth_prompt_dup_prompt_name      (FoundryAuthPrompt         *self,
                                                                    const char                *id);
FOUNDRY_AVAILABLE_IN_ALL
char                     *foundry_auth_prompt_dup_prompt_value     (FoundryAuthPrompt         *self,
                                                                    const char                *id);
FOUNDRY_AVAILABLE_IN_ALL
void                      foundry_auth_prompt_set_prompt_value     (FoundryAuthPrompt         *self,
                                                                    const char                *id,
                                                                    const char                *value);
FOUNDRY_AVAILABLE_IN_ALL
gboolean                  foundry_auth_prompt_is_prompt_hidden     (FoundryAuthPrompt         *self,
                                                                    const char                *id);
FOUNDRY_AVAILABLE_IN_ALL
GType                     foundry_auth_prompt_builder_get_type     (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_ALL
FoundryAuthPromptBuilder *foundry_auth_prompt_builder_new          (FoundryAuthProvider       *provider);
FOUNDRY_AVAILABLE_IN_ALL
void                      foundry_auth_prompt_builder_set_title    (FoundryAuthPromptBuilder  *builder,
                                                                    const char                *title);
FOUNDRY_AVAILABLE_IN_ALL
void                      foundry_auth_prompt_builder_set_subtitle (FoundryAuthPromptBuilder  *builder,
                                                                    const char                *subtitle);
FOUNDRY_AVAILABLE_IN_ALL
void                      foundry_auth_prompt_builder_add_param    (FoundryAuthPromptBuilder  *builder,
                                                                    const char                *id,
                                                                    const char                *name,
                                                                    const char                *value,
                                                                    gboolean                   hidden);
FOUNDRY_AVAILABLE_IN_ALL
FoundryAuthPrompt        *foundry_auth_prompt_builder_end          (FoundryAuthPromptBuilder  *builder);
FOUNDRY_AVAILABLE_IN_ALL
void                      foundry_auth_prompt_builder_free         (FoundryAuthPromptBuilder  *builder);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FoundryAuthPromptBuilder, foundry_auth_prompt_builder_free)

G_END_DECLS
