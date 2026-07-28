/* plugin-gitlab-ci-run.c
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

#include "foundry-util-private.h"

#include "plugin-gitlab-ci-run-private.h"

struct _PluginGitlabCiRun
{
  FoundryCiRun       parent_instance;
  DexCancellable    *cancellable;
  DexPromise        *completion;
  GListStore        *artifacts;
  char              *output_dir;
  GMutex             mutex;
  FoundryCiRunState  state;
  double             progress;
  int                exit_status;
};

G_DEFINE_FINAL_TYPE (PluginGitlabCiRun, plugin_gitlab_ci_run, FOUNDRY_TYPE_CI_RUN)

static DexFuture *
plugin_gitlab_ci_run_await (FoundryCiRun *run)
{
  return dex_ref (PLUGIN_GITLAB_CI_RUN (run)->completion);
}

static void
plugin_gitlab_ci_run_cancel (FoundryCiRun *run)
{
  PluginGitlabCiRun *self = PLUGIN_GITLAB_CI_RUN (run);

  if (dex_future_is_pending (DEX_FUTURE (self->cancellable)))
    dex_cancellable_cancel (self->cancellable);
}

static FoundryCiRunState
plugin_gitlab_ci_run_get_state (FoundryCiRun *run)
{
  return PLUGIN_GITLAB_CI_RUN (run)->state;
}

static double
plugin_gitlab_ci_run_get_progress (FoundryCiRun *run)
{
  PluginGitlabCiRun *self = PLUGIN_GITLAB_CI_RUN (run);
  double progress;

  g_mutex_lock (&self->mutex);
  progress = self->progress;
  g_mutex_unlock (&self->mutex);

  return progress;
}

static int
plugin_gitlab_ci_run_get_exit_status (FoundryCiRun *run)
{
  return PLUGIN_GITLAB_CI_RUN (run)->exit_status;
}

static char *
plugin_gitlab_ci_run_dup_output_dir (FoundryCiRun *run)
{
  return g_strdup (PLUGIN_GITLAB_CI_RUN (run)->output_dir);
}

static GListModel *
plugin_gitlab_ci_run_list_artifacts (FoundryCiRun *run)
{
  return G_LIST_MODEL (g_object_ref (PLUGIN_GITLAB_CI_RUN (run)->artifacts));
}

static void
plugin_gitlab_ci_run_dispose (GObject *object)
{
  PluginGitlabCiRun *self = PLUGIN_GITLAB_CI_RUN (object);

  plugin_gitlab_ci_run_cancel (FOUNDRY_CI_RUN (self));

  G_OBJECT_CLASS (plugin_gitlab_ci_run_parent_class)->dispose (object);
}

static void
plugin_gitlab_ci_run_finalize (GObject *object)
{
  PluginGitlabCiRun *self = PLUGIN_GITLAB_CI_RUN (object);

  dex_clear (&self->cancellable);
  dex_clear (&self->completion);
  g_clear_object (&self->artifacts);
  g_clear_pointer (&self->output_dir, g_free);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (plugin_gitlab_ci_run_parent_class)->finalize (object);
}

static void
plugin_gitlab_ci_run_class_init (PluginGitlabCiRunClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryCiRunClass *run_class = FOUNDRY_CI_RUN_CLASS (klass);

  object_class->dispose = plugin_gitlab_ci_run_dispose;
  object_class->finalize = plugin_gitlab_ci_run_finalize;

  run_class->await = plugin_gitlab_ci_run_await;
  run_class->cancel = plugin_gitlab_ci_run_cancel;
  run_class->get_state = plugin_gitlab_ci_run_get_state;
  run_class->get_progress = plugin_gitlab_ci_run_get_progress;
  run_class->get_exit_status = plugin_gitlab_ci_run_get_exit_status;
  run_class->dup_output_dir = plugin_gitlab_ci_run_dup_output_dir;
  run_class->list_artifacts = plugin_gitlab_ci_run_list_artifacts;
}

static void
plugin_gitlab_ci_run_init (PluginGitlabCiRun *self)
{
  g_mutex_init (&self->mutex);
  self->artifacts = g_list_store_new (FOUNDRY_TYPE_CI_ARTIFACT);
  self->cancellable = dex_cancellable_new ();
  self->completion = dex_promise_new ();
  self->exit_status = -1;
}

PluginGitlabCiRun *
plugin_gitlab_ci_run_new (FoundryContext *context)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);

  return g_object_new (PLUGIN_TYPE_GITLAB_CI_RUN,
                       "context", context,
                       NULL);
}

DexCancellable *
plugin_gitlab_ci_run_dup_cancellable (PluginGitlabCiRun *self)
{
  g_return_val_if_fail (PLUGIN_IS_GITLAB_CI_RUN (self), NULL);

  return dex_ref (self->cancellable);
}

void
plugin_gitlab_ci_run_set_state (PluginGitlabCiRun *self,
                                FoundryCiRunState  state)
{
  g_return_if_fail (PLUGIN_IS_GITLAB_CI_RUN (self));

  if (self->state != state)
    {
      self->state = state;
      g_object_notify (G_OBJECT (self), "state");
    }
}

void
plugin_gitlab_ci_run_set_progress (PluginGitlabCiRun *self,
                                   double             progress)
{
  GParamSpec *pspec;
  gboolean changed = FALSE;

  g_return_if_fail (PLUGIN_IS_GITLAB_CI_RUN (self));

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (self), "progress");
  progress = CLAMP (progress, 0, 1);
  g_mutex_lock (&self->mutex);
  if (self->progress != progress)
    {
      self->progress = progress;
      changed = TRUE;
    }
  g_mutex_unlock (&self->mutex);

  if (changed)
    foundry_notify_pspec_in_main (G_OBJECT (self), pspec);
}

void
plugin_gitlab_ci_run_set_output_dir (PluginGitlabCiRun *self,
                                     const char        *output_dir)
{
  g_return_if_fail (PLUGIN_IS_GITLAB_CI_RUN (self));

  if (g_set_str (&self->output_dir, output_dir))
    g_object_notify (G_OBJECT (self), "output-dir");
}

void
plugin_gitlab_ci_run_add_artifact (PluginGitlabCiRun *self,
                                   FoundryCiArtifact *artifact)
{
  g_return_if_fail (PLUGIN_IS_GITLAB_CI_RUN (self));
  g_return_if_fail (FOUNDRY_IS_CI_ARTIFACT (artifact));

  g_list_store_append (self->artifacts, artifact);
}

void
plugin_gitlab_ci_run_complete (PluginGitlabCiRun *self,
                               int                exit_status)
{
  g_return_if_fail (PLUGIN_IS_GITLAB_CI_RUN (self));

  if (!dex_future_is_pending (DEX_FUTURE (self->completion)))
    return;

  self->exit_status = exit_status;
  plugin_gitlab_ci_run_set_progress (self, 1);
  plugin_gitlab_ci_run_set_state (self,
                                  exit_status == 0
                                    ? FOUNDRY_CI_RUN_STATE_PASSED
                                    : FOUNDRY_CI_RUN_STATE_FAILED);
  g_object_notify (G_OBJECT (self), "exit-status");
  dex_promise_resolve_int (self->completion, exit_status);
}

void
plugin_gitlab_ci_run_fail (PluginGitlabCiRun *self,
                           GError            *error)
{
  g_return_if_fail (PLUGIN_IS_GITLAB_CI_RUN (self));
  g_return_if_fail (error != NULL);

  if (!dex_future_is_pending (DEX_FUTURE (self->completion)))
    {
      g_error_free (error);
      return;
    }

  plugin_gitlab_ci_run_set_state (self,
                                  g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)
                                    ? FOUNDRY_CI_RUN_STATE_CANCELLED
                                    : FOUNDRY_CI_RUN_STATE_FAILED);
  dex_promise_reject (self->completion, error);
}
