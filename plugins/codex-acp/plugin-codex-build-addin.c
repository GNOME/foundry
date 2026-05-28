/*
 * plugin-codex-build-addin.c
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

#include "foundry-acp-connection-private.h"

#include "plugin-codex-build-addin.h"

struct _PluginCodexBuildAddin
{
  FoundryBuildAddin parent_instance;
  FoundryAcpConnection *connection;
  GSubprocess *subprocess;
};

G_DEFINE_TYPE (PluginCodexBuildAddin, plugin_codex_build_addin, FOUNDRY_TYPE_BUILD_ADDIN)

static GQuark codex_build_addin_quark;

static GQuark
plugin_codex_build_addin_quark (void)
{
  if (codex_build_addin_quark == 0)
    codex_build_addin_quark = g_quark_from_static_string ("plugin-codex-build-addin");
  return codex_build_addin_quark;
}

PluginCodexBuildAddin *
plugin_codex_build_addin_for_pipeline (FoundryBuildPipeline *pipeline)
{
  PluginCodexBuildAddin *addin = NULL;

  g_return_val_if_fail (FOUNDRY_IS_BUILD_PIPELINE (pipeline), NULL);

  addin = g_object_get_qdata (G_OBJECT (pipeline), plugin_codex_build_addin_quark ());
  if (addin == NULL)
    {
      addin = g_object_new (PLUGIN_TYPE_CODEX_BUILD_ADDIN, NULL);
      g_object_set_qdata_full (G_OBJECT (pipeline),
                               plugin_codex_build_addin_quark (),
                               addin,
                               g_object_unref);
    }

  return g_object_ref (addin);
}

static void
plugin_codex_build_addin_clear_connection (PluginCodexBuildAddin *self,
                                           gboolean               force_subprocess)
{
  g_assert (PLUGIN_IS_CODEX_BUILD_ADDIN (self));

  if (self->connection != NULL)
    {
      dex_future_disown (foundry_acp_connection_close (self->connection));
      g_clear_object (&self->connection);
    }

  if (self->subprocess != NULL)
    {
      if (force_subprocess)
        g_subprocess_force_exit (self->subprocess);

      g_clear_object (&self->subprocess);
    }
}

static DexFuture *
plugin_codex_build_addin_subprocess_exited_cb (DexFuture *completed,
                                               gpointer   user_data)
{
  FoundryWeakPair *pair = user_data;
  g_autoptr(PluginCodexBuildAddin) self = NULL;
  g_autoptr(FoundryAcpConnection) connection = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *message = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (pair != NULL);

  if (!foundry_weak_pair_get (pair, &self, &subprocess))
    return dex_future_new_true ();

  if (self->subprocess != subprocess)
    return dex_future_new_true ();

  if (self->connection != NULL &&
      foundry_acp_connection_get_state (self->connection) != FOUNDRY_ACP_CONNECTION_CLOSING &&
      foundry_acp_connection_get_state (self->connection) != FOUNDRY_ACP_CONNECTION_CLOSED)
    {
      connection = g_object_ref (self->connection);

      if (!dex_future_get_value (completed, &error))
        message = g_strdup_printf ("Codex ACP subprocess exited unexpectedly: %s", error->message);
      else
        message = g_strdup ("Codex ACP subprocess exited unexpectedly");

      _foundry_acp_connection_fail (connection, message);
      g_clear_object (&self->connection);
      g_clear_object (&self->subprocess);

      return dex_future_new_true ();
    }

  plugin_codex_build_addin_clear_connection (self, FALSE);

  return dex_future_new_true ();
}

static gboolean
plugin_codex_build_addin_set_api_key (FoundryProcessLauncher  *launcher,
                                      FoundryContext          *context,
                                      GError                 **error)
{
  g_autoptr(FoundrySecretService) secret_service = NULL;
  g_autoptr(GError) local_error = NULL;
  g_autofree char *api_key = NULL;

  g_assert (FOUNDRY_IS_PROCESS_LAUNCHER (launcher));
  g_assert (FOUNDRY_IS_CONTEXT (context));

  secret_service = foundry_context_dup_secret_service (context);

  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (secret_service)), &local_error))
    {
      g_propagate_error (error, g_steal_pointer (&local_error));
      return FALSE;
    }

  api_key = dex_await_string (foundry_secret_service_lookup_api_key (secret_service,
                                                                     "api.openai.com",
                                                                     "openai"),
                              &local_error);

  if (local_error != NULL)
    {
      g_propagate_error (error, g_steal_pointer (&local_error));
      return FALSE;
    }

  if (!foundry_str_empty0 (api_key))
    foundry_process_launcher_setenv (launcher, "OPENAI_API_KEY", api_key);

  return TRUE;
}

FoundryAcpConnection *
plugin_codex_build_addin_get_connection (PluginCodexBuildAddin *self,
                                         FoundryBuildPipeline  *pipeline,
                                         FoundryAcpAgent       *agent,
                                         FoundryAcpClient      *client,
                                         GError              **error)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GIOStream) stream = NULL;
  g_autoptr(GFile) project_directory = NULL;
  g_autofree char *cwd = NULL;
  GSubprocessFlags subprocess_flags = G_SUBPROCESS_FLAGS_STDERR_SILENCE;

  g_return_val_if_fail (PLUGIN_IS_CODEX_BUILD_ADDIN (self), NULL);
  g_return_val_if_fail (FOUNDRY_IS_BUILD_PIPELINE (pipeline), NULL);
  g_return_val_if_fail (FOUNDRY_IS_ACP_AGENT (agent), NULL);
  g_return_val_if_fail (FOUNDRY_IS_ACP_CLIENT (client), NULL);

  if (self->connection != NULL)
    return g_object_ref (self->connection);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (pipeline));

  launcher = foundry_process_launcher_new ();

  if ((project_directory = foundry_context_dup_project_directory (context)))
    {
      cwd = g_file_get_path (project_directory);

      if (cwd != NULL)
        foundry_process_launcher_set_cwd (launcher, cwd);
    }

  if (!dex_await (foundry_build_pipeline_prepare_for_run (pipeline, launcher), error))
    return NULL;

  if (!plugin_codex_build_addin_set_api_key (launcher, context, error))
    return NULL;

  foundry_process_launcher_append_argv (launcher, "codex-acp");

  if (!(stream = foundry_process_launcher_create_stdio_stream (launcher, error)))
    return NULL;

  if (!(subprocess = foundry_process_launcher_spawn_with_flags (launcher,
                                                                subprocess_flags,
                                                                error)))
    return NULL;

  self->connection = _foundry_acp_connection_new_for_stream (context, stream, client);
  if (self->connection == NULL)
    {
      g_set_error (error,
                   FOUNDRY_ACP_ERROR,
                   FOUNDRY_ACP_ERROR_INVALID_REQUEST,
                   "Failed to create ACP connection");
      return NULL;
    }

  self->subprocess = g_steal_pointer (&subprocess);

  dex_future_disown (dex_future_finally (dex_subprocess_wait_check (self->subprocess),
                                         plugin_codex_build_addin_subprocess_exited_cb,
                                         foundry_weak_pair_new (self, self->subprocess),
                                         (GDestroyNotify)foundry_weak_pair_free));

  return g_object_ref (self->connection);
}

void
plugin_codex_build_addin_reset_connection (PluginCodexBuildAddin *self)
{
  g_return_if_fail (PLUGIN_IS_CODEX_BUILD_ADDIN (self));

  plugin_codex_build_addin_clear_connection (self, TRUE);
}

static void
plugin_codex_build_addin_dispose (GObject *object)
{
  PluginCodexBuildAddin *self = (PluginCodexBuildAddin *) object;

  plugin_codex_build_addin_clear_connection (self, TRUE);

  G_OBJECT_CLASS (plugin_codex_build_addin_parent_class)->dispose (object);
}

static void
plugin_codex_build_addin_init (PluginCodexBuildAddin *self)
{
}

static void
plugin_codex_build_addin_class_init (PluginCodexBuildAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = plugin_codex_build_addin_dispose;
}
