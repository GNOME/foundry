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
# include "foundry-build-manager.h"
# include "foundry-build-pipeline.h"
# include "foundry-build-progress.h"
# include "foundry-cli-command-tree.h"
# include "foundry-cli-command.h"
# include "foundry-code-action.h"
# include "foundry-code-action-provider.h"
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
# include "foundry-debug-manager.h"
# include "foundry-debug.h"
# include "foundry-debugger.h"
# include "foundry-device-manager.h"
# include "foundry-device-provider.h"
# include "foundry-device.h"
# include "foundry-diagnostic-builder.h"
# include "foundry-diagnostic-manager.h"
# include "foundry-diagnostic-provider.h"
# include "foundry-diagnostic.h"
# include "foundry-directory-reaper.h"
# include "foundry-extension-set.h"
# include "foundry-file.h"
# include "foundry-file-manager.h"
# include "foundry-future-list-model.h"
# include "foundry-init.h"
# include "foundry-local-device.h"
# include "foundry-markup.h"
# include "foundry-operation.h"
# include "foundry-operation-manager.h"
# include "foundry-json.h"
# include "foundry-path.h"
# include "foundry-plugin.h"
# include "foundry-process-launcher.h"
# include "foundry-sdk-manager.h"
# include "foundry-sdk-provider.h"
# include "foundry-sdk.h"
# include "foundry-search-manager.h"
# include "foundry-search-provider.h"
# include "foundry-settings.h"
# include "foundry-shell.h"
# include "foundry-subprocess.h"
# include "foundry-symbol-provider.h"
# include "foundry-symbol.h"
# include "foundry-text-buffer.h"
# include "foundry-text-document.h"
# include "foundry-text-edit.h"
# include "foundry-text-manager.h"
# include "foundry-triplet.h"
# include "foundry-unix-fd-map.h"
# include "foundry-util.h"
# include "foundry-version.h"
#undef FOUNDRY_INSIDE

G_END_DECLS
