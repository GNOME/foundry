/* foundry-acp-agent.h
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

#pragma once

#include <libdex.h>

#include "foundry-contextual.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_ACP_AGENT (foundry_acp_agent_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_DERIVABLE_TYPE (FoundryAcpAgent, foundry_acp_agent, FOUNDRY, ACP_AGENT, FoundryContextual)

struct _FoundryAcpAgentClass
{
  FoundryContextualClass parent_class;

  char                            *(*dup_id)          (FoundryAcpAgent        *self);
  char                            *(*dup_name)        (FoundryAcpAgent        *self);
  DexFuture                       *(*open_session)    (FoundryAcpAgent        *self,
                                                       FoundryAcpClient       *client,
                                                       FoundryBuildPipeline   *pipeline);
  DexFuture                       *(*resume_session)  (FoundryAcpAgent        *self,
                                                       FoundryAcpClient       *client,
                                                       FoundryBuildPipeline   *pipeline,
                                                       const char             *session_id);
  FoundryAcpAgentCapabilityFlags  (*get_capabilities) (FoundryAcpAgent        *self);

  /*< private >*/
  gpointer _reserved[11];
};

FOUNDRY_AVAILABLE_IN_1_2
char                           *foundry_acp_agent_dup_id           (FoundryAcpAgent      *self);
FOUNDRY_AVAILABLE_IN_1_2
char                           *foundry_acp_agent_dup_name         (FoundryAcpAgent      *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryAcpAgentCapabilityFlags  foundry_acp_agent_get_capabilities (FoundryAcpAgent      *self);
FOUNDRY_AVAILABLE_IN_1_2
DexFuture                      *foundry_acp_agent_open_session     (FoundryAcpAgent      *self,
                                                                    FoundryAcpClient     *client,
                                                                    FoundryBuildPipeline *pipeline);
FOUNDRY_AVAILABLE_IN_1_2
DexFuture                      *foundry_acp_agent_resume_session   (FoundryAcpAgent      *self,
                                                                    FoundryAcpClient     *client,
                                                                    FoundryBuildPipeline *pipeline,
                                                                    const char           *session_id);

G_END_DECLS
