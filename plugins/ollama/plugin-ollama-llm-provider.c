/* plugin-ollama-llm-provider.c
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

#include <libsoup/soup.h>

#include <foundry-secret-service.h>

#include "plugin-ollama-client.h"
#include "plugin-ollama-llm-provider.h"

struct _PluginOllamaLlmProvider
{
  FoundryLlmProvider    parent_instance;
  SoupSession          *session;
  PluginOllamaClient   *client;
  FoundrySettings      *settings;
  FoundrySecretService *secret_service;
  char                 *api_key;
  gulong                settings_changed_id;
};

G_DEFINE_FINAL_TYPE (PluginOllamaLlmProvider, plugin_ollama_llm_provider, FOUNDRY_TYPE_LLM_PROVIDER)

static DexFuture *
plugin_ollama_llm_provider_list_models (FoundryLlmProvider *provider)
{
  PluginOllamaLlmProvider *self = (PluginOllamaLlmProvider *)provider;

  g_assert (PLUGIN_IS_OLLAMA_LLM_PROVIDER (self));

  return plugin_ollama_client_list_models (self->client);
}

static DexFuture *
plugin_ollama_llm_provider_update_client_fiber (gpointer user_data)
{
  PluginOllamaLlmProvider *self = user_data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *host = NULL;
  g_autofree char *url_base = NULL;
  g_autofree char *api_key = NULL;
  guint port;
  gboolean use_tls;

  g_assert (PLUGIN_IS_OLLAMA_LLM_PROVIDER (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  host = foundry_settings_get_string (self->settings, "host");
  port = foundry_settings_get_uint (self->settings, "port");
  use_tls = foundry_settings_get_boolean (self->settings, "use-tls");

  if (foundry_str_empty0 (host))
    host = g_strdup ("localhost");

  if (port == 0)
    port = 11434;

  url_base = g_strdup_printf ("%s://%s:%u/", use_tls ? "https" : "http", host, port);

  if (self->secret_service != NULL)
    {
      if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (self->secret_service)), &error))
        {
          /* Continue without API key if secret service fails */
          g_clear_error (&error);
        }
      else
        {
          g_autofree char *lookup_key = NULL;
          lookup_key = dex_await_string (foundry_secret_service_lookup_api_key (self->secret_service, host, "ollama"), &error);
          if (!foundry_str_empty0 (lookup_key))
            g_set_str (&api_key, lookup_key);
          g_clear_error (&error);
        }
    }

  g_clear_object (&self->client);
  g_clear_pointer (&self->api_key, g_free);

  self->client = plugin_ollama_client_new (context, self->session, url_base, api_key, use_tls);
  self->api_key = g_strdup (api_key);

  return dex_future_new_true ();
}

static DexFuture *
plugin_ollama_llm_provider_update_client (PluginOllamaLlmProvider *self)
{
  g_assert (PLUGIN_IS_OLLAMA_LLM_PROVIDER (self));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_ollama_llm_provider_update_client_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static void
plugin_ollama_llm_provider_settings_changed_cb (PluginOllamaLlmProvider *self,
                                                const char              *key,
                                                FoundrySettings         *settings)
{
  g_assert (PLUGIN_IS_OLLAMA_LLM_PROVIDER (self));
  g_assert (key != NULL);
  g_assert (FOUNDRY_IS_SETTINGS (settings));

  if (g_str_equal (key, "host") || g_str_equal (key, "port") || g_str_equal (key, "use-tls"))
    plugin_ollama_llm_provider_update_client (self);
}

static DexFuture *
plugin_ollama_llm_provider_load (FoundryLlmProvider *provider)
{
  PluginOllamaLlmProvider *self = (PluginOllamaLlmProvider *)provider;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (PLUGIN_IS_OLLAMA_LLM_PROVIDER (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  self->session = soup_session_new ();
  self->settings = foundry_context_load_settings (context, "app.devsuite.foundry.ollama", NULL);
  self->secret_service = foundry_context_dup_secret_service (context);

  self->settings_changed_id = g_signal_connect_object (self->settings,
                                                       "changed",
                                                       G_CALLBACK (plugin_ollama_llm_provider_settings_changed_cb),
                                                       self,
                                                       G_CONNECT_SWAPPED);

  if (!dex_await (plugin_ollama_llm_provider_update_client (self), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
plugin_ollama_llm_provider_unload (FoundryLlmProvider *provider)
{
  PluginOllamaLlmProvider *self = (PluginOllamaLlmProvider *)provider;

  g_assert (PLUGIN_IS_OLLAMA_LLM_PROVIDER (self));

  g_clear_signal_handler (&self->settings_changed_id, self->settings);

  g_clear_object (&self->session);
  g_clear_object (&self->client);
  g_clear_object (&self->settings);
  g_clear_object (&self->secret_service);
  g_clear_pointer (&self->api_key, g_free);

  return dex_future_new_true ();
}

static void
plugin_ollama_llm_provider_dispose (GObject *object)
{
  PluginOllamaLlmProvider *self = (PluginOllamaLlmProvider *)object;

  g_clear_signal_handler (&self->settings_changed_id, self->settings);

  g_clear_object (&self->session);
  g_clear_object (&self->client);
  g_clear_object (&self->settings);
  g_clear_object (&self->secret_service);
  g_clear_pointer (&self->api_key, g_free);

  G_OBJECT_CLASS (plugin_ollama_llm_provider_parent_class)->dispose (object);
}

static void
plugin_ollama_llm_provider_class_init (PluginOllamaLlmProviderClass *klass)
{
  FoundryLlmProviderClass *llm_provider_class = FOUNDRY_LLM_PROVIDER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = plugin_ollama_llm_provider_dispose;

  llm_provider_class->load = plugin_ollama_llm_provider_load;
  llm_provider_class->unload = plugin_ollama_llm_provider_unload;
  llm_provider_class->list_models = plugin_ollama_llm_provider_list_models;
}

static void
plugin_ollama_llm_provider_init (PluginOllamaLlmProvider *self)
{
}
