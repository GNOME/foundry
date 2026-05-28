/* foundry-acp-content-block.c
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

#include "foundry-acp-content-block-private.h"
#include "foundry-json-node.h"

typedef enum _FoundryAcpContentBlockKind
{
  FOUNDRY_ACP_CONTENT_BLOCK_TEXT,
  FOUNDRY_ACP_CONTENT_BLOCK_RESOURCE_LINK,
  FOUNDRY_ACP_CONTENT_BLOCK_TEXT_RESOURCE,
} FoundryAcpContentBlockKind;

struct _FoundryAcpContentBlock
{
  GObject parent_instance;

  FoundryAcpContentBlockKind kind;
  char *text;
  char *uri;
  char *name;
  char *mime_type;
};

G_DEFINE_FINAL_TYPE (FoundryAcpContentBlock, foundry_acp_content_block, G_TYPE_OBJECT)

static void
foundry_acp_content_block_finalize (GObject *object)
{
  FoundryAcpContentBlock *self = (FoundryAcpContentBlock *)object;

  g_clear_pointer (&self->text, g_free);
  g_clear_pointer (&self->uri, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->mime_type, g_free);

  G_OBJECT_CLASS (foundry_acp_content_block_parent_class)->finalize (object);
}

static void
foundry_acp_content_block_class_init (FoundryAcpContentBlockClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_acp_content_block_finalize;
}

static void
foundry_acp_content_block_init (FoundryAcpContentBlock *self)
{
}

static FoundryAcpContentBlock *
foundry_acp_content_block_new (FoundryAcpContentBlockKind  kind,
                               const char                 *text,
                               const char                 *uri,
                               const char                 *name,
                               const char                 *mime_type)
{
  FoundryAcpContentBlock *self;

  self = g_object_new (FOUNDRY_TYPE_ACP_CONTENT_BLOCK, NULL);
  self->kind = kind;
  self->text = g_strdup (text);
  self->uri = g_strdup (uri);
  self->name = g_strdup (name);
  self->mime_type = g_strdup (mime_type);

  return self;
}

/**
 * foundry_acp_content_block_new_text:
 * @text: UTF-8 text
 *
 * Creates an ACP text content block.
 *
 * Returns: (transfer full): a [class@Foundry.AcpContentBlock]
 *
 * Since: 1.2
 */
FoundryAcpContentBlock *
foundry_acp_content_block_new_text (const char *text)
{
  g_return_val_if_fail (text != NULL, NULL);
  g_return_val_if_fail (g_utf8_validate (text, -1, NULL), NULL);

  return foundry_acp_content_block_new (FOUNDRY_ACP_CONTENT_BLOCK_TEXT,
                                        text,
                                        NULL,
                                        NULL,
                                        NULL);
}

/**
 * foundry_acp_content_block_new_resource_link:
 * @uri: a resource URI
 * @name: (nullable): a display name
 * @mime_type: (nullable): a MIME type
 *
 * Creates an ACP resource link content block.
 *
 * Returns: (transfer full): a [class@Foundry.AcpContentBlock]
 *
 * Since: 1.2
 */
FoundryAcpContentBlock *
foundry_acp_content_block_new_resource_link (const char *uri,
                                             const char *name,
                                             const char *mime_type)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return foundry_acp_content_block_new (FOUNDRY_ACP_CONTENT_BLOCK_RESOURCE_LINK,
                                        NULL,
                                        uri,
                                        name,
                                        mime_type);
}

/**
 * foundry_acp_content_block_new_text_resource:
 * @uri: a resource URI
 * @mime_type: (nullable): a MIME type
 * @text: UTF-8 resource contents
 *
 * Creates an embedded text resource content block.
 *
 * Returns: (transfer full): a [class@Foundry.AcpContentBlock]
 *
 * Since: 1.2
 */
FoundryAcpContentBlock *
foundry_acp_content_block_new_text_resource (const char *uri,
                                             const char *mime_type,
                                             const char *text)
{
  g_return_val_if_fail (uri != NULL, NULL);
  g_return_val_if_fail (text != NULL, NULL);
  g_return_val_if_fail (g_utf8_validate (text, -1, NULL), NULL);

  return foundry_acp_content_block_new (FOUNDRY_ACP_CONTENT_BLOCK_TEXT_RESOURCE,
                                        text,
                                        uri,
                                        NULL,
                                        mime_type);
}

/**
 * foundry_acp_content_block_dup_text:
 * @self: a [class@Foundry.AcpContentBlock]
 *
 * Returns: (transfer full) (nullable): the text
 *
 * Since: 1.2
 */
char *
foundry_acp_content_block_dup_text (FoundryAcpContentBlock *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_CONTENT_BLOCK (self), NULL);

  return g_strdup (self->text);
}

/**
 * foundry_acp_content_block_dup_uri:
 * @self: a [class@Foundry.AcpContentBlock]
 *
 * Returns: (transfer full) (nullable): the resource URI
 *
 * Since: 1.2
 */
char *
foundry_acp_content_block_dup_uri (FoundryAcpContentBlock *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_CONTENT_BLOCK (self), NULL);

  return g_strdup (self->uri);
}

/**
 * foundry_acp_content_block_dup_name:
 * @self: a [class@Foundry.AcpContentBlock]
 *
 * Returns: (transfer full) (nullable): the display name
 *
 * Since: 1.2
 */
char *
foundry_acp_content_block_dup_name (FoundryAcpContentBlock *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_CONTENT_BLOCK (self), NULL);

  return g_strdup (self->name);
}

/**
 * foundry_acp_content_block_dup_mime_type:
 * @self: a [class@Foundry.AcpContentBlock]
 *
 * Returns: (transfer full) (nullable): the MIME type
 *
 * Since: 1.2
 */
char *
foundry_acp_content_block_dup_mime_type (FoundryAcpContentBlock *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_CONTENT_BLOCK (self), NULL);

  return g_strdup (self->mime_type);
}

JsonNode *
_foundry_acp_content_block_to_json (FoundryAcpContentBlock *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_CONTENT_BLOCK (self), NULL);

  switch (self->kind)
    {
    case FOUNDRY_ACP_CONTENT_BLOCK_TEXT:
      return FOUNDRY_JSON_OBJECT_NEW ("type", "text",
                                      "text", FOUNDRY_JSON_NODE_PUT_STRING (self->text));

    case FOUNDRY_ACP_CONTENT_BLOCK_RESOURCE_LINK:
      return FOUNDRY_JSON_OBJECT_NEW ("type", "resource_link",
                                      "uri", FOUNDRY_JSON_NODE_PUT_STRING (self->uri),
                                      "name", FOUNDRY_JSON_NODE_PUT_STRING (self->name),
                                      "mimeType", FOUNDRY_JSON_NODE_PUT_STRING (self->mime_type));

    case FOUNDRY_ACP_CONTENT_BLOCK_TEXT_RESOURCE:
      return FOUNDRY_JSON_OBJECT_NEW ("type", "resource",
                                      "resource", "{",
                                        "uri", FOUNDRY_JSON_NODE_PUT_STRING (self->uri),
                                        "mimeType", FOUNDRY_JSON_NODE_PUT_STRING (self->mime_type),
                                        "text", FOUNDRY_JSON_NODE_PUT_STRING (self->text),
                                      "}");

    default:
      g_return_val_if_reached (NULL);
    }
}
