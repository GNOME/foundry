/* foundry-acp-permission-response.c
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

#include "foundry-acp-permission-response-private.h"
#include "foundry-json-node.h"

struct _FoundryAcpPermissionResponse
{
  GObject parent_instance;

  char *option_id;
  guint cancelled : 1;
};

G_DEFINE_FINAL_TYPE (FoundryAcpPermissionResponse, foundry_acp_permission_response, G_TYPE_OBJECT)

static void
foundry_acp_permission_response_finalize (GObject *object)
{
  FoundryAcpPermissionResponse *self = (FoundryAcpPermissionResponse *)object;

  g_clear_pointer (&self->option_id, g_free);

  G_OBJECT_CLASS (foundry_acp_permission_response_parent_class)->finalize (object);
}

static void
foundry_acp_permission_response_class_init (FoundryAcpPermissionResponseClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_acp_permission_response_finalize;
}

static void
foundry_acp_permission_response_init (FoundryAcpPermissionResponse *self)
{
}

/**
 * foundry_acp_permission_response_new_selected:
 * @option_id: the selected ACP option identifier
 *
 * Creates a permission response selecting @option_id.
 *
 * Returns: (transfer full): a [class@Foundry.AcpPermissionResponse]
 *
 * Since: 1.2
 */
FoundryAcpPermissionResponse *
foundry_acp_permission_response_new_selected (const char *option_id)
{
  FoundryAcpPermissionResponse *self;

  g_return_val_if_fail (option_id != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_ACP_PERMISSION_RESPONSE, NULL);
  self->option_id = g_strdup (option_id);

  return self;
}

/**
 * foundry_acp_permission_response_new_cancelled:
 *
 * Creates a cancelled permission response.
 *
 * Returns: (transfer full): a [class@Foundry.AcpPermissionResponse]
 *
 * Since: 1.2
 */
FoundryAcpPermissionResponse *
foundry_acp_permission_response_new_cancelled (void)
{
  FoundryAcpPermissionResponse *self;

  self = g_object_new (FOUNDRY_TYPE_ACP_PERMISSION_RESPONSE, NULL);
  self->cancelled = TRUE;

  return self;
}

/**
 * foundry_acp_permission_response_get_cancelled:
 * @self: a [class@Foundry.AcpPermissionResponse]
 *
 * Returns: %TRUE if the permission was cancelled
 *
 * Since: 1.2
 */
gboolean
foundry_acp_permission_response_get_cancelled (FoundryAcpPermissionResponse *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_RESPONSE (self), FALSE);

  return self->cancelled;
}

/**
 * foundry_acp_permission_response_dup_option_id:
 * @self: a [class@Foundry.AcpPermissionResponse]
 *
 * Returns: (transfer full) (nullable): the selected option identifier
 *
 * Since: 1.2
 */
char *
foundry_acp_permission_response_dup_option_id (FoundryAcpPermissionResponse *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_RESPONSE (self), NULL);

  return g_strdup (self->option_id);
}

JsonNode *
_foundry_acp_permission_response_to_json (FoundryAcpPermissionResponse *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_RESPONSE (self), NULL);

  if (self->cancelled)
    return FOUNDRY_JSON_OBJECT_NEW ("outcome", "cancelled");

  return FOUNDRY_JSON_OBJECT_NEW ("outcome", "selected",
                                  "optionId", FOUNDRY_JSON_NODE_PUT_STRING (self->option_id));
}
