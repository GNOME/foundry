/* foundry-acp-permission-option.c
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

#include "foundry-acp-permission-option-private.h"

struct _FoundryAcpPermissionOption
{
  GObject parent_instance;
  char *id;
  char *label;
  char *description;
  JsonNode *raw;
  guint destructive : 1;
};

G_DEFINE_FINAL_TYPE (FoundryAcpPermissionOption, foundry_acp_permission_option, G_TYPE_OBJECT)

static void
foundry_acp_permission_option_finalize (GObject *object)
{
  FoundryAcpPermissionOption *self = (FoundryAcpPermissionOption *)object;

  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->label, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->raw, json_node_unref);

  G_OBJECT_CLASS (foundry_acp_permission_option_parent_class)->finalize (object);
}

static void
foundry_acp_permission_option_class_init (FoundryAcpPermissionOptionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_acp_permission_option_finalize;
}

static void
foundry_acp_permission_option_init (FoundryAcpPermissionOption *self)
{
}

static const char *
get_optional_string_member (JsonObject *object,
                            const char *member)
{
  JsonNode *node;

  g_assert (object != NULL);
  g_assert (member != NULL);

  if (!(node = json_object_get_member (object, member)))
    return NULL;

  if (!JSON_NODE_HOLDS_VALUE (node) || json_node_get_value_type (node) != G_TYPE_STRING)
    return NULL;

  return json_node_get_string (node);
}

static gboolean
get_optional_boolean_member (JsonObject *object,
                             const char *member)
{
  JsonNode *node;

  g_assert (object != NULL);
  g_assert (member != NULL);

  if (!(node = json_object_get_member (object, member)))
    return FALSE;

  if (!JSON_NODE_HOLDS_VALUE (node) || json_node_get_value_type (node) != G_TYPE_BOOLEAN)
    return FALSE;

  return json_node_get_boolean (node);
}

/**
 * foundry_acp_permission_option_new:
 * @id: the option identifier to send in the response
 * @label: (nullable): a human-facing option label
 * @description: (nullable): additional option detail
 * @destructive: whether the option is marked destructive
 *
 * Creates a permission option.
 *
 * Returns: (transfer full): a [class@Foundry.AcpPermissionOption]
 *
 * Since: 1.2
 */
FoundryAcpPermissionOption *
foundry_acp_permission_option_new (const char *id,
                                   const char *label,
                                   const char *description,
                                   gboolean    destructive)
{
  FoundryAcpPermissionOption *self;

  g_return_val_if_fail (id != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_ACP_PERMISSION_OPTION, NULL);
  self->id = g_strdup (id);
  self->label = g_strdup (label);
  self->description = g_strdup (description);
  self->destructive = !!destructive;

  return self;
}

FoundryAcpPermissionOption *
_foundry_acp_permission_option_new_from_json (JsonNode *node)
{
  FoundryAcpPermissionOption *self;
  JsonObject *object;
  const char *id;
  const char *label;
  const char *description;
  gboolean destructive;

  g_return_val_if_fail (node != NULL, NULL);

  if (!JSON_NODE_HOLDS_OBJECT (node))
    return NULL;

  object = json_node_get_object (node);
  id = get_optional_string_member (object, "optionId");
  if (id == NULL)
    id = get_optional_string_member (object, "id");

  if (id == NULL)
    return NULL;

  label = get_optional_string_member (object, "label");
  description = get_optional_string_member (object, "description");
  destructive = get_optional_boolean_member (object, "destructive");

  self = foundry_acp_permission_option_new (id, label, description, destructive);
  self->raw = json_node_ref (node);

  return self;
}

/**
 * foundry_acp_permission_option_dup_id:
 * @self: a [class@Foundry.AcpPermissionOption]
 *
 * Returns: (transfer full): the option identifier
 *
 * Since: 1.2
 */
char *
foundry_acp_permission_option_dup_id (FoundryAcpPermissionOption *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_OPTION (self), NULL);

  return g_strdup (self->id);
}

/**
 * foundry_acp_permission_option_dup_label:
 * @self: a [class@Foundry.AcpPermissionOption]
 *
 * Returns: (transfer full) (nullable): the option label
 *
 * Since: 1.2
 */
char *
foundry_acp_permission_option_dup_label (FoundryAcpPermissionOption *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_OPTION (self), NULL);

  return g_strdup (self->label);
}

/**
 * foundry_acp_permission_option_dup_description:
 * @self: a [class@Foundry.AcpPermissionOption]
 *
 * Returns: (transfer full) (nullable): the option description
 *
 * Since: 1.2
 */
char *
foundry_acp_permission_option_dup_description (FoundryAcpPermissionOption *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_OPTION (self), NULL);

  return g_strdup (self->description);
}

/**
 * foundry_acp_permission_option_get_destructive:
 * @self: a [class@Foundry.AcpPermissionOption]
 *
 * Returns: %TRUE if the option is marked destructive
 *
 * Since: 1.2
 */
gboolean
foundry_acp_permission_option_get_destructive (FoundryAcpPermissionOption *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_OPTION (self), FALSE);

  return self->destructive;
}

/**
 * foundry_acp_permission_option_dup_raw:
 * @self: a [class@Foundry.AcpPermissionOption]
 *
 * Returns: (transfer full) (nullable): the raw protocol JSON
 *
 * Since: 1.2
 */
JsonNode *
foundry_acp_permission_option_dup_raw (FoundryAcpPermissionOption *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_OPTION (self), NULL);

  if (self->raw == NULL)
    return NULL;

  return json_node_ref (self->raw);
}
