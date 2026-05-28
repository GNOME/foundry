/*
 * test-acp-agent.c
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

#include <foundry.h>

typedef struct _TestAcpAgent
{
  FoundryAcpAgent parent_instance;
} TestAcpAgent;

typedef struct _TestAcpAgentClass
{
  FoundryAcpAgentClass parent_class;
} TestAcpAgentClass;

GType test_acp_agent_get_type (void);

G_DEFINE_FINAL_TYPE (TestAcpAgent, test_acp_agent, FOUNDRY_TYPE_ACP_AGENT)

static void
test_acp_agent_class_init (TestAcpAgentClass *klass)
{
}

static void
test_acp_agent_init (TestAcpAgent *self)
{
}

typedef struct _TestResumeAcpAgent
{
  FoundryAcpAgent parent_instance;
} TestResumeAcpAgent;

typedef struct _TestResumeAcpAgentClass
{
  FoundryAcpAgentClass parent_class;
} TestResumeAcpAgentClass;

GType test_resume_acp_agent_get_type (void);

G_DEFINE_FINAL_TYPE (TestResumeAcpAgent, test_resume_acp_agent, FOUNDRY_TYPE_ACP_AGENT)

static FoundryAcpAgentCapabilityFlags
test_resume_acp_agent_get_capabilities (FoundryAcpAgent *agent)
{
  g_assert (FOUNDRY_IS_ACP_AGENT (agent));

  return FOUNDRY_ACP_AGENT_CAPABILITY_RESUME_SESSION;
}

static void
test_resume_acp_agent_class_init (TestResumeAcpAgentClass *klass)
{
  FoundryAcpAgentClass *agent_class = FOUNDRY_ACP_AGENT_CLASS (klass);

  agent_class->get_capabilities = test_resume_acp_agent_get_capabilities;
}

static void
test_resume_acp_agent_init (TestResumeAcpAgent *self)
{
}

static void
test_agent_default_capabilities (void)
{
  g_autoptr(FoundryAcpAgent) agent = NULL;

  agent = g_object_new (test_acp_agent_get_type (), NULL);

  g_assert_cmpint (foundry_acp_agent_get_capabilities (agent), ==,
                   FOUNDRY_ACP_AGENT_CAPABILITY_NONE);
}

static void
test_agent_resume_capability (void)
{
  g_autoptr(FoundryAcpAgent) agent = NULL;
  FoundryAcpAgentCapabilityFlags capabilities;

  agent = g_object_new (test_resume_acp_agent_get_type (), NULL);
  capabilities = foundry_acp_agent_get_capabilities (agent);

  g_assert_true ((capabilities & FOUNDRY_ACP_AGENT_CAPABILITY_RESUME_SESSION) != 0);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Foundry/AcpAgent/default-capabilities",
                   test_agent_default_capabilities);
  g_test_add_func ("/Foundry/AcpAgent/resume-capability",
                   test_agent_resume_capability);

  return g_test_run ();
}
