/* foundry-diagnostic-tool.c
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

#include "foundry-build-manager.h"
#include "foundry-build-pipeline.h"
#include "foundry-command.h"
#include "foundry-diagnostic.h"
#include "foundry-diagnostic-tool.h"
#include "foundry-process-launcher.h"
#include "foundry-subprocess.h"

typedef struct
{
  FoundryCommand *command;
} FoundryDiagnosticToolPrivate;

enum {
  PROP_0,
  PROP_COMMAND,
  N_PROPS
};

G_DEFINE_QUARK (foundry-diagnostic-tool-error, foundry_diagnostic_tool_error)
G_DEFINE_TYPE_WITH_PRIVATE (FoundryDiagnosticTool, foundry_diagnostic_tool, FOUNDRY_TYPE_DIAGNOSTIC_PROVIDER)

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_diagnostic_tool_dup_bytes_for_stdin (FoundryDiagnosticTool *self,
                                             GFile                 *file,
                                             GBytes                *contents,
                                             const char            *language)
{
  if (FOUNDRY_DIAGNOSTIC_TOOL_GET_CLASS (self)->dup_bytes_for_stdin)
    return FOUNDRY_DIAGNOSTIC_TOOL_GET_CLASS (self)->dup_bytes_for_stdin (self, file, contents, language);

  return dex_future_new_take_boxed (G_TYPE_BYTES, g_bytes_new_static ("", 0));
}

static DexFuture *
foundry_diagnostic_tool_extract_from_stdout (FoundryDiagnosticTool *self,
                                             GFile                 *file,
                                             GBytes                *contents,
                                             const char            *language,
                                             GBytes                *stdout_bytes)
{
  if (FOUNDRY_DIAGNOSTIC_TOOL_GET_CLASS (self)->extract_from_stdout)
    return FOUNDRY_DIAGNOSTIC_TOOL_GET_CLASS (self)->extract_from_stdout (self, file, contents, language, stdout_bytes);

  return dex_future_new_take_object (g_list_store_new (FOUNDRY_TYPE_DIAGNOSTIC));
}

typedef struct _Diagnose
{
  FoundryDiagnosticTool *self;
  FoundryContext        *context;
  FoundryCommand        *command;
  GFile                 *file;
  GBytes                *contents;
  char                  *language;
} Diagnose;

static void
diagnose_free (Diagnose *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->context);
  g_clear_object (&state->command);
  g_clear_object (&state->file);
  g_clear_pointer (&state->contents, g_bytes_unref);
  g_clear_pointer (&state->language, g_free);
  g_free (state);
}

static DexFuture *
foundry_diagnostic_tool_diagnose_fiber (gpointer data)
{
  Diagnose *state = data;
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(FoundryBuildManager) build_manager = NULL;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GListModel) diagnostics = NULL;
  g_autoptr(GBytes) stdin_bytes = NULL;
  g_autoptr(GBytes) stdout_bytes = NULL;
  GSubprocessFlags flags = 0;
  g_autoptr(GError) error = NULL;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_DIAGNOSTIC_TOOL (state->self));
  g_assert (FOUNDRY_IS_COMMAND (state->command));
  g_assert (!state->file || G_IS_FILE (state->file));
  g_assert (state->file || state->contents);

  launcher = foundry_process_launcher_new ();

  build_manager = foundry_context_dup_build_manager (state->context);
  pipeline = dex_await_object (foundry_build_manager_load_pipeline (build_manager), NULL);

  if (!dex_await (foundry_command_prepare (state->command, pipeline, launcher, FOUNDRY_BUILD_PIPELINE_PHASE_BUILD), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  flags = G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE;

  if (!(stdin_bytes = dex_await_boxed (foundry_diagnostic_tool_dup_bytes_for_stdin (state->self, state->file, state->contents, state->language), &error)))
    {
      flags &= ~G_SUBPROCESS_FLAGS_STDIN_PIPE;

      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (!(subprocess = foundry_process_launcher_spawn_with_flags (launcher, flags, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(stdout_bytes = dex_await_boxed (foundry_subprocess_communicate (subprocess, stdin_bytes), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return foundry_diagnostic_tool_extract_from_stdout (state->self,
                                                      state->file,
                                                      state->contents,
                                                      state->language,
                                                      stdout_bytes);
}

static DexFuture *
foundry_diagnostic_tool_diagnose (FoundryDiagnosticProvider *provider,
                                  GFile                     *file,
                                  GBytes                    *contents,
                                  const char                *language)
{
  FoundryDiagnosticTool *self = (FoundryDiagnosticTool *)provider;
  FoundryDiagnosticToolPrivate *priv = foundry_diagnostic_tool_get_instance_private (self);
  Diagnose *state;

  dex_return_error_if_fail (FOUNDRY_IS_DIAGNOSTIC_TOOL (self));
  dex_return_error_if_fail (!file || G_IS_FILE (file));
  dex_return_error_if_fail (file || contents);

  if (priv->command == NULL)
    return dex_future_new_reject (FOUNDRY_DIAGNOSTIC_TOOL_ERROR,
                                  FOUNDRY_DIAGNOSTIC_TOOL_ERROR_NO_COMMAND,
                                  "No command was provided");

  state = g_new0 (Diagnose, 1);
  state->self = g_object_ref (FOUNDRY_DIAGNOSTIC_TOOL (provider));
  state->context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (provider));
  state->command = g_object_ref (priv->command);
  state->file = file ? g_object_ref (file) : NULL;
  state->contents = contents ? g_bytes_ref (contents) : NULL;
  state->language = g_strdup (language);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_diagnostic_tool_diagnose_fiber,
                              state,
                              (GDestroyNotify) diagnose_free);
}

static void
foundry_diagnostic_tool_finalize (GObject *object)
{
  FoundryDiagnosticTool *self = (FoundryDiagnosticTool *)object;
  FoundryDiagnosticToolPrivate *priv = foundry_diagnostic_tool_get_instance_private (self);

  g_clear_object (&priv->command);

  G_OBJECT_CLASS (foundry_diagnostic_tool_parent_class)->finalize (object);
}

static void
foundry_diagnostic_tool_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  FoundryDiagnosticTool *self = FOUNDRY_DIAGNOSTIC_TOOL (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      g_value_take_object (value, foundry_diagnostic_tool_dup_command (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_diagnostic_tool_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  FoundryDiagnosticTool *self = FOUNDRY_DIAGNOSTIC_TOOL (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      foundry_diagnostic_tool_set_command (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_diagnostic_tool_class_init (FoundryDiagnosticToolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryDiagnosticProviderClass *provider_class = FOUNDRY_DIAGNOSTIC_PROVIDER_CLASS (klass);

  object_class->finalize = foundry_diagnostic_tool_finalize;
  object_class->get_property = foundry_diagnostic_tool_get_property;
  object_class->set_property = foundry_diagnostic_tool_set_property;

  provider_class->diagnose = foundry_diagnostic_tool_diagnose;

  properties[PROP_COMMAND] =
    g_param_spec_object ("command", NULL, NULL,
                         FOUNDRY_TYPE_COMMAND,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_diagnostic_tool_init (FoundryDiagnosticTool *self)
{
}

/**
 * foundry_diagnostic_tool_dup_command:
 * @self: a #FoundryDiagnosticTool
 *
 * Returns: (transfer full) (nullable): a #FoundryCommand or %NULL
 */
FoundryCommand *
foundry_diagnostic_tool_dup_command (FoundryDiagnosticTool *self)
{
  FoundryDiagnosticToolPrivate *priv = foundry_diagnostic_tool_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_DIAGNOSTIC_TOOL (self), NULL);

  return priv->command ? g_object_ref (priv->command) : NULL;
}

void
foundry_diagnostic_tool_set_command (FoundryDiagnosticTool *self,
                                     FoundryCommand        *command)
{
  FoundryDiagnosticToolPrivate *priv = foundry_diagnostic_tool_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_DIAGNOSTIC_TOOL (self));
  g_return_if_fail (!command || FOUNDRY_IS_COMMAND (command));

  if (g_set_object (&priv->command, command))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMMAND]);
}
