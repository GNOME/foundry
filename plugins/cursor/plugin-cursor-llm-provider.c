/* plugin-cursor-llm-provider.c
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

#include <libsoup/soup.h>

#include "foundry-context-private.h"
#include "foundry-settings.h"

#include "plugin-cursor-client.h"
#include "plugin-cursor-llm-model.h"
#include "plugin-cursor-llm-provider.h"

struct _PluginCursorLlmProvider
{
  FoundryLlmProvider  parent_instance;
  SoupSession        *session;
  PluginCursorClient *client;
  FoundrySettings    *settings;
};

G_DEFINE_FINAL_TYPE (PluginCursorLlmProvider, plugin_cursor_llm_provider, FOUNDRY_TYPE_LLM_PROVIDER)

static DexFuture *
plugin_cursor_llm_provider_list_models (FoundryLlmProvider *provider)
{
  PluginCursorLlmProvider *self = (PluginCursorLlmProvider *)provider;

  g_assert (PLUGIN_IS_CURSOR_LLM_PROVIDER (self));

  return plugin_cursor_client_list_models (self->client);
}

static DexFuture *
plugin_cursor_llm_provider_load (FoundryLlmProvider *provider)
{
  PluginCursorLlmProvider *self = (PluginCursorLlmProvider *)provider;
  g_autoptr(FoundryContext) context = NULL;
  g_autofree char *api_key = NULL;

  g_assert (PLUGIN_IS_CURSOR_LLM_PROVIDER (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  self->session = soup_session_new ();
  self->settings = foundry_context_load_settings (context, "app.devsuite.foundry.cursor", NULL);

  api_key = foundry_settings_get_string (self->settings, "api-key");
  self->client = plugin_cursor_client_new (context, self->session, api_key);

  foundry_settings_bind (self->settings, "api-key",
                         self->client, "api-key",
                         G_SETTINGS_BIND_GET);

  return dex_future_new_true ();
}

static DexFuture *
plugin_cursor_llm_provider_unload (FoundryLlmProvider *provider)
{
  PluginCursorLlmProvider *self = (PluginCursorLlmProvider *)provider;

  g_assert (PLUGIN_IS_CURSOR_LLM_PROVIDER (self));

  g_clear_object (&self->session);
  g_clear_object (&self->client);
  g_clear_object (&self->settings);

  return dex_future_new_true ();
}

static void
plugin_cursor_llm_provider_finalize (GObject *object)
{
  PluginCursorLlmProvider *self = (PluginCursorLlmProvider *)object;

  g_clear_object (&self->session);
  g_clear_object (&self->client);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (plugin_cursor_llm_provider_parent_class)->finalize (object);
}

static void
plugin_cursor_llm_provider_class_init (PluginCursorLlmProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLlmProviderClass *llm_provider_class = FOUNDRY_LLM_PROVIDER_CLASS (klass);

  object_class->finalize = plugin_cursor_llm_provider_finalize;

  llm_provider_class->load = plugin_cursor_llm_provider_load;
  llm_provider_class->unload = plugin_cursor_llm_provider_unload;
  llm_provider_class->list_models = plugin_cursor_llm_provider_list_models;
}

static void
plugin_cursor_llm_provider_init (PluginCursorLlmProvider *self)
{
}
