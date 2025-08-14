/* foundry-llm-conversation.h
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

#include <libdex.h>

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_LLM_CONVERSATION (foundry_llm_conversation_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (FoundryLlmConversation, foundry_llm_conversation, FOUNDRY, LLM_CONVERSATION, GObject)

struct _FoundryLlmConversationClass
{
  GObjectClass parent_class;

  DexFuture *(*add_context)   (FoundryLlmConversation *self,
                               const char             *context);
  DexFuture *(*send_messages) (FoundryLlmConversation *self,
                               const char * const     *roles,
                               const char * const     *messages);
  void       (*reset)         (FoundryLlmConversation *self);

  /*< private >*/
  gpointer _reserved[8];
};

FOUNDRY_AVAILABLE_IN_ALL
DexFuture  *foundry_llm_conversation_add_context   (FoundryLlmConversation *self,
                                                    const char             *context);
FOUNDRY_AVAILABLE_IN_ALL
void        foundry_llm_conversation_add_tool      (FoundryLlmConversation *self,
                                                    FoundryLlmTool         *tool);
FOUNDRY_AVAILABLE_IN_ALL
void        foundry_llm_conversation_remove_tool   (FoundryLlmConversation *self,
                                                    FoundryLlmTool         *tool);
FOUNDRY_AVAILABLE_IN_ALL
GListModel *foundry_llm_conversation_list_tools    (FoundryLlmConversation *self);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture  *foundry_llm_conversation_send_message  (FoundryLlmConversation *self,
                                                    const char             *role,
                                                    const char             *message);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture  *foundry_llm_conversation_send_messages (FoundryLlmConversation *self,
                                                    const char * const     *roles,
                                                    const char * const     *messages);
FOUNDRY_AVAILABLE_IN_ALL
void        foundry_llm_conversation_reset         (FoundryLlmConversation *self);

G_END_DECLS
