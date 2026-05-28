/* foundry-acp-agent.c
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

#include "foundry-acp-agent.h"
#include "foundry-acp-client.h"
#include "foundry-acp-enums.h"
#include "foundry-build-pipeline.h"

G_DEFINE_ABSTRACT_TYPE (FoundryAcpAgent, foundry_acp_agent, FOUNDRY_TYPE_CONTEXTUAL)

enum {
  PROP_0,
  PROP_ID,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_acp_agent_real_open_session (FoundryAcpAgent      *self,
                                     FoundryAcpClient     *client,
                                     FoundryBuildPipeline *pipeline)
{
  g_assert (FOUNDRY_IS_ACP_AGENT (self));
  g_assert (FOUNDRY_IS_ACP_CLIENT (client));

  return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                FOUNDRY_ACP_ERROR_UNSUPPORTED,
                                "ACP agent does not implement open_session()");
}

static DexFuture *
foundry_acp_agent_real_resume_session (FoundryAcpAgent      *self,
                                       FoundryAcpClient     *client,
                                       FoundryBuildPipeline *pipeline,
                                       const char           *session_id)
{
  g_assert (FOUNDRY_IS_ACP_AGENT (self));
  g_assert (FOUNDRY_IS_ACP_CLIENT (client));

  return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                FOUNDRY_ACP_ERROR_UNSUPPORTED,
                                "ACP agent does not implement resume_session()");
}

static FoundryAcpAgentCapabilityFlags
foundry_acp_agent_real_get_capabilities (FoundryAcpAgent *self)
{
  g_assert (FOUNDRY_IS_ACP_AGENT (self));

  return FOUNDRY_ACP_AGENT_CAPABILITY_NONE;
}

static char *
foundry_acp_agent_real_dup_id (FoundryAcpAgent *self)
{
  g_assert (FOUNDRY_IS_ACP_AGENT (self));

  return g_strdup (G_OBJECT_TYPE_NAME (self));
}

static char *
foundry_acp_agent_real_dup_name (FoundryAcpAgent *self)
{
  g_assert (FOUNDRY_IS_ACP_AGENT (self));

  return g_strdup (G_OBJECT_TYPE_NAME (self));
}

static void
foundry_acp_agent_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  FoundryAcpAgent *self = FOUNDRY_ACP_AGENT (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_take_string (value, foundry_acp_agent_dup_id (self));
      break;

    case PROP_NAME:
      g_value_take_string (value, foundry_acp_agent_dup_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_acp_agent_class_init (FoundryAcpAgentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_acp_agent_get_property;

  klass->dup_id = foundry_acp_agent_real_dup_id;
  klass->dup_name = foundry_acp_agent_real_dup_name;
  klass->get_capabilities = foundry_acp_agent_real_get_capabilities;
  klass->open_session = foundry_acp_agent_real_open_session;
  klass->resume_session = foundry_acp_agent_real_resume_session;

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_acp_agent_init (FoundryAcpAgent *self)
{
}

/**
 * foundry_acp_agent_dup_id:
 * @self: a [class@Foundry.AcpAgent]
 *
 * Returns: (transfer full): the agent identifier
 *
 * Since: 1.2
 */
char *
foundry_acp_agent_dup_id (FoundryAcpAgent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_AGENT (self), NULL);

  return FOUNDRY_ACP_AGENT_GET_CLASS (self)->dup_id (self);
}

/**
 * foundry_acp_agent_dup_name:
 * @self: a [class@Foundry.AcpAgent]
 *
 * Returns: (transfer full): the user-visible agent name
 *
 * Since: 1.2
 */
char *
foundry_acp_agent_dup_name (FoundryAcpAgent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_AGENT (self), NULL);

  return FOUNDRY_ACP_AGENT_GET_CLASS (self)->dup_name (self);
}

/**
 * foundry_acp_agent_get_capabilities:
 * @self: a [class@Foundry.AcpAgent]
 *
 * Gets provider-side capabilities supported by @self.
 *
 * If %FOUNDRY_ACP_AGENT_CAPABILITY_RESUME_SESSION is set,
 * [method@Foundry.AcpAgent.resume_session] is a supported operation for
 * session identifiers previously issued by this agent or provider. It does not
 * imply that every arbitrary session identifier is valid.
 *
 * Returns: the agent capability flags
 *
 * Since: 1.2
 */
FoundryAcpAgentCapabilityFlags
foundry_acp_agent_get_capabilities (FoundryAcpAgent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_AGENT (self), FOUNDRY_ACP_AGENT_CAPABILITY_NONE);

  return FOUNDRY_ACP_AGENT_GET_CLASS (self)->get_capabilities (self);
}

/**
 * foundry_acp_agent_open_session:
 * @self: a [class@Foundry.AcpAgent]
 * @client: a [iface@Foundry.AcpClient] to receive client-side requests
 * @pipeline: (nullable): a [class@Foundry.BuildPipeline] to use for launch
 *
 * Starts a fresh ACP session for @self.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_agent_open_session (FoundryAcpAgent      *self,
                                FoundryAcpClient     *client,
                                FoundryBuildPipeline *pipeline)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_AGENT (self), NULL);
  g_return_val_if_fail (FOUNDRY_IS_ACP_CLIENT (client), NULL);
  g_return_val_if_fail (pipeline == NULL || FOUNDRY_IS_BUILD_PIPELINE (pipeline), NULL);

  return FOUNDRY_ACP_AGENT_GET_CLASS (self)->open_session (self, client, pipeline);
}

/**
 * foundry_acp_agent_resume_session:
 * @self: a [class@Foundry.AcpAgent]
 * @client: a [iface@Foundry.AcpClient] to receive client-side requests
 * @pipeline: (nullable): a [class@Foundry.BuildPipeline] to use for launch
 * @session_id: session identifier to resume
 *
 * Starts a session using @session_id. Implementations may multiplex sessions
 * or map the identifier to a process-specific transport.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_agent_resume_session (FoundryAcpAgent      *self,
                                  FoundryAcpClient     *client,
                                  FoundryBuildPipeline *pipeline,
                                  const char           *session_id)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_AGENT (self), NULL);
  g_return_val_if_fail (FOUNDRY_IS_ACP_CLIENT (client), NULL);
  g_return_val_if_fail (pipeline == NULL || FOUNDRY_IS_BUILD_PIPELINE (pipeline), NULL);
  g_return_val_if_fail (session_id != NULL, NULL);

  return FOUNDRY_ACP_AGENT_GET_CLASS (self)->resume_session (self, client, pipeline, session_id);
}
