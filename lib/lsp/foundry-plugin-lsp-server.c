/* foundry-plugin-lsp-server.c
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

#include <glib/gstdio.h>

#include "foundry-build-pipeline.h"
#include "foundry-jsonrpc-private.h"
#include "foundry-lsp-client.h"
#include "foundry-plugin-lsp-server-private.h"
#include "foundry-process-launcher.h"
#include "foundry-util.h"

struct _FoundryPluginLspServer
{
  FoundryLspServer  parent_instance;
  PeasPluginInfo   *plugin_info;
};

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryPluginLspServer, foundry_plugin_lsp_server, FOUNDRY_TYPE_LSP_SERVER)

static GParamSpec *properties[N_PROPS];

static char **
foundry_plugin_lsp_server_dup_command (FoundryPluginLspServer *self)
{
  g_auto(GStrv) command = NULL;
  const char *x_command;

  g_assert (FOUNDRY_IS_PLUGIN_LSP_SERVER (self));

  if (self->plugin_info == NULL)
    return NULL;

  if (!(x_command = peas_plugin_info_get_external_data (self->plugin_info, "LSP-Command")))
    return NULL;

  if (!g_shell_parse_argv (x_command, NULL, &command, NULL))
    return NULL;

  return g_steal_pointer (&command);
}

typedef struct _Spawn
{
  FoundryPluginLspServer *self;
  FoundryBuildPipeline *pipeline;
  int stdin_fd;
  int stdout_fd;
  guint log_stderr : 1;
} Spawn;

static void
spawn_free (Spawn *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->pipeline);
  g_clear_fd (&state->stdin_fd, NULL);
  g_clear_fd (&state->stdout_fd, NULL);
  g_free (state);
}

static DexFuture *
foundry_plugin_lsp_server_spawn_fiber (gpointer data)
{
  Spawn *state = data;
  FoundryPluginLspServer *self = state->self;
  FoundryBuildPipeline *pipeline = state->pipeline;
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(JsonrpcClient) client = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GIOStream) io_stream = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) command = NULL;
  GSubprocessFlags flags = 0;

  g_assert (FOUNDRY_IS_PLUGIN_LSP_SERVER (self));
  g_assert (!pipeline || FOUNDRY_IS_BUILD_PIPELINE (pipeline));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  dex_return_error_if_fail (FOUNDRY_IS_CONTEXT (context));

  launcher = foundry_process_launcher_new ();

  if (!(command = foundry_plugin_lsp_server_dup_command (self)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Plugin %s is missing X-LSP-Command",
                                  peas_plugin_info_get_module_name (self->plugin_info));

  if (pipeline != NULL)
    {
      if (!dex_await (foundry_build_pipeline_prepare (pipeline, launcher, FOUNDRY_BUILD_PIPELINE_PHASE_BUILD), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (state->stdin_fd == -1 && state->stdout_fd == -1)
    flags = G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE;

  if (!state->log_stderr)
    flags |= G_SUBPROCESS_FLAGS_STDERR_SILENCE;

  foundry_process_launcher_set_argv (launcher, (const char * const *)command);
  foundry_process_launcher_take_fd (launcher, g_steal_fd (&state->stdin_fd), STDIN_FILENO);
  foundry_process_launcher_take_fd (launcher, g_steal_fd (&state->stdout_fd), STDOUT_FILENO);

  if (!(subprocess = foundry_process_launcher_spawn_with_flags (launcher, flags, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  io_stream = g_simple_io_stream_new (g_subprocess_get_stdout_pipe (subprocess),
                                      g_subprocess_get_stdin_pipe (subprocess));

  return foundry_lsp_client_new (context, io_stream, subprocess);
}

static DexFuture *
foundry_plugin_lsp_server_spawn (FoundryLspServer     *lsp_server,
                                 FoundryBuildPipeline *pipeline,
                                 int                   stdin_fd,
                                 int                   stdout_fd,
                                 gboolean              log_stderr)
{
  FoundryPluginLspServer *self = (FoundryPluginLspServer *)lsp_server;
  Spawn *state;

  g_assert (FOUNDRY_IS_PLUGIN_LSP_SERVER (self));
  g_assert (FOUNDRY_IS_BUILD_PIPELINE (pipeline));

  state = g_new0 (Spawn, 1);
  state->self = g_object_ref (self);
  g_set_object (&state->pipeline, pipeline);
  state->stdin_fd = dup (stdin_fd);
  state->stdout_fd = dup (stdout_fd);
  state->log_stderr = !!log_stderr;

  return dex_scheduler_spawn (NULL, 0,
                              foundry_plugin_lsp_server_spawn_fiber,
                              state,
                              (GDestroyNotify) spawn_free);
}

static char *
foundry_plugin_lsp_server_dup_name (FoundryLspServer *lsp_server)
{
  FoundryPluginLspServer *self = FOUNDRY_PLUGIN_LSP_SERVER (lsp_server);

  return g_strdup (peas_plugin_info_get_name (self->plugin_info));
}

static char **
foundry_plugin_lsp_server_dup_languages (FoundryLspServer *lsp_server)
{
  FoundryPluginLspServer *self = FOUNDRY_PLUGIN_LSP_SERVER (lsp_server);
  const char *x_languages = peas_plugin_info_get_external_data (self->plugin_info, "LSP-Languages");

  if (x_languages == NULL)
    return g_new0 (char *, 1);

  return g_strsplit (x_languages, ";", 0);
}

static void
foundry_plugin_lsp_server_finalize (GObject *object)
{
  FoundryPluginLspServer *self = (FoundryPluginLspServer *)object;

  g_clear_object (&self->plugin_info);

  G_OBJECT_CLASS (foundry_plugin_lsp_server_parent_class)->finalize (object);
}

static void
foundry_plugin_lsp_server_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  FoundryPluginLspServer *self = FOUNDRY_PLUGIN_LSP_SERVER (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_set_object (value, self->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_plugin_lsp_server_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  FoundryPluginLspServer *self = FOUNDRY_PLUGIN_LSP_SERVER (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      self->plugin_info = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_plugin_lsp_server_class_init (FoundryPluginLspServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLspServerClass *lsp_server_class = FOUNDRY_LSP_SERVER_CLASS (klass);

  object_class->finalize = foundry_plugin_lsp_server_finalize;
  object_class->get_property = foundry_plugin_lsp_server_get_property;
  object_class->set_property = foundry_plugin_lsp_server_set_property;

  lsp_server_class->dup_name = foundry_plugin_lsp_server_dup_name;
  lsp_server_class->dup_languages = foundry_plugin_lsp_server_dup_languages;
  lsp_server_class->spawn = foundry_plugin_lsp_server_spawn;

  properties[PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_plugin_lsp_server_init (FoundryPluginLspServer *self)
{
}

FoundryLspServer *
foundry_plugin_lsp_server_new (FoundryContext *context,
                               PeasPluginInfo *plugin_info)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (PEAS_IS_PLUGIN_INFO (plugin_info), NULL);

  return g_object_new (FOUNDRY_TYPE_PLUGIN_LSP_SERVER,
                       "context", context,
                       "plugin-info", plugin_info,
                       NULL);
}
