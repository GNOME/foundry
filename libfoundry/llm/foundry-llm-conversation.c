/* foundry-llm-conversation.c
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

#include "foundry-llm-conversation.h"
#include "foundry-llm-tool.h"
#include "foundry-util.h"

typedef struct
{
  GListModel *tools;
} FoundryLlmConversationPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryLlmConversation, foundry_llm_conversation, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_TOOLS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_llm_conversation_dispose (GObject *object)
{
  FoundryLlmConversation *self = (FoundryLlmConversation *)object;
  FoundryLlmConversationPrivate *priv = foundry_llm_conversation_get_instance_private (self);

  g_clear_object (&priv->tools);

  G_OBJECT_CLASS (foundry_llm_conversation_parent_class)->dispose (object);
}

static void
foundry_llm_conversation_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  FoundryLlmConversation *self = FOUNDRY_LLM_CONVERSATION (object);

  switch (prop_id)
    {
    case PROP_TOOLS:
      g_value_set_object (value, foundry_llm_conversation_dup_tools (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_llm_conversation_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  FoundryLlmConversation *self = FOUNDRY_LLM_CONVERSATION (object);

  switch (prop_id)
    {
    case PROP_TOOLS:
      foundry_llm_conversation_set_tools (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_llm_conversation_class_init (FoundryLlmConversationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_llm_conversation_dispose;
  object_class->get_property = foundry_llm_conversation_get_property;
  object_class->set_property = foundry_llm_conversation_set_property;

  properties[PROP_TOOLS] =
    g_param_spec_object ("tools", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_llm_conversation_init (FoundryLlmConversation *self)
{
}

/**
 * foundry_llm_conversation_add_context:
 * @self: a [class@Foundry.LlmConversation]
 *
 * Adds context to the conversation.
 *
 * Generally this applies to the conversation right after the system prompt.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any
 *   value or rejects with error.
 */
DexFuture *
foundry_llm_conversation_add_context (FoundryLlmConversation *self,
                                      const char             *context)
{
  dex_return_error_if_fail (FOUNDRY_IS_LLM_CONVERSATION (self));

  if (FOUNDRY_LLM_CONVERSATION_GET_CLASS (self)->add_context)
    return FOUNDRY_LLM_CONVERSATION_GET_CLASS (self)->add_context (self, context);

  return foundry_future_new_not_supported ();
}

/**
 * foundry_llm_conversation_send_message:
 * @self: a [class@Foundry.LlmConversation]
 * @role: the role of the message sender
 * @message: the message to be sent
 *
 * The role should generally be something like "system", "user", "assistant",
 * or "tool".
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any
 *   value or rejects with error.
 */
DexFuture *
foundry_llm_conversation_send_message (FoundryLlmConversation *self,
                                       const char             *role,
                                       const char             *message)
{
  const char *roles[] = {role, NULL};
  const char *messages[] = {message, NULL};

  dex_return_error_if_fail (FOUNDRY_IS_LLM_CONVERSATION (self));
  dex_return_error_if_fail (role != NULL);
  dex_return_error_if_fail (message != NULL);

  if (FOUNDRY_LLM_CONVERSATION_GET_CLASS (self)->send_messages)
    return FOUNDRY_LLM_CONVERSATION_GET_CLASS (self)->send_messages (self, roles, messages);

  return foundry_future_new_not_supported ();
}

/**
 * foundry_llm_conversation_send_messages:
 * @self: a [class@Foundry.LlmConversation]
 *
 * Send multiple messages together.
 *
 * The length of @roles must be the same as @messages.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any value
 *   or rejects with error.
 */
DexFuture *
foundry_llm_conversation_send_messages (FoundryLlmConversation *self,
                                        const char * const     *roles,
                                        const char * const     *messages)
{
  dex_return_error_if_fail (FOUNDRY_IS_LLM_CONVERSATION (self));
  dex_return_error_if_fail (roles != NULL && roles[0] != NULL);
  dex_return_error_if_fail (messages != NULL && messages[0] != NULL);
  dex_return_error_if_fail (g_strv_length ((char **)roles) == g_strv_length ((char **)messages));

  if (FOUNDRY_LLM_CONVERSATION_GET_CLASS (self)->send_messages)
    return FOUNDRY_LLM_CONVERSATION_GET_CLASS (self)->send_messages (self, roles, messages);

  return foundry_future_new_not_supported ();
}

/**
 * foundry_llm_conversation_reset:
 * @self: a [class@Foundry.LlmConversation]
 *
 * Reset the conversation to the initial state.
 */
void
foundry_llm_conversation_reset (FoundryLlmConversation *self)
{
  g_return_if_fail (FOUNDRY_IS_LLM_CONVERSATION (self));

  if (FOUNDRY_LLM_CONVERSATION_GET_CLASS (self)->reset)
    FOUNDRY_LLM_CONVERSATION_GET_CLASS (self)->reset (self);
}

/**
 * foundry_llm_conversation_dup_tools:
 * @self: a [class@Foundry.LlmConversation]
 *
 * Lists tools made available to the conversation.
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of [class@Foundry.LllmTool]
 */
GListModel *
foundry_llm_conversation_dup_tools (FoundryLlmConversation *self)
{
  FoundryLlmConversationPrivate *priv = foundry_llm_conversation_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_LLM_CONVERSATION (self), NULL);

  return priv->tools ? g_object_ref (priv->tools) : NULL;
}

/**
 * foundry_llm_conversation_set_tools:
 * @self: a [class@Foundry.LlmConversation]
 * @tools: a list model of [class@Foundry.LlmTool]
 *
 * Set the tools that are allowed to be used by the model.
 */
void
foundry_llm_conversation_set_tools (FoundryLlmConversation *self,
                                    GListModel             *tools)
{
  FoundryLlmConversationPrivate *priv = foundry_llm_conversation_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_LLM_CONVERSATION (self));
  g_return_if_fail (!tools || G_IS_LIST_MODEL (tools));

  if (g_set_object (&priv->tools, tools))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TOOLS]);
}

/**
 * foundry_llm_conversation_list_history:
 * @self: a [class@Foundry.LlmConversation]
 *
 * List the available history of the conversation.
 *
 * Returns: (transfer full) (nullable): a [iface@Gio.ListModel] of
 *   [class@Foundry.LlmMessage].
 */
GListModel *
foundry_llm_conversation_list_history (FoundryLlmConversation *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LLM_CONVERSATION (self), NULL);

  if (FOUNDRY_LLM_CONVERSATION_GET_CLASS (self)->list_history)
    return FOUNDRY_LLM_CONVERSATION_GET_CLASS (self)->list_history (self);

  return NULL;
}
