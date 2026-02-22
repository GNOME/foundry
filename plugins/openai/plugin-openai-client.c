/* plugin-openai-client.c
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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <fcntl.h>

#include <glib/gstdio.h>

#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

#include <foundry.h>
#include <foundry-soup.h>

#include "plugin-openai-client.h"
#include "plugin-openai-llm-model.h"

struct _PluginOpenaiClient
{
  FoundryContextual parent_instance;
  SoupSession *session;
  char *url_base;
  char *api_key;
};

enum {
  PROP_0,
  PROP_SESSION,
  PROP_URL_BASE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PluginOpenaiClient, plugin_openai_client, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];
static gboolean debug_jsonrpc;

static void
plugin_openai_client_constructed (GObject *object)
{
  PluginOpenaiClient *self = (PluginOpenaiClient *)object;

  G_OBJECT_CLASS (plugin_openai_client_parent_class)->constructed (object);

  g_assert (PLUGIN_IS_OPENAI_CLIENT (self));
  g_assert (SOUP_IS_SESSION (self->session));
  g_assert (self->url_base != NULL);
}

static void
plugin_openai_client_finalize (GObject *object)
{
  PluginOpenaiClient *self = (PluginOpenaiClient *)object;

  g_clear_object (&self->session);
  g_clear_pointer (&self->url_base, g_free);
  g_clear_pointer (&self->api_key, g_free);

  G_OBJECT_CLASS (plugin_openai_client_parent_class)->finalize (object);
}

static void
plugin_openai_client_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  PluginOpenaiClient *self = PLUGIN_OPENAI_CLIENT (object);

  switch (prop_id)
    {
    case PROP_SESSION:
      g_value_set_object (value, self->session);
      break;

    case PROP_URL_BASE:
      g_value_set_string (value, self->url_base);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_openai_client_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  PluginOpenaiClient *self = PLUGIN_OPENAI_CLIENT (object);

  switch (prop_id)
    {
    case PROP_SESSION:
      self->session = g_value_dup_object (value);
      break;

    case PROP_URL_BASE:
      self->url_base = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_openai_client_class_init (PluginOpenaiClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = plugin_openai_client_constructed;
  object_class->finalize = plugin_openai_client_finalize;
  object_class->get_property = plugin_openai_client_get_property;
  object_class->set_property = plugin_openai_client_set_property;

  properties[PROP_SESSION] =
    g_param_spec_object ("session", NULL, NULL,
                         SOUP_TYPE_SESSION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_URL_BASE] =
    g_param_spec_string ("url-base", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  debug_jsonrpc = g_getenv ("JSONRPC_DEBUG") != NULL;
}

static void
plugin_openai_client_init (PluginOpenaiClient *self)
{
}

PluginOpenaiClient *
plugin_openai_client_new (FoundryContext *context,
                          SoupSession    *session,
                          const char     *url_base)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (SOUP_IS_SESSION (session), NULL);

  if (url_base == NULL)
    url_base = "https://api.openai.com/v1/";

  return g_object_new (PLUGIN_TYPE_OPENAI_CLIENT,
                       "context", context,
                       "session", session,
                       "url-base", url_base,
                       NULL);
}

static char *
plugin_openai_client_dup_url (PluginOpenaiClient *self,
                              const char         *suffix)
{
  g_return_val_if_fail (PLUGIN_IS_OPENAI_CLIENT (self), NULL);
  g_return_val_if_fail (self->url_base != NULL, NULL);

  if (g_str_has_suffix (self->url_base, "/"))
    {
      while (suffix[0] == '/')
        suffix++;
    }

  return g_strconcat (self->url_base, suffix, NULL);
}

static char *
plugin_openai_client_get_api_key (PluginOpenaiClient *self,
                                  GError            **error)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundrySecretService) secret_service = NULL;
  g_autofree char *api_key = NULL;

  g_assert (PLUGIN_IS_OPENAI_CLIENT (self));

  if (self->api_key != NULL)
    {
      if (foundry_str_empty0 (self->api_key))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       "OpenAI API key is not configured");
          return NULL;
        }

      return g_strdup (self->api_key);
    }

  if (!(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to get context");
      return NULL;
    }

  secret_service = foundry_context_dup_secret_service (context);

  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (secret_service)), error))
    return NULL;

  api_key = dex_await_string (foundry_secret_service_lookup_api_key (secret_service,
                                                                     "api.openai.com",
                                                                     "openai"),
                              error);

  if (api_key == NULL)
    return NULL;

  if (foundry_str_empty0 (api_key))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "OpenAI API key is not configured");
      return NULL;
    }

  g_set_str (&self->api_key, api_key);

  return g_strdup (self->api_key);
}

static DexFuture *
plugin_openai_client_list_models_fiber (gpointer user_data)
{
  PluginOpenaiClient *self = user_data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(SoupMessage) message = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *url = NULL;
  g_autofree char *api_key = NULL;
  SoupMessageHeaders *headers;
  JsonObject *root_obj;
  JsonArray *data_ar;
  JsonNode *data;

  g_assert (PLUGIN_IS_OPENAI_CLIENT (self));

  if (!(api_key = plugin_openai_client_get_api_key (self, &error)))
    {
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      return foundry_future_new_not_supported ();
    }

  url = plugin_openai_client_dup_url (self, "models");
  message = soup_message_new (SOUP_METHOD_GET, url);
  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  headers = soup_message_get_request_headers (message);
  soup_message_headers_append (headers, "Authorization", g_strconcat ("Bearer ", api_key, NULL));

  if (!(bytes = dex_await_boxed (foundry_soup_session_send_and_read (self->session, message), &error)) ||
      !(node = dex_await_boxed (foundry_json_node_from_bytes (bytes), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (JSON_NODE_HOLDS_OBJECT (node) &&
      (root_obj = json_node_get_object (node)) &&
      json_object_has_member (root_obj, "data") &&
      (data = json_object_get_member (root_obj, "data")) &&
      JSON_NODE_HOLDS_ARRAY (data) &&
      (data_ar = json_node_get_array (data)))
    {
      g_autoptr(GListStore) store = g_list_store_new (PLUGIN_TYPE_OPENAI_LLM_MODEL);
      guint length = json_array_get_length (data_ar);

      for (guint i = 0; i < length; i++)
        {
          JsonNode *model = json_array_get_element (data_ar, i);

          if (JSON_NODE_HOLDS_OBJECT (model))
            {
              g_autoptr(PluginOpenaiLlmModel) item = plugin_openai_llm_model_new (context, self, model);

              if (item != NULL)
                g_list_store_append (store, item);
            }
        }

      return dex_future_new_take_object (g_steal_pointer (&store));
    }

  return foundry_future_new_not_supported ();
}

/**
 * plugin_openai_client_list_models:
 * @self: a [class@Plugin.OpenaiClient]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves
 *   to a [iface@Gio.ListModel].
 */
DexFuture *
plugin_openai_client_list_models (PluginOpenaiClient *self)
{
  dex_return_error_if_fail (PLUGIN_IS_OPENAI_CLIENT (self));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_openai_client_list_models_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static DexFuture *
plugin_openai_client_post_fiber (SoupSession *session,
                                 const char  *url,
                                 const char  *api_key,
                                 JsonNode    *body_node)
{
  g_autoptr(GOutputStream) output = NULL;
  g_autoptr(GInputStream) response_stream = NULL;
  g_autoptr(GInputStream) input = NULL;
  g_autoptr(SoupMessage) message = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  g_autofd int read_fd = -1;
  g_autofd int write_fd = -1;
  SoupMessageHeaders *headers;
  guint status_code;

  g_assert (SOUP_IS_SESSION (session));
  g_assert (url != NULL);
  g_assert (body_node != NULL);
  g_assert (api_key != NULL);

  if (!(bytes = dex_await_boxed (foundry_json_node_to_bytes (body_node), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!foundry_pipe (&read_fd, &write_fd, O_CLOEXEC|O_NONBLOCK, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  input = g_unix_input_stream_new (g_steal_fd (&read_fd), TRUE);
  output = g_unix_output_stream_new (g_steal_fd (&write_fd), TRUE);

  message = soup_message_new (SOUP_METHOD_POST, url);
  soup_message_set_request_body_from_bytes (message, "application/json", bytes);

  headers = soup_message_get_request_headers (message);
  soup_message_headers_append (headers, "Authorization", g_strconcat ("Bearer ", api_key, NULL));

  /* Use send_async so we can check the status code */
  if (!(response_stream = dex_await_object (foundry_soup_session_send (session, message), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  status_code = soup_message_get_status (message);

  if (status_code < 200 || status_code >= 300)
    {
      g_autofree char *error_body = NULL;
      g_autoptr(GBytes) error_bytes = NULL;

      /* Try to read error response body for better error message */
      if ((error_bytes = dex_await_boxed (dex_input_stream_read_bytes (response_stream, 4096, G_PRIORITY_DEFAULT), NULL)))
        {
          const char *data = g_bytes_get_data (error_bytes, NULL);
          gsize size = g_bytes_get_size (error_bytes);

          if (size > 0 && data != NULL)
            error_body = g_strndup (data, size);
        }

      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "HTTP %u: %s",
                                    status_code,
                                    error_body ? error_body : soup_status_get_phrase (status_code));
    }

  /* Splice the response body to our pipe in the background */
  g_output_stream_splice_async (output,
                                response_stream,
                                G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                NULL,
                                NULL);

  if (debug_jsonrpc)
    FOUNDRY_DUMP_BYTES (openai,
                        ((const char *)g_bytes_get_data (bytes, NULL)),
                        g_bytes_get_size (bytes));

  return dex_future_new_take_object (g_steal_pointer (&input));
}

/**
 * plugin_openai_client_post:
 * @self: a [class@Plugin.OpenaiClient]
 * @path: the API path (e.g. "chat/completions")
 * @body: the JSON body to send
 *
 * Does a HTTP post to @path (using `PluginOpenaiClient:url-base`) and returns
 * an [iface@Gio.InputStream] which can be read as new data is received.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.InputStream] or rejects with error.
 */
typedef struct {
  PluginOpenaiClient *client;
  char               *path;
  JsonNode           *body;
} PostState;

static void
post_state_free (PostState *state)
{
  if (state == NULL)
    return;

  g_clear_object (&state->client);
  g_clear_pointer (&state->path, g_free);
  g_clear_pointer (&state->body, json_node_unref);
  g_free (state);
}

static DexFuture *
plugin_openai_client_post_after_key_fiber (gpointer user_data)
{
  PostState *state = user_data;
  g_autofree char *url = NULL;
  g_autofree char *api_key = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (state != NULL);
  g_assert (PLUGIN_IS_OPENAI_CLIENT (state->client));
  g_assert (state->path != NULL);
  g_assert (state->body != NULL);

  if (!(api_key = plugin_openai_client_get_api_key (state->client, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (g_str_has_suffix (state->client->url_base, "/"))
    {
      const char *path = state->path;

      while (path[0] == '/')
        path++;

      url = g_strconcat (state->client->url_base, path, NULL);
    }
  else
    {
      url = g_strconcat (state->client->url_base, state->path, NULL);
    }

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_openai_client_post_fiber),
                                  4,
                                  SOUP_TYPE_SESSION, state->client->session,
                                  G_TYPE_STRING, url,
                                  G_TYPE_STRING, api_key,
                                  JSON_TYPE_NODE, state->body);
}

/**
 * plugin_openai_client_post:
 * @self: a [class@Plugin.OpenaiClient]
 * @path: the API path (e.g. "chat/completions")
 * @body: the JSON body to send
 *
 * Does a HTTP post to @path (using `PluginOpenaiClient:url-base`) and returns
 * an [iface@Gio.InputStream] which can be read as new data is received.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.InputStream] or rejects with error.
 */
DexFuture *
plugin_openai_client_post (PluginOpenaiClient *self,
                           const char         *path,
                           JsonNode           *body)
{
  PostState *state;

  dex_return_error_if_fail (PLUGIN_IS_OPENAI_CLIENT (self));
  dex_return_error_if_fail (path != NULL);
  dex_return_error_if_fail (body != NULL);

  state = g_new0 (PostState, 1);
  state->client = g_object_ref (self);
  state->path = g_strdup (path);
  state->body = json_node_ref (body);

  return dex_scheduler_spawn (NULL, 0,
                              plugin_openai_client_post_after_key_fiber,
                              state,
                              (GDestroyNotify) post_state_free);
}
