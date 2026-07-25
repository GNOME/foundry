/* foundry-ci-run.c
 *
 * Copyright 2026 Christian Hergert
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

#include "foundry-ci-artifact.h"
#include "foundry-ci-run.h"

/**
 * FoundryCiRun:
 *
 * Abstract representation of an asynchronous continuous integration run.
 *
 * CI providers subclass `FoundryCiRun` and own the run's state,
 * completion, progress, output, and artifacts. The corresponding properties
 * are read-only. Subclasses whose values change must emit `notify` for the
 * affected property and emit [signal@Gio.ListModel::items-changed] from the
 * model returned by [method@Foundry.CiRun.list_artifacts].
 *
 * Since: 1.2
 */

enum {
  PROP_0,
  PROP_EXIT_STATUS,
  PROP_OUTPUT_DIR,
  PROP_PROGRESS,
  PROP_STATE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryCiRun, foundry_ci_run, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_ci_run_real_await (FoundryCiRun *self)
{
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "%s cannot be awaited",
                                G_OBJECT_TYPE_NAME (self));
}

static void
foundry_ci_run_real_cancel (FoundryCiRun *self)
{
}

static FoundryCiRunState
foundry_ci_run_real_get_state (FoundryCiRun *self)
{
  return FOUNDRY_CI_RUN_STATE_PENDING;
}

static double
foundry_ci_run_real_get_progress (FoundryCiRun *self)
{
  return 0;
}

static int
foundry_ci_run_real_get_exit_status (FoundryCiRun *self)
{
  return -1;
}

static char *
foundry_ci_run_real_dup_output_dir (FoundryCiRun *self)
{
  return NULL;
}

static GListModel *
foundry_ci_run_real_list_artifacts (FoundryCiRun *self)
{
  return G_LIST_MODEL (g_list_store_new (FOUNDRY_TYPE_CI_ARTIFACT));
}

static void
foundry_ci_run_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  FoundryCiRun *self = FOUNDRY_CI_RUN (object);

  switch (prop_id)
    {
    case PROP_EXIT_STATUS:
      g_value_set_int (value, foundry_ci_run_get_exit_status (self));
      break;

    case PROP_OUTPUT_DIR:
      g_value_take_string (value, foundry_ci_run_dup_output_dir (self));
      break;

    case PROP_PROGRESS:
      g_value_set_double (value, foundry_ci_run_get_progress (self));
      break;

    case PROP_STATE:
      g_value_set_enum (value, foundry_ci_run_get_state (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
foundry_ci_run_class_init (FoundryCiRunClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_ci_run_get_property;

  klass->await = foundry_ci_run_real_await;
  klass->cancel = foundry_ci_run_real_cancel;
  klass->get_state = foundry_ci_run_real_get_state;
  klass->get_progress = foundry_ci_run_real_get_progress;
  klass->get_exit_status = foundry_ci_run_real_get_exit_status;
  klass->dup_output_dir = foundry_ci_run_real_dup_output_dir;
  klass->list_artifacts = foundry_ci_run_real_list_artifacts;

  /**
   * FoundryCiRun:exit-status:
   *
   * The process-style exit status, or -1 until completion.
   *
   * Since: 1.2
   */
  properties[PROP_EXIT_STATUS] =
    g_param_spec_int ("exit-status", NULL, NULL,
                      -1, G_MAXINT, -1,
                      (G_PARAM_READABLE |
                       G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiRun:output-dir:
   *
   * The directory containing durable run output.
   *
   * Since: 1.2
   */
  properties[PROP_OUTPUT_DIR] =
    g_param_spec_string ("output-dir", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiRun:progress:
   *
   * The current progress from 0.0 to 1.0.
   *
   * Since: 1.2
   */
  properties[PROP_PROGRESS] =
    g_param_spec_double ("progress", NULL, NULL,
                         0, 1, 0,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiRun:state:
   *
   * The current run state.
   *
   * Since: 1.2
   */
  properties[PROP_STATE] =
    g_param_spec_enum ("state", NULL, NULL,
                       FOUNDRY_TYPE_CI_RUN_STATE,
                       FOUNDRY_CI_RUN_STATE_PENDING,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_ci_run_init (FoundryCiRun *self)
{
}

/**
 * foundry_ci_run_await:
 * @self: a [class@Foundry.CiRun]
 *
 * Awaits completion of the run.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to the exit
 *   status or rejects with error
 *
 * Since: 1.2
 */
DexFuture *
foundry_ci_run_await (FoundryCiRun *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_CI_RUN (self));

  return FOUNDRY_CI_RUN_GET_CLASS (self)->await (self);
}

/**
 * foundry_ci_run_cancel:
 * @self: a [class@Foundry.CiRun]
 *
 * Requests cancellation of the run.
 *
 * Since: 1.2
 */
void
foundry_ci_run_cancel (FoundryCiRun *self)
{
  g_return_if_fail (FOUNDRY_IS_CI_RUN (self));

  if (FOUNDRY_CI_RUN_GET_CLASS (self)->cancel)
    FOUNDRY_CI_RUN_GET_CLASS (self)->cancel (self);
}

/**
 * foundry_ci_run_get_state:
 * @self: a [class@Foundry.CiRun]
 *
 * Gets the current run state.
 *
 * Returns: the current state
 *
 * Since: 1.2
 */
FoundryCiRunState
foundry_ci_run_get_state (FoundryCiRun *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_RUN (self), 0);

  return FOUNDRY_CI_RUN_GET_CLASS (self)->get_state (self);
}

/**
 * foundry_ci_run_get_progress:
 * @self: a [class@Foundry.CiRun]
 *
 * Gets the current progress.
 *
 * Returns: progress from 0.0 to 1.0
 *
 * Since: 1.2
 */
double
foundry_ci_run_get_progress (FoundryCiRun *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_RUN (self), 0);

  return FOUNDRY_CI_RUN_GET_CLASS (self)->get_progress (self);
}

/**
 * foundry_ci_run_get_exit_status:
 * @self: a [class@Foundry.CiRun]
 *
 * Gets the exit status, or -1 before successful completion.
 *
 * Returns: the exit status
 *
 * Since: 1.2
 */
int
foundry_ci_run_get_exit_status (FoundryCiRun *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_RUN (self), -1);

  return FOUNDRY_CI_RUN_GET_CLASS (self)->get_exit_status (self);
}

/**
 * foundry_ci_run_dup_output_dir:
 * @self: a [class@Foundry.CiRun]
 *
 * Gets the directory containing the run's durable output.
 *
 * Returns: (transfer full) (nullable): the output directory
 *
 * Since: 1.2
 */
char *
foundry_ci_run_dup_output_dir (FoundryCiRun *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_RUN (self), NULL);

  return FOUNDRY_CI_RUN_GET_CLASS (self)->dup_output_dir (self);
}

/**
 * foundry_ci_run_list_artifacts:
 * @self: a [class@Foundry.CiRun]
 *
 * Lists artifacts discovered by the run.
 *
 * The returned model may emit [signal@Gio.ListModel::items-changed] as the
 * provider discovers artifacts.
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of
 *   [class@Foundry.CiArtifact]
 *
 * Since: 1.2
 */
GListModel *
foundry_ci_run_list_artifacts (FoundryCiRun *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_RUN (self), NULL);

  return FOUNDRY_CI_RUN_GET_CLASS (self)->list_artifacts (self);
}
