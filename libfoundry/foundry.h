/* foundry.h
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <glib.h>

G_BEGIN_DECLS

#define FOUNDRY_INSIDE

# include "libfoundry-config.h"

# include "foundry-auth-prompt.h"
# include "foundry-auth-provider.h"
# include "foundry-build-addin.h"
# include "foundry-build-flags.h"
# include "foundry-build-manager.h"
# include "foundry-build-pipeline.h"
# include "foundry-build-progress.h"
# include "foundry-build-stage.h"
# include "foundry-cli-command-tree.h"
# include "foundry-cli-command.h"
# include "foundry-command.h"
# include "foundry-command-stage.h"
# include "foundry-compile-commands.h"
# include "foundry-code-action.h"
# include "foundry-command.h"
# include "foundry-command-line.h"
# include "foundry-command-manager.h"
# include "foundry-command-provider.h"
# include "foundry-config-manager.h"
# include "foundry-config-provider.h"
# include "foundry-config.h"
# include "foundry-context.h"
# include "foundry-contextual.h"
# include "foundry-dbus-service.h"
# include "foundry-debug.h"
# include "foundry-dependency.h"
# include "foundry-dependency-manager.h"
# include "foundry-dependency-provider.h"
# include "foundry-deploy-strategy.h"
# include "foundry-device.h"
# include "foundry-device-chassis.h"
# include "foundry-device-info.h"
# include "foundry-device-manager.h"
# include "foundry-device-provider.h"
# include "foundry-diagnostic-builder.h"
# include "foundry-diagnostic-manager.h"
# include "foundry-diagnostic-provider.h"
# include "foundry-diagnostic-range.h"
# include "foundry-diagnostic-tool.h"
# include "foundry-diagnostic.h"
# include "foundry-directory-item.h"
# include "foundry-directory-listing.h"
# include "foundry-directory-reaper.h"
# include "foundry-extension.h"
# include "foundry-extension-set.h"
# include "foundry-file.h"
# include "foundry-file-manager.h"
# include "foundry-file-monitor.h"
# include "foundry-file-monitor-event.h"
# include "foundry-inhibitor.h"
# include "foundry-init.h"
# include "foundry-language-guesser.h"
# include "foundry-linked-pipeline-stage.h"
# include "foundry-local-device.h"
# include "foundry-markup.h"
# include "foundry-model-manager.h"
# include "foundry-operation.h"
# include "foundry-operation-manager.h"
# include "foundry-os-info.h"
# include "foundry-plugin-manager.h"
# include "foundry-json.h"
# include "foundry-json-input-stream.h"
# include "foundry-json-node.h"
# include "foundry-json-output-stream.h"
# include "foundry-path.h"
# include "foundry-path-cache.h"
# include "foundry-plugin.h"
# include "foundry-plugin-build-addin.h"
# include "foundry-process-launcher.h"
# include "foundry-run-manager.h"
# include "foundry-run-tool.h"
# include "foundry-sdk-manager.h"
# include "foundry-sdk-provider.h"
# include "foundry-sdk.h"
# include "foundry-search-manager.h"
# include "foundry-search-path.h"
# include "foundry-search-provider.h"
# include "foundry-search-request.h"
# include "foundry-search-result.h"
# include "foundry-settings.h"
# include "foundry-shell.h"
# include "foundry-subprocess.h"
# include "foundry-symbol-provider.h"
# include "foundry-symbol.h"
# include "foundry-test-manager.h"
# include "foundry-test-provider.h"
# include "foundry-test.h"
# include "foundry-triplet.h"
# include "foundry-unix-fd-map.h"
# include "foundry-util.h"
# include "foundry-version.h"

#ifdef FOUNDRY_FEATURE_DAP
# include "foundry-dap-client.h"
# include "foundry-dap-debugger.h"
# include "foundry-dap-event.h"
# include "foundry-dap-initialize-request.h"
# include "foundry-dap-request.h"
# include "foundry-dap-response.h"
#endif

#ifdef FOUNDRY_FEATURE_DEBUGGER
# include "foundry-debugger-breakpoint.h"
# include "foundry-debugger-countpoint.h"
# include "foundry-debugger-event.h"
# include "foundry-debugger-instruction.h"
# include "foundry-debugger-manager.h"
# include "foundry-debugger-mapped-region.h"
# include "foundry-debugger-module.h"
# include "foundry-debugger-provider.h"
# include "foundry-debugger-source.h"
# include "foundry-debugger-target-command.h"
# include "foundry-debugger-target-process.h"
# include "foundry-debugger-target-remote.h"
# include "foundry-debugger-target.h"
# include "foundry-debugger-thread.h"
# include "foundry-debugger-thread-group.h"
# include "foundry-debugger-trap.h"
# include "foundry-debugger-trap-params.h"
# include "foundry-debugger-variable.h"
# include "foundry-debugger-watchpoint.h"
# include "foundry-debugger.h"
#endif

#ifdef FOUNDRY_FEATURE_DOCS
# include "foundry-documentation.h"
# include "foundry-documentation-bundle.h"
# include "foundry-documentation-manager.h"
# include "foundry-documentation-matches.h"
# include "foundry-documentation-provider.h"
# include "foundry-documentation-query.h"
# include "foundry-documentation-root.h"
#endif

#ifdef FOUNDRY_FEATURE_FLATPAK
# include "foundry-flatpak.h"
#endif

#ifdef FOUNDRY_FEATURE_GIT
# include "foundry-git.h"
#endif

#ifdef FOUNDRY_FEATURE_LLM
# include "foundry-llm.h"
#endif

#ifdef FOUNDRY_FEATURE_LSP
# include "foundry-lsp.h"
#endif

#ifdef FOUNDRY_FEATURE_MCP
# include "foundry-mcp.h"
#endif

#ifdef FOUNDRY_FEATURE_TEXT
# include "foundry-completion-proposal.h"
# include "foundry-completion-provider.h"
# include "foundry-completion-request.h"
# include "foundry-hover-provider.h"
# include "foundry-on-type-formatter.h"
# include "foundry-rename-provider.h"
# include "foundry-simple-text-buffer.h"
# include "foundry-text-buffer.h"
# include "foundry-text-buffer-provider.h"
# include "foundry-text-document.h"
# include "foundry-text-iter.h"
# include "foundry-text-edit.h"
# include "foundry-text-formatter.h"
# include "foundry-text-manager.h"
#endif

#ifdef FOUNDRY_FEATURE_TERMINAL
# include "foundry-terminal-launcher.h"
#endif

#ifdef FOUNDRY_FEATURE_VCS
# include "foundry-no-vcs.h"
# include "foundry-vcs.h"
# include "foundry-vcs-blame.h"
# include "foundry-vcs-branch.h"
# include "foundry-vcs-commit.h"
# include "foundry-vcs-file.h"
# include "foundry-vcs-provider.h"
# include "foundry-vcs-manager.h"
# include "foundry-vcs-reference.h"
# include "foundry-vcs-remote.h"
# include "foundry-vcs-signature.h"
# include "foundry-vcs-tag.h"
#endif

#undef FOUNDRY_INSIDE

G_END_DECLS
