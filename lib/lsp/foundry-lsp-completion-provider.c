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

#include <jsonrpc-glib.h>

#include "foundry-completion-request.h"
#include "foundry-lsp-client.h"
#include "foundry-lsp-completion-provider.h"
#include "foundry-lsp-completion-results.h"
#include "foundry-lsp-manager.h"
#include "foundry-text-iter.h"
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

static DexFuture *
foundry_lsp_completion_provider_complete_fiber (FoundryLspCompletionProvider *self,
                                                FoundryCompletionRequest     *request)
{
  g_autoptr(FoundryLspClient) client = NULL;
  g_autofree char *language_id = NULL;

  g_assert (FOUNDRY_IS_LSP_COMPLETION_PROVIDER (self));
  g_assert (FOUNDRY_IS_COMPLETION_REQUEST (request));

  if ((language_id = foundry_completion_request_dup_language_id (request)))
    {
      g_autoptr(GVariant) params = NULL;
      g_autoptr(GVariant) reply = NULL;
      g_autoptr(GError) error = NULL;
      g_autoptr(GFile) file = NULL;
      g_autofree char *uri = NULL;
      FoundryCompletionActivation activation;
      FoundryTextIter begin;
      FoundryTextIter end;
      int trigger_kind;
      int line;
      int line_offset;

      if (!(file = foundry_completion_request_dup_file (request)))
        return foundry_future_new_disposed ();

      if (!(client = dex_await_object (foundry_lsp_completion_provider_load_client (self, language_id), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      g_assert (language_id != NULL);
      g_assert (FOUNDRY_IS_LSP_CLIENT (client));

      foundry_completion_request_get_bounds (request, &begin, &end);

      activation = foundry_completion_request_get_activation (request);

      if (activation == FOUNDRY_COMPLETION_ACTIVATION_INTERACTIVE)
        trigger_kind = 2;
      else
        trigger_kind = 1;

      line = foundry_text_iter_get_line (&begin);
      line_offset = foundry_text_iter_get_line_offset (&begin);
      uri = g_file_get_uri (file);

      params = JSONRPC_MESSAGE_NEW (
        "textDocument", "{",
          "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
        "}",
        "position", "{",
          "line", JSONRPC_MESSAGE_PUT_INT32 (line),
          "character", JSONRPC_MESSAGE_PUT_INT32 (line_offset),
        "}",
        "context", "{",
          "triggerKind", JSONRPC_MESSAGE_PUT_INT32 (trigger_kind),
        "}"
      );

      if (!(reply = dex_await_variant (foundry_lsp_client_call (client, "textDocument/completion", params), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      return foundry_lsp_completion_results_new (client, reply);
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not supported");
}

static DexFuture *
foundry_lsp_completion_provider_complete (FoundryCompletionProvider *provider,
                                          FoundryCompletionRequest  *request)
{
  g_assert (FOUNDRY_IS_LSP_COMPLETION_PROVIDER (provider));
  g_assert (FOUNDRY_IS_COMPLETION_REQUEST (request));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (foundry_lsp_completion_provider_complete_fiber),
                                  2,
                                  FOUNDRY_TYPE_LSP_COMPLETION_PROVIDER, provider,
                                  FOUNDRY_TYPE_COMPLETION_REQUEST, request);
}

static void
foundry_lsp_completion_provider_class_init (FoundryLspCompletionProviderClass *klass)
{
  FoundryCompletionProviderClass *completion_provider_class = FOUNDRY_COMPLETION_PROVIDER_CLASS (klass);

  completion_provider_class->complete = foundry_lsp_completion_provider_complete;

}

static void
foundry_lsp_completion_provider_init (FoundryLspCompletionProvider *self)
{
}
