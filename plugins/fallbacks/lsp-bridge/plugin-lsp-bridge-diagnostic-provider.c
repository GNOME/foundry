/* plugin-lsp-bridge-diagnostic-provider.c
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

#include "foundry-lsp-client-private.h"

#include "plugin-lsp-bridge-diagnostic-provider.h"

struct _PluginLspBridgeDiagnosticProvider
{
  FoundryDiagnosticProvider parent_instance;
  guint has_textDocument_diagnostic : 1;
  guint checked_textDocument_diagnostic : 1;
};

G_DEFINE_FINAL_TYPE (PluginLspBridgeDiagnosticProvider, plugin_lsp_bridge_diagnostic_provider, FOUNDRY_TYPE_DIAGNOSTIC_PROVIDER)

static DexFuture *
plugin_lsp_bridge_diagnostic_provider_diagnose_fiber (PluginLspBridgeDiagnosticProvider *self,
                                                      GFile                             *file,
                                                      GBytes                            *contents,
                                                      const char                        *language)
{
  g_autoptr(FoundryTextDocument) document = NULL;
  g_autoptr(FoundryTextManager) text_manager = NULL;
  g_autoptr(FoundryLspManager) lsp_manager = NULL;
  g_autoptr(FoundryLspClient) client = NULL;
  g_autoptr(FoundryOperation) operation = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(JsonNode) params = NULL;
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *uri = NULL;
  GListModel *model;

  g_assert (PLUGIN_IS_LSP_BRIDGE_DIAGNOSTIC_PROVIDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (language != NULL);

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  lsp_manager = foundry_context_dup_lsp_manager (context);
  text_manager = foundry_context_dup_text_manager (context);

  if (!(client = dex_await_object (foundry_lsp_manager_load_client (lsp_manager, language), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  uri = g_file_get_uri (file);
  store = g_list_store_new (G_TYPE_LIST_MODEL);

  /* The first thing we need to do is make sure the client knows about the
   * document and its contents. The easiest way to do this is to just open
   * the document with the text manager so the client will synchronize it
   * to the LSP worker.
   */
  operation = foundry_operation_new ();
  if (!(document = dex_await_object (foundry_text_manager_load (text_manager, file, operation, NULL), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  params = FOUNDRY_JSON_OBJECT_NEW (
    "textDocument", "{",
      "uri", FOUNDRY_JSON_NODE_PUT_STRING (uri),
    "}"
  );

  /* In LSP 3.17 an option was added to query diagnostics specifically instead of
   * waiting for the peer to publish them periodically. This fits much better into
   * our design of diagnostics though may not be supported by all LSP servers.
   */
  if ((!self->checked_textDocument_diagnostic ||
       self->has_textDocument_diagnostic) &&
      (reply = dex_await_boxed (foundry_lsp_client_call (client, "textDocument/diagnostic", params), &error)))
    {
      self->has_textDocument_diagnostic = TRUE;

      /* TODO: Make list from @reply */
    }
  else
    {
      self->checked_textDocument_diagnostic = TRUE;

      /* Delay just a bit to see if we get diagnostics in as a result
       * of opening the document and synchronizing contents.
       */
      dex_await (dex_timeout_new_msec (50), NULL);
    }

  if ((model = _foundry_lsp_client_get_diagnostics (client, file)))
    g_list_store_append (store, model);

  return dex_future_new_take_object (foundry_flatten_list_model_new (g_object_ref (G_LIST_MODEL (store))));
}

static DexFuture *
plugin_lsp_bridge_diagnostic_provider_diagnose (FoundryDiagnosticProvider *provider,
                                                GFile                     *file,
                                                GBytes                    *contents,
                                                const char                *language)
{
  g_assert (PLUGIN_IS_LSP_BRIDGE_DIAGNOSTIC_PROVIDER (provider));
  g_assert (G_IS_FILE (file) || contents != NULL);

  if (language == NULL || file == NULL)
    return foundry_future_new_not_supported ();

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_lsp_bridge_diagnostic_provider_diagnose_fiber),
                                  4,
                                  PLUGIN_TYPE_LSP_BRIDGE_DIAGNOSTIC_PROVIDER, provider,
                                  G_TYPE_FILE, file,
                                  G_TYPE_BYTES, contents,
                                  G_TYPE_STRING, language);
}

static void
plugin_lsp_bridge_diagnostic_provider_class_init (PluginLspBridgeDiagnosticProviderClass *klass)
{
  FoundryDiagnosticProviderClass *diagnostic_provider_class = FOUNDRY_DIAGNOSTIC_PROVIDER_CLASS (klass);

  diagnostic_provider_class->diagnose = plugin_lsp_bridge_diagnostic_provider_diagnose;
}

static void
plugin_lsp_bridge_diagnostic_provider_init (PluginLspBridgeDiagnosticProvider *self)
{
}
