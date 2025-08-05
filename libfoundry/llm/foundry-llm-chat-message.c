/* foundry-llm-chat-message.c
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

#include "foundry-llm-chat-message.h"

typedef struct
{
  char *content;
  char *role;
} FoundryLlmChatMessagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (FoundryLlmChatMessage, foundry_llm_chat_message, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CONTENT,
  PROP_ROLE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_llm_chat_message_finalize (GObject *object)
{
  FoundryLlmChatMessage *self = (FoundryLlmChatMessage *)object;
  FoundryLlmChatMessagePrivate *priv = foundry_llm_chat_message_get_instance_private (self);

  g_clear_pointer (&priv->content, g_free);
  g_clear_pointer (&priv->role, g_free);

  G_OBJECT_CLASS (foundry_llm_chat_message_parent_class)->finalize (object);
}

static void
foundry_llm_chat_message_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  FoundryLlmChatMessage *self = FOUNDRY_LLM_CHAT_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_CONTENT:
      g_value_take_string (value, foundry_llm_chat_message_dup_content (self));
      break;

    case PROP_ROLE:
      g_value_take_string (value, foundry_llm_chat_message_dup_role (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_llm_chat_message_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  FoundryLlmChatMessage *self = FOUNDRY_LLM_CHAT_MESSAGE (object);
  FoundryLlmChatMessagePrivate *priv = foundry_llm_chat_message_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CONTENT:
      priv->content = g_value_dup_string (value);
      break;

    case PROP_ROLE:
      priv->role = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_llm_chat_message_class_init (FoundryLlmChatMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_llm_chat_message_finalize;
  object_class->get_property = foundry_llm_chat_message_get_property;
  object_class->set_property = foundry_llm_chat_message_set_property;

  properties[PROP_CONTENT] =
    g_param_spec_string ("content", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ROLE] =
    g_param_spec_string ("role", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_llm_chat_message_init (FoundryLlmChatMessage *self)
{
}

FoundryLlmChatMessage *
foundry_llm_chat_message_new (const char *role,
                              const char *content)
{
  return g_object_new (FOUNDRY_TYPE_LLM_CHAT_MESSAGE,
                       "role", role,
                       "content", content,
                       NULL);
}

char *
foundry_llm_chat_message_dup_content (FoundryLlmChatMessage *self)
{
  FoundryLlmChatMessagePrivate *priv = foundry_llm_chat_message_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_LLM_CHAT_MESSAGE (self), NULL);

  return g_strdup (priv->content);
}

char *
foundry_llm_chat_message_dup_role (FoundryLlmChatMessage *self)
{
  FoundryLlmChatMessagePrivate *priv = foundry_llm_chat_message_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_LLM_CHAT_MESSAGE (self), NULL);

  return g_strdup (priv->role);
}
