/* plugin-ollama-client.c
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

#include <foundry.h>
#include <foundry-soup.h>

#include "plugin-ollama-client.h"
#include "plugin-ollama-llm-model.h"

struct _PluginOllamaClient
{
  FoundryContextual parent_instance;
  SoupSession *session;
  char *url_base;
};

enum {
  PROP_0,
  PROP_SESSION,
  PROP_URL_BASE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PluginOllamaClient, plugin_ollama_client, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static void
plugin_ollama_client_constructed (GObject *object)
{
  PluginOllamaClient *self = (PluginOllamaClient *)object;

  G_OBJECT_CLASS (plugin_ollama_client_parent_class)->constructed (object);

  g_assert (PLUGIN_IS_OLLAMA_CLIENT (self));
  g_assert (SOUP_IS_SESSION (self->session));
  g_assert (self->url_base != NULL);
}

static void
plugin_ollama_client_finalize (GObject *object)
{
  PluginOllamaClient *self = (PluginOllamaClient *)object;

  g_clear_object (&self->session);
  g_clear_pointer (&self->url_base, g_free);

  G_OBJECT_CLASS (plugin_ollama_client_parent_class)->finalize (object);
}

static void
plugin_ollama_client_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  PluginOllamaClient *self = PLUGIN_OLLAMA_CLIENT (object);

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
plugin_ollama_client_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  PluginOllamaClient *self = PLUGIN_OLLAMA_CLIENT (object);

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
plugin_ollama_client_class_init (PluginOllamaClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = plugin_ollama_client_constructed;
  object_class->finalize = plugin_ollama_client_finalize;
  object_class->get_property = plugin_ollama_client_get_property;
  object_class->set_property = plugin_ollama_client_set_property;

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
}

static void
plugin_ollama_client_init (PluginOllamaClient *self)
{
}

PluginOllamaClient *
plugin_ollama_client_new (FoundryContext *context,
                          SoupSession    *session,
                          const char     *url_base)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (SOUP_IS_SESSION (session), NULL);

  if (url_base == NULL)
    url_base = "http://127.0.0.1:11434/";

  return g_object_new (PLUGIN_TYPE_OLLAMA_CLIENT,
                       "session", session,
                       "url-base", url_base,
                       NULL);
}

static char *
plugin_ollama_client_dup_url (PluginOllamaClient *self,
                              const char         *suffix)
{
  g_return_val_if_fail (PLUGIN_IS_OLLAMA_CLIENT (self), NULL);
  g_return_val_if_fail (self->url_base != NULL, NULL);

  if (g_str_has_suffix (self->url_base, "/"))
    {
      while (suffix[0] == '/')
        suffix++;
    }

  return g_strconcat (self->url_base, suffix, NULL);
}

static DexFuture *
plugin_ollama_client_list_models_fiber (gpointer user_data)
{
  PluginOllamaClient *self = user_data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(SoupMessage) message = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *url = NULL;
  JsonObject *root_obj;
  JsonArray *models_ar;
  JsonNode *models;

  g_assert (PLUGIN_IS_OLLAMA_CLIENT (self));

  url = plugin_ollama_client_dup_url (self, "/api/tags");
  message = soup_message_new (SOUP_METHOD_GET, url);
  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

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
      g_autoptr(GListStore) store = g_list_store_new (PLUGIN_TYPE_OLLAMA_LLM_MODEL);
      guint length = json_array_get_length (models_ar);

      for (guint i = 0; i < length; i++)
        {
          JsonNode *model = json_array_get_element (models_ar, i);

          if (JSON_NODE_HOLDS_OBJECT (model))
            {
              g_autoptr(PluginOllamaLlmModel) item = plugin_ollama_llm_model_new (context, self, model);

              if (item != NULL)
                g_list_store_append (store, item);
            }
        }

      return dex_future_new_take_object (g_steal_pointer (&store));
    }

  return foundry_future_new_not_supported ();
}

/**
 * plugin_ollama_client_list_models:
 * @self: a [class@Plugin.OllamaClient]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves
 *   to a [iface@Gio.ListModel].
 */
DexFuture *
plugin_ollama_client_list_models (PluginOllamaClient *self)
{
  dex_return_error_if_fail (PLUGIN_IS_OLLAMA_CLIENT (self));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_ollama_client_list_models_fiber,
                              g_object_ref (self),
                              g_object_unref);
}
