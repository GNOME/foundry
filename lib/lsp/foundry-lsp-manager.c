/* foundry-lsp-manager.c
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

#include "config.h"

#include <glib/gstdio.h>

#include <libpeas.h>

#include "foundry-build-manager.h"
#include "foundry-build-pipeline.h"
#include "foundry-context.h"
#include "foundry-contextual-private.h"
#include "foundry-debug.h"
#include "foundry-model-manager.h"
#include "foundry-lsp-client.h"
#include "foundry-lsp-manager-private.h"
#include "foundry-lsp-provider-private.h"
#include "foundry-lsp-server.h"
#include "foundry-process-launcher.h"
#include "foundry-service-private.h"
#include "foundry-settings.h"
#include "foundry-util-private.h"

struct _FoundryLspManager
{
  FoundryService    parent_instance;
  PeasExtensionSet *addins;
  GListModel       *flatten;
  GPtrArray        *clients;
  GHashTable       *loading;
};

struct _FoundryLspManagerClass
{
  FoundryServiceClass parent_class;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryLspManager, foundry_lsp_manager, FOUNDRY_TYPE_SERVICE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
foundry_lsp_manager_provider_added (PeasExtensionSet *set,
                                    PeasPluginInfo   *plugin_info,
                                    GObject          *addin,
                                    gpointer          user_data)
{
  FoundryLspManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_LSP_PROVIDER (addin));
  g_assert (FOUNDRY_IS_LSP_MANAGER (self));

  g_debug ("Adding FoundryLspProvider of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_lsp_provider_load (FOUNDRY_LSP_PROVIDER (addin)));
}

static void
foundry_lsp_manager_provider_removed (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      GObject          *addin,
                                      gpointer          user_data)
{
  FoundryLspManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_LSP_PROVIDER (addin));
  g_assert (FOUNDRY_IS_LSP_MANAGER (self));

  g_debug ("Removing FoundryLspProvider of type %s", G_OBJECT_TYPE_NAME (addin));

  dex_future_disown (foundry_lsp_provider_unload (FOUNDRY_LSP_PROVIDER (addin)));
}

static DexFuture *
foundry_lsp_manager_start (FoundryService *service)
{
  FoundryLspManager *self = (FoundryLspManager *)service;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_SERVICE (service));
  g_assert (PEAS_IS_EXTENSION_SET (self->addins));

  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (foundry_lsp_manager_provider_added),
                           self,
                           0);
  g_signal_connect_object (self->addins,
                           "extension-removed",
                           G_CALLBACK (foundry_lsp_manager_provider_removed),
                           self,
                           0);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryLspProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures,
                       foundry_lsp_provider_load (provider));
    }

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static DexFuture *
foundry_lsp_manager_stop (FoundryService *service)
{
  FoundryLspManager *self = (FoundryLspManager *)service;
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_SERVICE (service));

  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_lsp_manager_provider_added),
                                        self);
  g_signal_handlers_disconnect_by_func (self->addins,
                                        G_CALLBACK (foundry_lsp_manager_provider_removed),
                                        self);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryLspProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures,
                       foundry_lsp_provider_unload (provider));
    }

  g_clear_object (&self->addins);

  if (futures->len > 0)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static void
foundry_lsp_manager_constructed (GObject *object)
{
  FoundryLspManager *self = (FoundryLspManager *)object;
  g_autoptr(FoundryContext) context = NULL;

  G_OBJECT_CLASS (foundry_lsp_manager_parent_class)->constructed (object);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));

  self->addins = peas_extension_set_new (NULL,
                                         FOUNDRY_TYPE_LSP_PROVIDER,
                                         "context", context,
                                         NULL);

  g_object_set (self->flatten,
                "model", self->addins,
                NULL);
}

static void
foundry_lsp_manager_finalize (GObject *object)
{
  FoundryLspManager *self = (FoundryLspManager *)object;

  g_clear_pointer (&self->clients, g_ptr_array_unref);
  g_clear_pointer (&self->loading, g_hash_table_unref);
  g_clear_object (&self->flatten);
  g_clear_object (&self->addins);

  G_OBJECT_CLASS (foundry_lsp_manager_parent_class)->finalize (object);
}

static void
foundry_lsp_manager_class_init (FoundryLspManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  object_class->constructed = foundry_lsp_manager_constructed;
  object_class->finalize = foundry_lsp_manager_finalize;

  service_class->start = foundry_lsp_manager_start;
  service_class->stop = foundry_lsp_manager_stop;
}

static void
foundry_lsp_manager_init (FoundryLspManager *self)
{
  self->clients = g_ptr_array_new_with_free_func (g_object_unref);
  self->loading = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, dex_unref);
  self->flatten = foundry_flatten_list_model_new (NULL);

  g_signal_connect_object (self->flatten,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

typedef struct _LoadClient
{
  FoundryLspManager  *self;
  FoundryLspServer   *server;
  char              **languages;
  int                 stdin_fd;
  int                 stdout_fd;
} LoadClient;

static void
load_client_free (LoadClient *state)
{
  for (guint i = 0; state->languages[i]; i++)
    g_hash_table_remove (state->self->loading, state->languages[i]);

  g_clear_object (&state->self);
  g_clear_object (&state->server);
  g_clear_pointer (&state->languages, g_strfreev);
  g_clear_fd (&state->stdin_fd, NULL);
  g_clear_fd (&state->stdout_fd, NULL);
  g_free (state);
}

static DexFuture *
foundry_lsp_manager_load_client_fiber (gpointer data)
{
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(FoundryBuildManager) build_manager = NULL;
  g_autoptr(FoundryLspClient) client = NULL;
  g_autoptr(FoundrySettings) settings = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GIOStream) io_stream = NULL;
  g_autoptr(GError) error = NULL;
  GSubprocessFlags flags = 0;
  LoadClient *state = data;
  gboolean log_stderr;

  g_assert (FOUNDRY_IS_LSP_MANAGER (state->self));
  g_assert (FOUNDRY_IS_LSP_SERVER (state->server));
  g_assert (state->languages != NULL);

  if (!(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (state->self))))
    return foundry_future_new_disposed ();

  build_manager = foundry_context_dup_build_manager (context);
  pipeline = dex_await_object (foundry_build_manager_load_pipeline (build_manager), NULL);

  settings = foundry_context_load_settings (context, "app.devsuite.foundry.lsp", NULL);
  log_stderr = foundry_settings_get_boolean (settings, "log-stderr");

  if (!log_stderr)
    flags |= G_SUBPROCESS_FLAGS_STDERR_SILENCE;

  launcher = foundry_process_launcher_new ();

  if (!dex_await (foundry_lsp_server_prepare (state->server, pipeline, launcher), &error) ||
      !(io_stream = foundry_process_launcher_create_stdio_stream (launcher, &error)) ||
      !(subprocess = foundry_process_launcher_spawn_with_flags (launcher, flags, &error)) ||
      !(client = dex_await_object (foundry_lsp_client_new (context, io_stream, subprocess), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  g_ptr_array_add (state->self->clients, g_object_ref (client));

  return dex_future_new_take_object (g_steal_pointer (&client));
}

/**
 * foundry_lsp_manager_load_client:
 * @self: a #FoundryLspManager
 *
 * Loads a [class@Foundry.LspClient] for the @language_id.
 *
 * If an existing client is already created for this language,
 * that client will be returned.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.LspClient].
 */
DexFuture *
foundry_lsp_manager_load_client (FoundryLspManager *self,
                                 const char        *language_id)
{
  DexFuture *loading;
  guint n_items;

  dex_return_error_if_fail (FOUNDRY_IS_LSP_MANAGER (self));
  dex_return_error_if_fail (language_id != NULL);

  for (guint i = 0; i < self->clients->len; i++)
    {
      FoundryLspClient *client = g_ptr_array_index (self->clients, i);

      if (foundry_lsp_client_supports_language (client, language_id))
        return dex_future_new_take_object (g_object_ref (client));
    }

  if ((loading = g_hash_table_lookup (self->loading, language_id)))
    return dex_ref (loading);

  /* Find the server which supports this language and insert a future
   * for completion for each language of the hashtable. That way we can
   * try to coalesce multiple requests into one.
   */
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryLspServer) server = g_list_model_get_item (G_LIST_MODEL (self), i);
      g_auto(GStrv) languages = foundry_lsp_server_dup_languages (server);

      if (languages &&
          g_strv_contains ((const char * const *)languages, language_id))
        {
          LoadClient *state;

          state = g_new0 (LoadClient, 1);
          state->self = g_object_ref (self);
          state->server = g_object_ref (server);
          state->languages = g_strdupv (languages);
          state->stdin_fd = -1;
          state->stdout_fd = -1;

          loading = dex_scheduler_spawn (NULL, 0,
                                         foundry_lsp_manager_load_client_fiber,
                                         state,
                                         (GDestroyNotify) load_client_free);

          for (guint j = 0; languages[j]; j++)
            g_hash_table_insert (self->loading, g_strdup (languages[j]), dex_ref (loading));

          return loading;
        }
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "not supported");
}

static GType
foundry_lsp_manager_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_LSP_SERVER;
}

static guint
foundry_lsp_manager_get_n_items (GListModel *model)
{
  FoundryLspManager *self = FOUNDRY_LSP_MANAGER (model);

  return g_list_model_get_n_items (G_LIST_MODEL (self->flatten));
}

static gpointer
foundry_lsp_manager_get_item (GListModel *model,
                              guint       position)
{
  FoundryLspManager *self = FOUNDRY_LSP_MANAGER (model);

  return g_list_model_get_item (G_LIST_MODEL (self->flatten), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_lsp_manager_get_item_type;
  iface->get_n_items = foundry_lsp_manager_get_n_items;
  iface->get_item = foundry_lsp_manager_get_item;
}

DexFuture *
_foundry_lsp_manager_load_client_for_plugin (FoundryLspManager *self,
                                             PeasPluginInfo    *plugin_info)
{
  dex_return_error_if_fail (FOUNDRY_IS_LSP_MANAGER (self));
  dex_return_error_if_fail (PEAS_IS_PLUGIN_INFO (plugin_info));

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not yet supported");
}

/**
 * foundry_lsp_manager_load_language_settings:
 * @self: a [class@Foundry.LspManager]
 * @language_id: the language identifier
 *
 * Returns: (transfer full): a [class@Foundry.Settings]
 */
FoundrySettings *
foundry_lsp_manager_load_language_settings (FoundryLspManager *self,
                                            const char        *language_id)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autofree char *path = NULL;

  g_return_val_if_fail (FOUNDRY_IS_LSP_MANAGER (self), NULL);
  g_return_val_if_fail (language_id != NULL, NULL);
  g_return_val_if_fail (strchr (language_id, '/') == NULL, NULL);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  path = g_strdup_printf ("/app/devsuite/Foundry/lsp/language/%s/", language_id);

  return foundry_context_load_settings (context, "app.devsuite.Foundry.Lsp.Language", path);
}
