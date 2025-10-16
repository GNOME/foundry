/* plugin-cursor-client.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <foundry.h>
#include <foundry-soup.h>

#include "plugin-cursor-client.h"
#include "plugin-cursor-llm-model.h"

struct _PluginCursorClient
{
  FoundryContextual  parent_instance;
  SoupSession       *session;
  char              *api_key;
  char              *url_base;
};

G_DEFINE_FINAL_TYPE (PluginCursorClient, plugin_cursor_client, FOUNDRY_TYPE_CONTEXTUAL)

enum {
  PROP_0,
  PROP_API_KEY,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
plugin_cursor_client_finalize (GObject *object)
{
  PluginCursorClient *self = (PluginCursorClient *)object;

  g_clear_object (&self->session);
  g_clear_pointer (&self->api_key, g_free);
  g_clear_pointer (&self->url_base, g_free);

  G_OBJECT_CLASS (plugin_cursor_client_parent_class)->finalize (object);
}

static void
plugin_cursor_client_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  PluginCursorClient *self = PLUGIN_CURSOR_CLIENT (object);

  switch (prop_id)
    {
    case PROP_API_KEY:
      g_value_set_string (value, self->api_key);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_cursor_client_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  PluginCursorClient *self = PLUGIN_CURSOR_CLIENT (object);

  switch (prop_id)
    {
    case PROP_API_KEY:
      if (g_set_str (&self->api_key, g_value_get_string (value)))
        g_object_notify_by_pspec (object, pspec);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_cursor_client_class_init (PluginCursorClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_cursor_client_finalize;
  object_class->get_property = plugin_cursor_client_get_property;
  object_class->set_property = plugin_cursor_client_set_property;

  properties[PROP_API_KEY] =
    g_param_spec_string ("api-key", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_cursor_client_init (PluginCursorClient *self)
{
  self->url_base = g_strdup ("https://api.cursor.com/");
}

PluginCursorClient *
plugin_cursor_client_new (FoundryContext *context,
                          SoupSession    *session,
                          const char     *api_key)
{
  PluginCursorClient *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (SOUP_IS_SESSION (session), NULL);

  self = g_object_new (PLUGIN_TYPE_CURSOR_CLIENT,
                       "context", context,
                       NULL);

  self->session = g_object_ref (session);
  self->api_key = g_strdup (api_key);

  return g_steal_pointer (&self);
}

static char *
plugin_cursor_client_dup_url (PluginCursorClient *self,
                              const char         *suffix)
{
  g_return_val_if_fail (PLUGIN_IS_CURSOR_CLIENT (self), NULL);
  g_return_val_if_fail (self->url_base != NULL, NULL);

  if (g_str_has_suffix (self->url_base, "/"))
    {
      while (suffix[0] == '/')
        suffix++;
    }

  return g_strconcat (self->url_base, suffix, NULL);
}

static SoupMessage *
plugin_cursor_client_create_message (PluginCursorClient *self,
                                     const char         *method,
                                     const char         *path)
{
  g_autoptr(SoupMessage) message = NULL;
  g_autofree char *url = NULL;
  g_autofree char *token = NULL;

  g_assert (PLUGIN_IS_CURSOR_CLIENT (self));
  g_assert (method != NULL);
  g_assert (path != NULL);

  url = plugin_cursor_client_dup_url (self, "/v0/models");
  message = soup_message_new (method, url);

  if (!foundry_str_empty0 (self->api_key))
    {
      g_autofree char *bearer = g_strdup_printf ("Bearer %s", self->api_key);

      soup_message_headers_append (soup_message_get_request_headers (message),
                                   "Authorization", bearer);
    }

  return g_steal_pointer (&message);
}

static DexFuture *
plugin_cursor_client_list_models_fiber (gpointer user_data)
{
  PluginCursorClient *self = user_data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(SoupMessage) message = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  JsonObject *root_obj;
  JsonArray *models_ar;
  JsonNode *models;

  dex_return_error_if_fail (PLUGIN_IS_CURSOR_CLIENT (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  message = plugin_cursor_client_create_message (self, SOUP_METHOD_GET, "/v0/models");

  if (!(bytes = dex_await_boxed (foundry_soup_session_send_and_read (self->session, message), &error)) ||
      !(node = dex_await_boxed (foundry_json_node_from_bytes (bytes), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (JSON_NODE_HOLDS_OBJECT (node) &&
      (root_obj = json_node_get_object (node)) &&
      json_object_has_member (root_obj, "models") &&
      (models = json_object_get_member (root_obj, "models")) &&
      JSON_NODE_HOLDS_ARRAY (models) &&
      (models_ar = json_node_get_array (models)))
    {
      g_autoptr(GListStore) store = g_list_store_new (PLUGIN_TYPE_CURSOR_LLM_MODEL);
      guint length = json_array_get_length (models_ar);

      for (guint i = 0; i < length; i++)
        {
          JsonNode *element = json_array_get_element (models_ar, i);
          g_autoptr(PluginCursorLlmModel) item = plugin_cursor_llm_model_new (context, self, element);

          if (item != NULL)
            g_list_store_append (store, item);
        }

      return dex_future_new_take_object (g_steal_pointer (&store));
    }

  return foundry_future_new_not_supported ();
}

/**
 * plugin_cursor_client_list_models:
 * @self: a [class@Plugin.CursorClient]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.LlmModel].
 */
DexFuture *
plugin_cursor_client_list_models (PluginCursorClient *self)
{
  dex_return_error_if_fail (PLUGIN_IS_CURSOR_CLIENT (self));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_cursor_client_list_models_fiber,
                              g_object_ref (self),
                              g_object_unref);
}
