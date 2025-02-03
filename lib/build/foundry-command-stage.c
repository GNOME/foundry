/* foundry-command-stage.c
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

#include "foundry-build-pipeline.h"
#include "foundry-build-progress.h"
#include "foundry-command-stage.h"
#include "foundry-debug.h"
#include "foundry-process-launcher.h"

struct _FoundryCommandStage
{
  FoundryBuildStage          parent_instance;
  FoundryCommand            *build_command;
  FoundryCommand            *clean_command;
  FoundryCommand            *purge_command;
  GFile                     *query_file;
  FoundryBuildPipelinePhase  phase;
};

enum {
  PROP_0,
  PROP_BUILD_COMMAND,
  PROP_CLEAN_COMMAND,
  PROP_PURGE_COMMAND,
  PROP_QUERY_FILE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryCommandStage, foundry_command_stage, FOUNDRY_TYPE_BUILD_STAGE)

static GParamSpec *properties[N_PROPS];

typedef struct _Run
{
  FoundryBuildPipeline *pipeline;
  FoundryBuildProgress *progress;
  FoundryCommand       *command;
} Run;

static void
run_free (Run *state)
{
  g_clear_object (&state->pipeline);
  g_clear_object (&state->progress);
  g_clear_object (&state->command);
  g_free (state);
}

static DexFuture *
foundry_command_stage_run_fiber (gpointer data)
{
  Run *state = data;
  g_autoptr(FoundryProcessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  FoundryBuildPipelinePhase phase;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_BUILD_PIPELINE (state->pipeline));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (state->progress));
  g_assert (FOUNDRY_IS_COMMAND (state->command));

  launcher = foundry_process_launcher_new ();

  phase = foundry_build_progress_get_phase (state->progress);

  if (!dex_await (foundry_command_prepare (state->command, state->pipeline, launcher, phase), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  foundry_build_progress_setup_pty (state->progress, launcher);

  if (!(subprocess = foundry_process_launcher_spawn (launcher, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!dex_await (dex_subprocess_wait_check (subprocess), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
foundry_command_stage_run (FoundryCommandStage  *self,
                           FoundryCommand       *command,
                           FoundryBuildProgress *progress)
{
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  Run *state;

  g_assert (FOUNDRY_IS_COMMAND_STAGE (self));
  g_assert (!command || FOUNDRY_IS_COMMAND (command));

  if (command == NULL)
    return dex_future_new_true ();

  if (!(pipeline = foundry_build_stage_dup_pipeline (FOUNDRY_BUILD_STAGE (self))))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Pipeline was disposed");

  state = g_new0 (Run, 1);
  state->pipeline = g_object_ref (pipeline);
  state->command = g_object_ref (command);
  state->progress = g_object_ref (progress);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_command_stage_run_fiber,
                              state,
                              (GDestroyNotify) run_free);
}

static DexFuture *
foundry_command_stage_build (FoundryBuildStage    *stage,
                             FoundryBuildProgress *progress)
{
  FoundryCommandStage *self = (FoundryCommandStage *)stage;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_COMMAND_STAGE (self));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (progress));

  return foundry_command_stage_run (self, self->build_command, progress);
}

static DexFuture *
foundry_command_stage_clean (FoundryBuildStage    *stage,
                             FoundryBuildProgress *progress)
{
  FoundryCommandStage *self = (FoundryCommandStage *)stage;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_COMMAND_STAGE (self));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (progress));

  return foundry_command_stage_run (self, self->clean_command, progress);
}

static DexFuture *
foundry_command_stage_purge (FoundryBuildStage    *stage,
                             FoundryBuildProgress *progress)
{
  FoundryCommandStage *self = (FoundryCommandStage *)stage;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_COMMAND_STAGE (self));
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (progress));

  return foundry_command_stage_run (self, self->purge_command, progress);
}

static DexFuture *
foundry_command_stage_query_fiber (gpointer data)
{
  FoundryCommandStage *self = data;

  g_assert (FOUNDRY_IS_COMMAND_STAGE (self));

  if (self->query_file != NULL &&
      dex_await_boolean (dex_file_query_exists (self->query_file), NULL))
    foundry_build_stage_set_completed (FOUNDRY_BUILD_STAGE (self), TRUE);

  return dex_future_new_true ();
}

static DexFuture *
foundry_command_stage_query (FoundryBuildStage *stage)
{
  return dex_scheduler_spawn (NULL, 0,
                              foundry_command_stage_query_fiber,
                              g_object_ref (stage),
                              g_object_unref);
}

static FoundryBuildPipelinePhase
foundry_command_stage_get_phase (FoundryBuildStage *stage)
{
  return FOUNDRY_COMMAND_STAGE (stage)->phase;
}

static void
foundry_command_stage_dispose (GObject *object)
{
  FoundryCommandStage *self = (FoundryCommandStage *)object;

  g_clear_object (&self->build_command);
  g_clear_object (&self->clean_command);
  g_clear_object (&self->purge_command);
  g_clear_object (&self->query_file);

  G_OBJECT_CLASS (foundry_command_stage_parent_class)->dispose (object);
}

static void
foundry_command_stage_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundryCommandStage *self = FOUNDRY_COMMAND_STAGE (object);

  switch (prop_id)
    {
    case PROP_BUILD_COMMAND:
      g_value_take_object (value, foundry_command_stage_dup_build_command (self));
      break;

    case PROP_CLEAN_COMMAND:
      g_value_take_object (value, foundry_command_stage_dup_clean_command (self));
      break;

    case PROP_PURGE_COMMAND:
      g_value_take_object (value, foundry_command_stage_dup_purge_command (self));
      break;

    case PROP_QUERY_FILE:
      g_value_take_object (value, foundry_command_stage_dup_query_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_command_stage_class_init (FoundryCommandStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryBuildStageClass *build_stage_class = FOUNDRY_BUILD_STAGE_CLASS (klass);

  object_class->dispose = foundry_command_stage_dispose;
  object_class->get_property = foundry_command_stage_get_property;

  build_stage_class->build = foundry_command_stage_build;
  build_stage_class->clean = foundry_command_stage_clean;
  build_stage_class->query = foundry_command_stage_query;
  build_stage_class->purge = foundry_command_stage_purge;
  build_stage_class->get_phase = foundry_command_stage_get_phase;

  properties[PROP_BUILD_COMMAND] =
    g_param_spec_object ("build-command", NULL, NULL,
                         FOUNDRY_TYPE_COMMAND,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_CLEAN_COMMAND] =
    g_param_spec_object ("clean-command", NULL, NULL,
                         FOUNDRY_TYPE_COMMAND,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PURGE_COMMAND] =
    g_param_spec_object ("purge-command", NULL, NULL,
                         FOUNDRY_TYPE_COMMAND,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_QUERY_FILE] =
    g_param_spec_object ("queyr-file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_command_stage_init (FoundryCommandStage *self)
{
}

/**
 * foundry_command_stage_dup_query_file:
 * @self: a [class@Foundry.CommandStage]
 *
 * Returns: (transfer full) (nullable): the file to check for completion
 *    or %NULL if unset.
 */
GFile *
foundry_command_stage_dup_query_file (FoundryCommandStage *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_STAGE (self), NULL);

  return self->query_file ? g_object_ref (self->query_file) : NULL;
}

/**
 * foundry_command_stage_dup_build_command:
 * @self: a [class@Foundry.CommandStage]
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.Command]
 */
FoundryCommand *
foundry_command_stage_dup_build_command (FoundryCommandStage *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_STAGE (self), NULL);

  return self->build_command ? g_object_ref (self->build_command) : NULL;
}

/**
 * foundry_command_stage_dup_clean_command:
 * @self: a [class@Foundry.CommandStage]
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.Command]
 */
FoundryCommand *
foundry_command_stage_dup_clean_command (FoundryCommandStage *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_STAGE (self), NULL);

  return self->clean_command ? g_object_ref (self->clean_command) : NULL;
}

/**
 * foundry_command_stage_dup_purge_command:
 * @self: a [class@Foundry.CommandStage]
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.Command]
 */
FoundryCommand *
foundry_command_stage_dup_purge_command (FoundryCommandStage *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_STAGE (self), NULL);

  return self->purge_command ? g_object_ref (self->purge_command) : NULL;
}

/**
 * foundry_command_stage_new:
 * @build_command: (nullable): a command or %NULL
 * @clean_command: (nullable): a command or %NULL
 * @purge_command: (nullable): a command or %NULL
 * @query_file: (nullable): a file to query for completion
 *
 * Returns: (transfer full): a [class@Foundry.CommandStage]
 */
FoundryBuildStage *
foundry_command_stage_new (FoundryContext            *context,
                           FoundryBuildPipelinePhase  phase,
                           FoundryCommand            *build_command,
                           FoundryCommand            *clean_command,
                           FoundryCommand            *purge_command,
                           GFile                     *query_file)
{
  FoundryCommandStage *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (!build_command || FOUNDRY_IS_COMMAND (build_command), NULL);
  g_return_val_if_fail (!clean_command || FOUNDRY_IS_COMMAND (clean_command), NULL);
  g_return_val_if_fail (!query_file || G_IS_FILE (query_file), NULL);

  self = g_object_new (FOUNDRY_TYPE_COMMAND_STAGE,
                       "context", context,
                       NULL);

  g_set_object (&self->build_command, build_command);
  g_set_object (&self->clean_command, clean_command);
  g_set_object (&self->purge_command, purge_command);
  g_set_object (&self->query_file, query_file);

  self->phase = phase;

  return FOUNDRY_BUILD_STAGE (self);
}
