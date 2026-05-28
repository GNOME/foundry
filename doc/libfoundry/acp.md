Title: Agent Client Protocol

# Agent Client Protocol

Foundry's Agent Client Protocol support lets an application expose a project to
agent subprocesses while keeping the policy, file access, terminal access, and
user interface in the application. Agents are discovered through plugins and
presented as [class@Foundry.AcpAgent] objects from the [class@Foundry.AcpManager]
service.

The ACP API is part of `libfoundry` and is available when the library is built
with `feature-acp`.

```c
#include <foundry.h>

#ifdef FOUNDRY_FEATURE_ACP
  /* ACP-specific code */
#endif
```

## Application Integration

Applications start from their [class@Foundry.Context]. The context owns a
[class@Foundry.AcpManager], and the manager tracks loaded
[class@Foundry.AcpProvider] plugins and their available agents.

```c
g_autoptr(FoundryAcpManager) acp_manager = NULL;
g_autoptr(GListModel) agents = NULL;

acp_manager = foundry_context_dup_acp_manager (context);
agents = foundry_acp_manager_list_agents (acp_manager);
```

`agents` is a [iface@Gio.ListModel] of [class@Foundry.AcpAgent] instances. A
GTK application can bind that model directly to a drop-down or list view; a
headless application can call [method@Foundry.AcpManager.find_agent] with a
stable agent identifier.

Opening a session requires an application-provided [iface@Foundry.AcpClient].
The client is the boundary where the agent calls back into the application for
updates, permission prompts, file operations, and terminal operations.

```c
g_autoptr(FoundryAcpAgent) agent = NULL;
g_autoptr(FoundryAcpSession) session = NULL;
g_autoptr(GError) error = NULL;

agent = dex_await_object (foundry_acp_manager_find_agent (acp_manager, "codex"), &error);

if (agent != NULL)
  session = dex_await_object (foundry_acp_agent_open_session (agent,
                                                             FOUNDRY_ACP_CLIENT (client),
                                                             pipeline),
                              &error);
```

The [class@Foundry.BuildPipeline] gives the agent plugin the same build and
runtime environment that the application would use for project commands. For
example, the bundled Codex ACP plugin prepares the pipeline for execution and
starts `codex-acp` in the project directory.

Once a session is open, send prompts as [class@Foundry.AcpContentBlock]
arrays. The returned future resolves to [class@Foundry.AcpPromptResult].

```c
g_autoptr(FoundryAcpContentBlock) text = NULL;
g_autoptr(FoundryAcpPromptResult) result = NULL;
FoundryAcpContentBlock *content[2];

text = foundry_acp_content_block_new_text ("Summarize the pending changes");
content[0] = text;
content[1] = NULL;

result = dex_await_boxed (foundry_acp_session_prompt (session, content, 1), &error);
```

Applications should keep the session object around while the conversation is
visible. The session exposes list models for event history, active terminals,
and changed files:

 * [method@Foundry.AcpSession.list_events]
 * [method@Foundry.AcpSession.list_active_terminals]
 * [method@Foundry.AcpSession.list_changed_files]

These models are updated as ACP notifications are received and as the
application refreshes changed-file state.

## Implementing an AcpClient

[iface@Foundry.AcpClient] is implemented by the application or by an adapter
owned by the application. The interface methods all return [class@Dex.Future]
so UI prompts, file operations, and terminal operations can finish
asynchronously.

At minimum, most interactive applications implement:

 * `session_update` to append agent messages, tool calls, progress, file
   changes, and errors to the conversation UI
 * `request_permission` to show a permission dialog and resolve to
   [class@Foundry.AcpPermissionResponse]
 * `read_text_file` and `write_text_file` if the agent may read or edit project
   files
 * the terminal methods if the agent may run commands
 * `refresh_changed_files` so the session's changed-file model matches the VCS
   state after prompts or terminal commands

For common project-scoped file and terminal behavior, delegate to
[class@Foundry.AcpProjectClient]. It provides a reusable implementation backed
by a [class@Foundry.Context] or project directory, including a headless
[class@Foundry.AcpPermissionPolicy].

```c
static DexFuture *
my_acp_client_read_text_file (FoundryAcpClient  *client,
                              FoundryAcpSession *session,
                              const char        *path,
                              guint              line,
                              guint              limit)
{
  MyAcpClient *self = (MyAcpClient *)client;

  return foundry_acp_client_read_text_file (FOUNDRY_ACP_CLIENT (self->project_client),
                                            session,
                                            path,
                                            line,
                                            limit);
}
```

This split is useful because the application can keep control of UI-facing
methods such as `session_update` and `request_permission`, while using Foundry's
default project client for low-level file and terminal work.

## Permission Model

ACP agents must not assume unrestricted access to the project. Permission
requests arrive through [method@Foundry.AcpClient.request_permission] with a
[class@Foundry.AcpPermissionRequest]. The application chooses an option and
returns [class@Foundry.AcpPermissionResponse].

Interactive applications normally present the request title, description, and
options to the user. Headless applications can use
[class@Foundry.AcpPermissionPolicy] directly or through
[class@Foundry.AcpProjectClient]. The project client confines file operations to
the project directory and records changed files on the session.

## Session Updates

Agents stream progress through [method@Foundry.AcpClient.session_update].
Updates are represented by [class@Foundry.AcpSessionUpdate] and include message
chunks, tool calls, terminal events, file reads and writes, patches, progress,
and errors.

Applications should treat session updates as an append-only event stream for
the conversation, while using the session models for stateful views such as
active terminals and changed files. Message chunks may arrive incrementally, so
UIs should merge adjacent chunks into the currently displayed agent response
when appropriate.

## Adding an ACP Agent Plugin

ACP agents are provided by Foundry plugins. A plugin usually contains:

 * a [class@Foundry.AcpProvider] subclass that decides when agents are
   available
 * one or more [class@Foundry.AcpAgent] subclasses
 * optional build, runtime, or subprocess helpers used by the agent
 * a `.plugin` metadata file with `X-Category=acp`
 * a `meson.build` entry gated by `feature-acp`

Register the provider from the plugin entry point:

```c
#include <foundry.h>

#include "plugin-example-acp-provider.h"

FOUNDRY_PLUGIN_DEFINE (_plugin_example_acp_register_types,
                       FOUNDRY_PLUGIN_REGISTER_TYPE (FOUNDRY_TYPE_ACP_PROVIDER,
                                                    PLUGIN_TYPE_EXAMPLE_ACP_PROVIDER))
```

The provider loads inside a [class@Foundry.Context]. It can inspect the project
or build pipeline, create an agent, and call
[method@Foundry.AcpProvider.agent_added] when that agent should appear in the
manager.

```c
static DexFuture *
plugin_example_acp_provider_load (FoundryAcpProvider *provider)
{
  PluginExampleAcpProvider *self = (PluginExampleAcpProvider *)provider;

  if (self->agent == NULL)
    self->agent = g_object_new (PLUGIN_TYPE_EXAMPLE_ACP_AGENT, NULL);

  foundry_acp_provider_agent_added (provider, self->agent);

  return dex_future_new_true ();
}
```

On unload, remove any agents previously added and clear resources:

```c
static DexFuture *
plugin_example_acp_provider_unload (FoundryAcpProvider *provider)
{
  PluginExampleAcpProvider *self = (PluginExampleAcpProvider *)provider;

  if (self->agent != NULL)
    foundry_acp_provider_agent_removed (provider, self->agent);

  return dex_future_new_true ();
}
```

The agent subclass supplies an identifier, display name, capabilities, and the
session-opening implementation. `open_session` normally starts or connects to
the protocol peer, creates a [class@Foundry.AcpConnection], initializes it, and
returns a [class@Foundry.AcpSession].

```c
static char *
plugin_example_acp_agent_dup_id (FoundryAcpAgent *agent)
{
  return g_strdup ("example");
}

static char *
plugin_example_acp_agent_dup_name (FoundryAcpAgent *agent)
{
  return g_strdup ("Example Agent");
}

static DexFuture *
plugin_example_acp_agent_open_session (FoundryAcpAgent      *agent,
                                       FoundryAcpClient     *client,
                                       FoundryBuildPipeline *pipeline)
{
  /* Start the ACP peer, create a FoundryAcpConnection, initialize it,
   * and return a FoundryAcpSession.
   */
  return dex_future_new_reject (FOUNDRY_ACP_ERROR,
                                FOUNDRY_ACP_ERROR_UNSUPPORTED,
                                "Not implemented");
}
```

Plugin metadata should identify the plugin as ACP-related:

```ini
[Plugin]
Name=Example ACP
Description=Provides Example Agent through the Agent Client Protocol
Embedded=_plugin_example_acp_register_types
Module=example-acp
X-Category=acp
```

Add the plugin to `plugins/meson.build` with the same option gate used by the
bundled ACP provider:

```meson
'example-acp': {'options': ['feature-acp']},
```

The bundled `plugins/codex-acp/` provider is a complete example. It exposes a
Codex agent only when `codex` is available in the prepared build pipeline,
wraps the application client with a project client for file and terminal
operations, starts `codex-acp`, and resets the ACP connection if the subprocess
exits unexpectedly.

## Testing

ACP behavior is covered by GLib tests under `testsuite/`:

 * `test-acp-agent`
 * `test-acp-permission-policy`
 * `test-acp-project-client`
 * `test-acp-session-update`

There is also a GTK test tool in `testsuite/tools/test-acp-gtk.c` that shows a
minimal interactive client using [class@Foundry.AcpManager], a custom
[iface@Foundry.AcpClient], and the session list models.
