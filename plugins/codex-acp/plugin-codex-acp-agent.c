/*
 * plugin-codex-acp-agent.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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
 * You should have received a copy of the GNU Lesser General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include "plugin-codex-acp-agent.h"
#include "plugin-codex-build-addin.h"

struct _PluginCodexAcpAgent
{
  FoundryAcpAgent parent_instance;
};

typedef struct _PluginCodexAcpClient
{
  GObject parent_instance;
  FoundryAcpClient *delegate;
  FoundryAcpProjectClient *project_client;
} PluginCodexAcpClient;

typedef struct _PluginCodexAcpClientClass
{
  GObjectClass parent_class;
} PluginCodexAcpClientClass;

static void plugin_codex_acp_client_iface_init (FoundryAcpClientInterface *iface);

GType plugin_codex_acp_client_get_type (void);

G_DEFINE_TYPE (PluginCodexAcpAgent, plugin_codex_acp_agent, FOUNDRY_TYPE_ACP_AGENT)
G_DEFINE_FINAL_TYPE_WITH_CODE (PluginCodexAcpClient,
                               plugin_codex_acp_client,
                               G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (FOUNDRY_TYPE_ACP_CLIENT,
                                                      plugin_codex_acp_client_iface_init))

static FoundryAcpClientCapabilityFlags plugin_codex_acp_agent_get_client_capabilities
                                                        (FoundryAcpClient *client);

typedef struct _OpenSession
{
  FoundryAcpAgent *agent;
  FoundryAcpClient *client;
  FoundryBuildPipeline *pipeline;
  char *session_id;
} OpenSession;

static PluginCodexAcpClient *
plugin_codex_acp_client_new (FoundryAcpClient        *delegate,
                             FoundryAcpProjectClient *project_client)
{
  PluginCodexAcpClient *self;

  g_assert (FOUNDRY_IS_ACP_CLIENT (delegate));
  g_assert (FOUNDRY_IS_ACP_PROJECT_CLIENT (project_client));

  self = g_object_new (plugin_codex_acp_client_get_type (), NULL);
  self->delegate = g_object_ref (delegate);
  self->project_client = g_object_ref (project_client);

  return self;
}

static DexFuture *
plugin_codex_acp_client_session_update (FoundryAcpClient        *client,
                                        FoundryAcpSession       *session,
                                        FoundryAcpSessionUpdate *update)
{
  PluginCodexAcpClient *self = (PluginCodexAcpClient *)client;

  g_assert (plugin_codex_acp_client_get_type () == G_OBJECT_TYPE (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (update != NULL);

  return foundry_acp_client_session_update (self->delegate, session, update);
}

static DexFuture *
plugin_codex_acp_client_request_permission (FoundryAcpClient            *client,
                                            FoundryAcpSession           *session,
                                            FoundryAcpPermissionRequest *request)
{
  PluginCodexAcpClient *self = (PluginCodexAcpClient *)client;
  FoundryAcpClientInterface *iface;

  g_assert (plugin_codex_acp_client_get_type () == G_OBJECT_TYPE (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (request != NULL);

  iface = FOUNDRY_ACP_CLIENT_GET_IFACE (self->delegate);

  if (iface->request_permission != NULL)
    return foundry_acp_client_request_permission (self->delegate, session, request);

  return foundry_acp_client_request_permission (FOUNDRY_ACP_CLIENT (self->project_client),
                                               session,
                                               request);
}

static DexFuture *
plugin_codex_acp_client_read_text_file (FoundryAcpClient  *client,
                                        FoundryAcpSession *session,
                                        const char        *path,
                                        guint              line,
                                        guint              limit)
{
  PluginCodexAcpClient *self = (PluginCodexAcpClient *)client;

  g_assert (plugin_codex_acp_client_get_type () == G_OBJECT_TYPE (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (path != NULL);

  return foundry_acp_client_read_text_file (FOUNDRY_ACP_CLIENT (self->project_client),
                                            session,
                                            path,
                                            line,
                                            limit);
}

static DexFuture *
plugin_codex_acp_client_write_text_file (FoundryAcpClient  *client,
                                         FoundryAcpSession *session,
                                         const char        *path,
                                         const char        *content)
{
  PluginCodexAcpClient *self = (PluginCodexAcpClient *)client;

  g_assert (plugin_codex_acp_client_get_type () == G_OBJECT_TYPE (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (path != NULL);
  g_assert (content != NULL);

  return foundry_acp_client_write_text_file (FOUNDRY_ACP_CLIENT (self->project_client),
                                             session,
                                             path,
                                             content);
}

static DexFuture *
plugin_codex_acp_client_create_terminal (FoundryAcpClient   *client,
                                         FoundryAcpSession  *session,
                                         const char         *command,
                                         const char * const *argv,
                                         const char         *cwd,
                                         const char * const *environ,
                                         gssize              output_byte_limit)
{
  PluginCodexAcpClient *self = (PluginCodexAcpClient *)client;

  g_assert (plugin_codex_acp_client_get_type () == G_OBJECT_TYPE (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (command != NULL);

  return foundry_acp_client_create_terminal (FOUNDRY_ACP_CLIENT (self->project_client),
                                             session,
                                             command,
                                             argv,
                                             cwd,
                                             environ,
                                             output_byte_limit);
}

static DexFuture *
plugin_codex_acp_client_terminal_output (FoundryAcpClient  *client,
                                         FoundryAcpSession *session,
                                         const char        *terminal_id)
{
  PluginCodexAcpClient *self = (PluginCodexAcpClient *)client;

  g_assert (plugin_codex_acp_client_get_type () == G_OBJECT_TYPE (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (terminal_id != NULL);

  return foundry_acp_client_terminal_output (FOUNDRY_ACP_CLIENT (self->project_client),
                                             session,
                                             terminal_id);
}

static DexFuture *
plugin_codex_acp_client_terminal_wait_for_exit (FoundryAcpClient  *client,
                                                FoundryAcpSession *session,
                                                const char        *terminal_id)
{
  PluginCodexAcpClient *self = (PluginCodexAcpClient *)client;

  g_assert (plugin_codex_acp_client_get_type () == G_OBJECT_TYPE (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (terminal_id != NULL);

  return foundry_acp_client_terminal_wait_for_exit (FOUNDRY_ACP_CLIENT (self->project_client),
                                                    session,
                                                    terminal_id);
}

static DexFuture *
plugin_codex_acp_client_terminal_kill (FoundryAcpClient  *client,
                                       FoundryAcpSession *session,
                                       const char        *terminal_id)
{
  PluginCodexAcpClient *self = (PluginCodexAcpClient *)client;

  g_assert (plugin_codex_acp_client_get_type () == G_OBJECT_TYPE (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (terminal_id != NULL);

  return foundry_acp_client_terminal_kill (FOUNDRY_ACP_CLIENT (self->project_client),
                                           session,
                                           terminal_id);
}

static DexFuture *
plugin_codex_acp_client_terminal_release (FoundryAcpClient  *client,
                                          FoundryAcpSession *session,
                                          const char        *terminal_id)
{
  PluginCodexAcpClient *self = (PluginCodexAcpClient *)client;

  g_assert (plugin_codex_acp_client_get_type () == G_OBJECT_TYPE (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));
  g_assert (terminal_id != NULL);

  return foundry_acp_client_terminal_release (FOUNDRY_ACP_CLIENT (self->project_client),
                                              session,
                                              terminal_id);
}

static DexFuture *
plugin_codex_acp_client_refresh_changed_files (FoundryAcpClient  *client,
                                               FoundryAcpSession *session)
{
  PluginCodexAcpClient *self = (PluginCodexAcpClient *)client;

  g_assert (plugin_codex_acp_client_get_type () == G_OBJECT_TYPE (self));
  g_assert (FOUNDRY_IS_ACP_SESSION (session));

  return foundry_acp_client_refresh_changed_files (FOUNDRY_ACP_CLIENT (self->project_client),
                                                   session);
}

static void
plugin_codex_acp_client_finalize (GObject *object)
{
  PluginCodexAcpClient *self = (PluginCodexAcpClient *)object;

  g_clear_object (&self->delegate);
  g_clear_object (&self->project_client);

  G_OBJECT_CLASS (plugin_codex_acp_client_parent_class)->finalize (object);
}

static void
plugin_codex_acp_client_class_init (PluginCodexAcpClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_codex_acp_client_finalize;
}

static void
plugin_codex_acp_client_init (PluginCodexAcpClient *self)
{
}

static void
plugin_codex_acp_client_iface_init (FoundryAcpClientInterface *iface)
{
  iface->session_update = plugin_codex_acp_client_session_update;
  iface->request_permission = plugin_codex_acp_client_request_permission;
  iface->read_text_file = plugin_codex_acp_client_read_text_file;
  iface->write_text_file = plugin_codex_acp_client_write_text_file;
  iface->create_terminal = plugin_codex_acp_client_create_terminal;
  iface->terminal_output = plugin_codex_acp_client_terminal_output;
  iface->terminal_wait_for_exit = plugin_codex_acp_client_terminal_wait_for_exit;
  iface->terminal_kill = plugin_codex_acp_client_terminal_kill;
  iface->terminal_release = plugin_codex_acp_client_terminal_release;
  iface->refresh_changed_files = plugin_codex_acp_client_refresh_changed_files;
}

static void
open_session_free (OpenSession *state)
{
  g_clear_object (&state->agent);
  g_clear_object (&state->client);
  g_clear_object (&state->pipeline);
  g_clear_pointer (&state->session_id, g_free);
  g_free (state);
}

static OpenSession *
open_session_new (FoundryAcpAgent      *agent,
                  FoundryAcpClient     *client,
                  FoundryBuildPipeline *pipeline,
                  const char           *session_id)
{
  OpenSession *state;

  g_assert (FOUNDRY_IS_ACP_AGENT (agent));
  g_assert (FOUNDRY_IS_ACP_CLIENT (client));
  g_assert (FOUNDRY_IS_BUILD_PIPELINE (pipeline));

  state = g_new0 (OpenSession, 1);
  state->agent = g_object_ref (agent);
  state->client = g_object_ref (client);
  state->pipeline = g_object_ref (pipeline);
  state->session_id = g_strdup (session_id);

  return state;
}

static DexFuture *
plugin_codex_acp_agent_open_session_fiber (gpointer user_data)
{
  OpenSession *state = user_data;
  g_autoptr (PluginCodexBuildAddin) addin = NULL;
  g_autoptr (FoundryAcpClient) client = NULL;
  g_autoptr (FoundryAcpConnection) connection = NULL;
  g_autoptr (FoundryAcpProjectClient) project_client = NULL;
  g_autoptr (FoundryContext) context = NULL;
  g_autoptr (FoundryAcpSession) session = NULL;
  g_autoptr (GError) error = NULL;
  FoundryAcpClientCapabilityFlags client_capabilities;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_ACP_AGENT (state->agent));
  g_assert (FOUNDRY_IS_ACP_CLIENT (state->client));
  g_assert (FOUNDRY_IS_BUILD_PIPELINE (state->pipeline));

  addin = plugin_codex_build_addin_for_pipeline (state->pipeline);
  if (addin == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                 FOUNDRY_ACP_ERROR_INVALID_REQUEST,
                                 "Failed to locate ACP addin for pipeline");

  if (!(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (state->pipeline))))
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                  FOUNDRY_ACP_ERROR_INVALID_REQUEST,
                                  "Pipeline does not have a context");

  project_client = foundry_acp_project_client_new (context);
  client = FOUNDRY_ACP_CLIENT (plugin_codex_acp_client_new (state->client, project_client));

  connection = plugin_codex_build_addin_get_connection (addin,
                                                        state->pipeline,
                                                        state->agent,
                                                        client,
                                                        &error);
  if (connection == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!dex_await (foundry_acp_connection_start (connection), &error))
    {
      plugin_codex_build_addin_reset_connection (addin);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  client_capabilities = plugin_codex_acp_agent_get_client_capabilities (client);

  if (!dex_await (foundry_acp_connection_initialize (connection,
                                                     "codex-acp",
                                                     "Codex ACP",
                                                     FOUNDRY_VERSION_S,
                                                     client_capabilities),
                  &error))
    {
      plugin_codex_build_addin_reset_connection (addin);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (!(session = dex_await_object (foundry_acp_connection_new_session (connection, NULL),
                                    &error)))
    {
      plugin_codex_build_addin_reset_connection (addin);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_take_object (g_steal_pointer (&session));
}

static FoundryAcpClientCapabilityFlags
plugin_codex_acp_agent_get_client_capabilities (FoundryAcpClient *client)
{
  FoundryAcpClientInterface *iface;
  FoundryAcpClientCapabilityFlags ret = FOUNDRY_ACP_CLIENT_CAPABILITY_NONE;

  g_assert (FOUNDRY_IS_ACP_CLIENT (client));

  iface = FOUNDRY_ACP_CLIENT_GET_IFACE (client);

  if (iface->read_text_file != NULL)
    ret |= FOUNDRY_ACP_CLIENT_CAPABILITY_FS_READ_TEXT_FILE;

  if (iface->write_text_file != NULL)
    ret |= FOUNDRY_ACP_CLIENT_CAPABILITY_FS_WRITE_TEXT_FILE;

  if (iface->create_terminal != NULL &&
      iface->terminal_output != NULL &&
      iface->terminal_wait_for_exit != NULL &&
      iface->terminal_kill != NULL &&
      iface->terminal_release != NULL)
    ret |= FOUNDRY_ACP_CLIENT_CAPABILITY_TERMINAL;

  return ret;
}

static DexFuture *
plugin_codex_acp_agent_open_session (FoundryAcpAgent      *agent,
                                     FoundryAcpClient     *client,
                                     FoundryBuildPipeline *pipeline)
{
  g_assert (FOUNDRY_IS_ACP_AGENT (agent));
  g_assert (FOUNDRY_IS_ACP_CLIENT (client));

  if (pipeline == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                 FOUNDRY_ACP_ERROR_INVALID_REQUEST,
                                 "Pipeline is required");

  return dex_scheduler_spawn (NULL, 0,
                              plugin_codex_acp_agent_open_session_fiber,
                              open_session_new (agent, client, pipeline, NULL),
                              (GDestroyNotify)open_session_free);
}

static DexFuture *
plugin_codex_acp_agent_resume_session (FoundryAcpAgent     *agent,
                                       FoundryAcpClient    *client,
                                       FoundryBuildPipeline *pipeline,
                                       const char          *session_id)
{
  g_assert (FOUNDRY_IS_ACP_AGENT (agent));
  g_assert (FOUNDRY_IS_ACP_CLIENT (client));

  if (pipeline == NULL)
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                 FOUNDRY_ACP_ERROR_INVALID_REQUEST,
                                 "Pipeline is required");

  if (session_id == NULL || session_id[0] == '\0')
    return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                 FOUNDRY_ACP_ERROR_INVALID_REQUEST,
                                 "Session id is required");

  return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                FOUNDRY_ACP_ERROR_UNSUPPORTED,
                                "Resuming Codex ACP sessions is not supported yet");
}

static void
plugin_codex_acp_agent_init (PluginCodexAcpAgent *self)
{
}

static char *
plugin_codex_acp_agent_dup_id (FoundryAcpAgent *agent)
{
  return g_strdup ("codex");
}

static char *
plugin_codex_acp_agent_dup_name (FoundryAcpAgent *agent)
{
  return g_strdup (_("Codex"));
}

static void
plugin_codex_acp_agent_class_init (PluginCodexAcpAgentClass *klass)
{
  FoundryAcpAgentClass *agent_class = FOUNDRY_ACP_AGENT_CLASS (klass);

  agent_class->dup_id = plugin_codex_acp_agent_dup_id;
  agent_class->dup_name = plugin_codex_acp_agent_dup_name;
  agent_class->open_session = plugin_codex_acp_agent_open_session;
  agent_class->resume_session = plugin_codex_acp_agent_resume_session;
}
