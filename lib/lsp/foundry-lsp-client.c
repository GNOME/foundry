/* foundry-lsp-client.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <jsonrpc-glib.h>

#include "foundry-lsp-client.h"
#include "foundry-lsp-provider.h"
#include "foundry-service-private.h"

struct _FoundryLspClient
{
  FoundryContextual   parent_instance;
  FoundryLspProvider *provider;
  JsonrpcClient      *client;
};

struct _FoundryLspClientClass
{
  FoundryContextualClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryLspClient, foundry_lsp_client, FOUNDRY_TYPE_CONTEXTUAL)

enum {
  PROP_0,
  PROP_CLIENT,
  PROP_IO_STREAM,
  PROP_PROVIDER,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_lsp_client_finalize (GObject *object)
{
  FoundryLspClient *self = (FoundryLspClient *)object;

  g_clear_object (&self->client);
  g_clear_object (&self->provider);

  G_OBJECT_CLASS (foundry_lsp_client_parent_class)->finalize (object);
}

static void
foundry_lsp_client_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryLspClient *self = FOUNDRY_LSP_CLIENT (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, self->client);
      break;

    case PROP_PROVIDER:
      g_value_set_object (value, self->provider);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_lsp_client_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  FoundryLspClient *self = FOUNDRY_LSP_CLIENT (object);

  switch (prop_id)
    {
    case PROP_IO_STREAM:
      self->client = jsonrpc_client_new (g_value_get_object (value));
      break;

    case PROP_PROVIDER:
      self->provider = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_lsp_client_class_init (FoundryLspClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_lsp_client_finalize;
  object_class->get_property = foundry_lsp_client_get_property;
  object_class->set_property = foundry_lsp_client_set_property;

  properties[PROP_CLIENT] =
    g_param_spec_object ("client", NULL, NULL,
                         JSONRPC_TYPE_CLIENT,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_IO_STREAM] =
    g_param_spec_object ("io-stream", NULL, NULL,
                         G_TYPE_IO_STREAM,
                         (G_PARAM_WRITABLE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PROVIDER] =
    g_param_spec_object ("provider", NULL, NULL,
                         FOUNDRY_TYPE_LSP_PROVIDER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_lsp_client_init (FoundryLspClient *self)
{
}

/**
 * foundry_lsp_client_query_capabilities:
 * @self: a #FoundryLspClient
 *
 * Queries the servers capabilities.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when
 *   the query completes or fails.
 */
DexFuture *
foundry_lsp_client_query_capabilities (FoundryLspClient *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_LSP_CLIENT (self));

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "not supported");
}

/**
 * foundry_lsp_client_call:
 * @self: a #FoundryLspClient
 * @method: the method name to call
 * @params: parameters for the method call
 *
 * If @params is floating, the reference will be consumed.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when
 *   a reply is received for the method call.
 */
DexFuture *
foundry_lsp_client_call (FoundryLspClient *self,
                         const char       *method,
                         GVariant         *params)
{
  dex_return_error_if_fail (FOUNDRY_IS_LSP_CLIENT (self));

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "not supported");
}

/**
 * foundry_lsp_client_notify:
 * @self: a #FoundryLspClient
 * @method: the method name to call
 * @params: parameters for the notification
 *
 * If @params is floating, the reference will be consumed.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when
 *   the notification has been sent.
 */
DexFuture *
foundry_lsp_client_notify (FoundryLspClient *self,
                           const char       *method,
                           GVariant         *params)
{
  dex_return_error_if_fail (FOUNDRY_IS_LSP_CLIENT (self));

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "not supported");
}

FoundryLspClient *
foundry_lsp_client_new (FoundryContext *context,
                        GIOStream      *io_stream)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (G_IS_IO_STREAM (io_stream), NULL);

  return g_object_new (FOUNDRY_TYPE_LSP_CLIENT,
                       "context", context,
                       "io-stream", io_stream,
                       NULL);
}
