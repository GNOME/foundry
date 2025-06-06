/* foundry-mcp-client.c
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

#include "foundry-jsonrpc-private.h"
#include "foundry-mcp-client.h"

struct _FoundryMcpClient
{
  GObject        parent_instance;
  GIOStream     *stream;
  JsonrpcClient *client;
};

enum {
  PROP_0,
  PROP_STREAM,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryMcpClient, foundry_mcp_client, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_mcp_client_constructed (GObject *object)
{
  FoundryMcpClient *self = (FoundryMcpClient *)object;

  G_OBJECT_CLASS (foundry_mcp_client_parent_class)->constructed (object);

  g_return_if_fail (self->stream != NULL);

  self->client = jsonrpc_client_new (self->stream);
}

static void
foundry_mcp_client_dispose (GObject *object)
{
  FoundryMcpClient *self = (FoundryMcpClient *)object;

  g_clear_object (&self->client);
  g_clear_object (&self->stream);

  G_OBJECT_CLASS (foundry_mcp_client_parent_class)->dispose (object);
}

static void
foundry_mcp_client_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryMcpClient *self = FOUNDRY_MCP_CLIENT (object);

  switch (prop_id)
    {
    case PROP_STREAM:
      g_value_set_object (value, self->stream);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_mcp_client_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  FoundryMcpClient *self = FOUNDRY_MCP_CLIENT (object);

  switch (prop_id)
    {
    case PROP_STREAM:
      self->stream = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_mcp_client_class_init (FoundryMcpClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = foundry_mcp_client_constructed;
  object_class->dispose = foundry_mcp_client_dispose;
  object_class->get_property = foundry_mcp_client_get_property;
  object_class->set_property = foundry_mcp_client_set_property;

  properties[PROP_STREAM] =
    g_param_spec_object ("stream", NULL, NULL,
                         G_TYPE_IO_STREAM,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_mcp_client_init (FoundryMcpClient *self)
{
}

FoundryMcpClient *
foundry_mcp_client_new (GIOStream *stream)
{
  g_return_val_if_fail (G_IS_IO_STREAM (stream), NULL);

  return g_object_new (G_TYPE_IO_STREAM,
                       "stream", stream,
                       NULL);
}

/**
 * foundry_mcp_client_initialize:
 * @self: a [class@Foundry.McpClient]
 * @params: parameters for the initialize request
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [struct@GLib.Variant] with the reply.
 */
DexFuture *
foundry_mcp_client_initialize (FoundryMcpClient *self,
                               GVariant         *params)
{
  dex_return_error_if_fail (FOUNDRY_IS_MCP_CLIENT (self));
  dex_return_error_if_fail (self->client != NULL);

  if (g_variant_is_floating (params))
    g_variant_take_ref (params);

  return _jsonrpc_client_call (self->client, "initialize", params);
}

/**
 * foundry_mcp_client_ping:
 * @self: a [class@Foundry.McpClient]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   boolean or rejects with error.
 */
DexFuture *
foundry_mcp_client_ping (FoundryMcpClient *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_MCP_CLIENT (self));
  dex_return_error_if_fail (self->client != NULL);

  return _jsonrpc_client_call (self->client, "ping", NULL);
}
