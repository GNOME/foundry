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
#include "foundry-lsp-manager-private.h"

struct _FoundryLspCompletionProvider
{
  FoundryCompletionProvider parent_instance;
  FoundryLspClient *client;
};

G_DEFINE_FINAL_TYPE (FoundryLspCompletionProvider, foundry_lsp_completion_provider, G_TYPE_OBJECT)

static DexFuture *
foundry_lsp_completion_provider_load_fiber (gpointer user_data)
{
  FoundryLspCompletionProvider *self = user_data;
  g_autoptr(FoundryLspManager) lsp_manager = NULL;
  g_autoptr(FoundryLspClient) client = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GError) error = NULL;
  PeasPluginInfo *plugin_info;

  g_assert (FOUNDRY_IS_LSP_COMPLETION_PROVIDER (self));

  if (!(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))) ||
      !(plugin_info = foundry_completion_provider_get_plugin_info (FOUNDRY_COMPLETION_PROVIDER (self))) ||
      !(lsp_manager = foundry_context_dup_lsp_manager (context)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "No plugin registered");

  if (!(client = dex_await_object (_foundry_lsp_manager_load_client_for_plugin (lsp_manager, plugin_info), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
foundry_lsp_completion_provider_load (FoundryCompletionProvider *provider)
{
  return dex_scheduler_spawn (NULL, 0,
                              foundry_lsp_completion_provider_load_fiber,
                              g_object_ref (provider),
                              g_object_unref);
}

static DexFuture *
foundry_lsp_completion_provider_unload (FoundryCompletionProvider *provider)
{
  FoundryLspCompletionProvider *self = (FoundryLspCompletionProvider *)provider;

  g_assert (FOUNDRY_IS_LSP_COMPLETION_PROVIDER (self));

  g_clear_object (&self->client);

  return dex_future_new_true ();
}

static void
foundry_lsp_completion_provider_finalize (GObject *object)
{
  FoundryLspCompletionProvider *self = (FoundryLspCompletionProvider *)object;

  g_clear_object (&self->client);

  G_OBJECT_CLASS (foundry_lsp_completion_provider_parent_class)->finalize (object);
}

static void
foundry_lsp_completion_provider_class_init (FoundryLspCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryCompletionProviderClass *completion_provider_class = FOUNDRY_COMPLETION_PROVIDER_CLASS (klass);

  object_class->finalize = foundry_lsp_completion_provider_finalize;

  completion_provider_class->load = foundry_lsp_completion_provider_load;
  completion_provider_class->unload = foundry_lsp_completion_provider_unload;
}

static void
foundry_lsp_completion_provider_init (FoundryLspCompletionProvider *self)
{
}
