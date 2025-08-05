/* foundry-llm-chat.c
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

#include "foundry-llm-chat.h"
#include "foundry-util.h"

G_DEFINE_ABSTRACT_TYPE (FoundryLlmChat, foundry_llm_chat, FOUNDRY_TYPE_CONTEXTUAL)

static void
foundry_llm_chat_class_init (FoundryLlmChatClass *klass)
{
}

static void
foundry_llm_chat_init (FoundryLlmChat *self)
{
}

/**
 * foundry_llm_chat_next_reply:
 * @self: a [class@Foundry.LlmChat]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.LlmChatMessage] or rejects with error.
 */
DexFuture *
foundry_llm_chat_next_reply (FoundryLlmChat *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_LLM_CHAT (self));

  if (FOUNDRY_LLM_CHAT_GET_CLASS (self)->next_reply)
    return FOUNDRY_LLM_CHAT_GET_CLASS (self)->next_reply (self);

  return foundry_future_new_not_supported ();
}

/**
 * foundry_llm_chat_when_finished:
 * @self: a [class@Foundry.LlmChat]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when the
 *   chat has completed or rejects with error.
 */
DexFuture *
foundry_llm_chat_when_finished (FoundryLlmChat *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_LLM_CHAT (self));

  if (FOUNDRY_LLM_CHAT_GET_CLASS (self)->when_finished)
    return FOUNDRY_LLM_CHAT_GET_CLASS (self)->when_finished (self);

  return foundry_future_new_not_supported ();
}
