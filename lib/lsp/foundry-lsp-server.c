/* foundry-lsp-server.c
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

#include "foundry-build-pipeline.h"
#include "foundry-lsp-server.h"

G_DEFINE_ABSTRACT_TYPE (FoundryLspServer, foundry_lsp_server, FOUNDRY_TYPE_CONTEXTUAL)

static void
foundry_lsp_server_class_init (FoundryLspServerClass *klass)
{
}

static void
foundry_lsp_server_init (FoundryLspServer *self)
{
}

/**
 * foundry_lsp_server_spawn:
 * @self: a #FoundryLspServer
 * @pipeline: (nullable): a [class@Foundry.BuildPipeline] or %NULL
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.LspClient] or rejects with error
 */
DexFuture *
foundry_lsp_server_spawn (FoundryLspServer     *self,
                          FoundryBuildPipeline *pipeline)
{
  g_return_val_if_fail (FOUNDRY_IS_LSP_SERVER (self), NULL);
  g_return_val_if_fail (!pipeline || FOUNDRY_IS_BUILD_PIPELINE (pipeline), NULL);

  return FOUNDRY_LSP_SERVER_GET_CLASS (self)->spawn (self, pipeline);
}
