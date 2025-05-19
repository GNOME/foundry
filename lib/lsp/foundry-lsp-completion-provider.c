/* foundry-lsp-completion-provider.c
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

#include "foundry-lsp-client.h"
#include "foundry-lsp-completion-provider.h"
#include "foundry-lsp-manager.h"
#include "foundry-util.h"

struct _FoundryLspCompletionProvider
{
  FoundryCompletionProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (FoundryLspCompletionProvider, foundry_lsp_completion_provider, FOUNDRY_TYPE_COMPLETION_PROVIDER)

static DexFuture *
foundry_lsp_completion_provider_load_client (FoundryLspCompletionProvider *self,
                                             const char                   *language_id)
{
  g_autoptr(FoundryLspManager) lsp_manager = NULL;
  g_autoptr(FoundryContext) context = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_LSP_COMPLETION_PROVIDER (self));
  dex_return_error_if_fail (language_id != NULL);

  if ((context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))) &&
      (lsp_manager = foundry_context_dup_lsp_manager (context)))
    return foundry_lsp_manager_load_client (lsp_manager, language_id);

  return foundry_future_new_disposed ();
}

static void
foundry_lsp_completion_provider_class_init (FoundryLspCompletionProviderClass *klass)
{
  FoundryCompletionProviderClass *completion_provider_class = FOUNDRY_COMPLETION_PROVIDER_CLASS (klass);

}

static void
foundry_lsp_completion_provider_init (FoundryLspCompletionProvider *self)
{
}
