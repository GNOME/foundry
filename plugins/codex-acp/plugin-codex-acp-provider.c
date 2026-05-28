/*
 * plugin-codex-acp-provider.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "plugin-codex-acp-agent.h"
#include "plugin-codex-acp-provider.h"

#include <foundry.h>

struct _PluginCodexAcpProvider
{
  FoundryAcpProvider parent_instance;

  FoundryBuildManager *build_manager;
  FoundryAcpAgent *agent;

  gulong pipeline_invalidated_handler;

  guint agent_added : 1;
};

G_DEFINE_FINAL_TYPE (PluginCodexAcpProvider, plugin_codex_acp_provider, FOUNDRY_TYPE_ACP_PROVIDER)

static DexFuture *
plugin_codex_acp_provider_update_agent_fiber (gpointer user_data)
{
  PluginCodexAcpProvider *self = user_data;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *codex_acp_path = NULL;

  g_assert (PLUGIN_IS_CODEX_ACP_PROVIDER (self));

  if (self->build_manager == NULL)
    {
      if (self->agent_added)
        {
          foundry_acp_provider_agent_removed (FOUNDRY_ACP_PROVIDER (self), self->agent);
          self->agent_added = FALSE;
        }

      return dex_future_new_true ();
    }

  pipeline = dex_await_object (foundry_build_manager_load_pipeline (self->build_manager), &error);
  if (pipeline != NULL)
    {
      codex_acp_path =
        dex_await_string (foundry_build_pipeline_contains_program (pipeline, "codex-acp"),
                          &error);
    }

  if (codex_acp_path == NULL)
    {
      if (self->agent_added)
        {
          foundry_acp_provider_agent_removed (FOUNDRY_ACP_PROVIDER (self), self->agent);
          self->agent_added = FALSE;
        }

      if (error != NULL)
        {
          g_debug ("Failed to determine `codex-acp` availability: %s", error->message);
        }

      return dex_future_new_true ();
    }

  if (!self->agent_added)
    {
      foundry_acp_provider_agent_added (FOUNDRY_ACP_PROVIDER (self), self->agent);
      self->agent_added = TRUE;
    }

  return dex_future_new_true ();
}

static void
plugin_codex_acp_provider_on_pipeline_invalidated (FoundryBuildManager *build_manager,
                                                  gpointer            user_data)
{
  PluginCodexAcpProvider *self = user_data;

  g_assert (FOUNDRY_IS_BUILD_MANAGER (build_manager));
  g_assert (PLUGIN_IS_CODEX_ACP_PROVIDER (self));

  dex_future_disown (dex_scheduler_spawn (NULL, 0,
                                          plugin_codex_acp_provider_update_agent_fiber,
                                          g_object_ref (self),
                                          g_object_unref));
}

static DexFuture *
plugin_codex_acp_provider_load_fiber (gpointer user_data)
{
  g_autoptr(FoundryContext) context = NULL;
  PluginCodexAcpProvider *self = user_data;

  g_assert (PLUGIN_IS_CODEX_ACP_PROVIDER (self));

  if (self->agent == NULL)
    self->agent = g_object_new (PLUGIN_TYPE_CODEX_ACP_AGENT, NULL);

  if (self->agent_added)
    self->agent_added = FALSE;

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  g_set_object (&self->build_manager, foundry_context_dup_build_manager (context));

  if (self->build_manager == NULL)
    {
      return dex_future_new_true ();
    }

  if (self->pipeline_invalidated_handler == 0)
    {
      self->pipeline_invalidated_handler =
        g_signal_connect_object (self->build_manager,
                                "pipeline-invalidated",
                                G_CALLBACK (plugin_codex_acp_provider_on_pipeline_invalidated),
                                self,
                                0);
    }

  return plugin_codex_acp_provider_update_agent_fiber (self);
}

static DexFuture *
plugin_codex_acp_provider_load (FoundryAcpProvider *provider)
{
  g_assert (PLUGIN_IS_CODEX_ACP_PROVIDER (provider));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_codex_acp_provider_load_fiber,
                              g_object_ref (provider),
                              g_object_unref);
}

static DexFuture *
plugin_codex_acp_provider_unload (FoundryAcpProvider *provider)
{
  PluginCodexAcpProvider *self = (PluginCodexAcpProvider *)provider;

  g_assert (PLUGIN_IS_CODEX_ACP_PROVIDER (self));

  if (self->agent_added)
    {
      foundry_acp_provider_agent_removed (FOUNDRY_ACP_PROVIDER (self), self->agent);
      self->agent_added = FALSE;
    }

  if (self->pipeline_invalidated_handler)
    {
      g_clear_signal_handler (&self->pipeline_invalidated_handler, self->build_manager);
    }

  g_clear_object (&self->build_manager);

  return dex_future_new_true ();
}

static void
plugin_codex_acp_provider_dispose (GObject *object)
{
  PluginCodexAcpProvider *self = (PluginCodexAcpProvider *)object;

  if (self->pipeline_invalidated_handler)
    {
      g_clear_signal_handler (&self->pipeline_invalidated_handler, self->build_manager);
    }

  g_clear_object (&self->build_manager);
  g_clear_object (&self->agent);

  G_OBJECT_CLASS (plugin_codex_acp_provider_parent_class)->dispose (object);
}

static void
plugin_codex_acp_provider_class_init (PluginCodexAcpProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryAcpProviderClass *acp_provider_class = FOUNDRY_ACP_PROVIDER_CLASS (klass);

  object_class->dispose = plugin_codex_acp_provider_dispose;
  acp_provider_class->load = plugin_codex_acp_provider_load;
  acp_provider_class->unload = plugin_codex_acp_provider_unload;
}

static void
plugin_codex_acp_provider_init (PluginCodexAcpProvider *self)
{
  self->pipeline_invalidated_handler = 0;
  self->agent_added = FALSE;
}
