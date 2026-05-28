/* foundry-acp-prompt-result.c
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

#include "foundry-acp-prompt-result-private.h"

struct _FoundryAcpPromptResult
{
  GObject parent_instance;

  char *raw_stop_reason;
  FoundryAcpStopReason stop_reason;
};

G_DEFINE_FINAL_TYPE (FoundryAcpPromptResult, foundry_acp_prompt_result, G_TYPE_OBJECT)

static void
foundry_acp_prompt_result_finalize (GObject *object)
{
  FoundryAcpPromptResult *self = (FoundryAcpPromptResult *)object;

  g_clear_pointer (&self->raw_stop_reason, g_free);

  G_OBJECT_CLASS (foundry_acp_prompt_result_parent_class)->finalize (object);
}

static void
foundry_acp_prompt_result_class_init (FoundryAcpPromptResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_acp_prompt_result_finalize;
}

static void
foundry_acp_prompt_result_init (FoundryAcpPromptResult *self)
{
}

static FoundryAcpStopReason
stop_reason_from_string (const char *stop_reason)
{
  if (g_strcmp0 (stop_reason, "end_turn") == 0)
    return FOUNDRY_ACP_STOP_END_TURN;

  if (g_strcmp0 (stop_reason, "max_tokens") == 0)
    return FOUNDRY_ACP_STOP_MAX_TOKENS;

  if (g_strcmp0 (stop_reason, "max_turn_requests") == 0)
    return FOUNDRY_ACP_STOP_MAX_TURN_REQUESTS;

  if (g_strcmp0 (stop_reason, "refusal") == 0)
    return FOUNDRY_ACP_STOP_REFUSAL;

  if (g_strcmp0 (stop_reason, "cancelled") == 0)
    return FOUNDRY_ACP_STOP_CANCELLED;

  return FOUNDRY_ACP_STOP_UNKNOWN;
}

FoundryAcpPromptResult *
_foundry_acp_prompt_result_new_from_json (JsonNode *node)
{
  FoundryAcpPromptResult *self;
  const char *stop_reason = NULL;

  g_return_val_if_fail (node != NULL, NULL);

  if (JSON_NODE_HOLDS_OBJECT (node))
    {
      JsonObject *object = json_node_get_object (node);

      if (json_object_has_member (object, "stopReason"))
        stop_reason = json_object_get_string_member (object, "stopReason");
    }

  self = g_object_new (FOUNDRY_TYPE_ACP_PROMPT_RESULT, NULL);
  self->raw_stop_reason = g_strdup (stop_reason);
  self->stop_reason = stop_reason_from_string (stop_reason);

  return self;
}

/**
 * foundry_acp_prompt_result_get_stop_reason:
 * @self: a [class@Foundry.AcpPromptResult]
 *
 * Returns: the normalized stop reason
 *
 * Since: 1.2
 */
FoundryAcpStopReason
foundry_acp_prompt_result_get_stop_reason (FoundryAcpPromptResult *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PROMPT_RESULT (self), FOUNDRY_ACP_STOP_UNKNOWN);

  return self->stop_reason;
}

/**
 * foundry_acp_prompt_result_dup_raw_stop_reason:
 * @self: a [class@Foundry.AcpPromptResult]
 *
 * Returns: (transfer full) (nullable): the protocol stop reason
 *
 * Since: 1.2
 */
char *
foundry_acp_prompt_result_dup_raw_stop_reason (FoundryAcpPromptResult *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PROMPT_RESULT (self), NULL);

  return g_strdup (self->raw_stop_reason);
}
