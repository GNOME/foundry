/* foundry-build-progress.c
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

#include "foundry-build-progress-private.h"
#include "foundry-build-stage-private.h"

struct _FoundryBuildProgress
{
  FoundryContextual          parent_instance;
  FoundryBuildPipelinePhase  phase;
  GPtrArray                 *stages;
  DexFuture                 *fiber;
  int                        pty_fd;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryBuildProgress, foundry_build_progress, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static void
foundry_build_progress_dispose (GObject *object)
{
  FoundryBuildProgress *self = (FoundryBuildProgress *)object;

  g_clear_fd (&self->pty_fd, NULL);

  if (self->stages->len > 0)
    g_ptr_array_remove_range (self->stages, 0, self->stages->len);

  G_OBJECT_CLASS (foundry_build_progress_parent_class)->dispose (object);
}

static void
foundry_build_progress_finalize (GObject *object)
{
  FoundryBuildProgress *self = (FoundryBuildProgress *)object;

  g_clear_pointer (&self->stages, g_ptr_array_unref);
  dex_clear (&self->fiber);

  G_OBJECT_CLASS (foundry_build_progress_parent_class)->finalize (object);
}

static void
foundry_build_progress_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  FoundryBuildProgress *self = FOUNDRY_BUILD_PROGRESS (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_build_progress_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  FoundryBuildProgress *self = FOUNDRY_BUILD_PROGRESS (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_build_progress_class_init (FoundryBuildProgressClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_build_progress_dispose;
  object_class->finalize = foundry_build_progress_finalize;
  object_class->get_property = foundry_build_progress_get_property;
  object_class->set_property = foundry_build_progress_set_property;
}

static void
foundry_build_progress_init (FoundryBuildProgress *self)
{
  self->pty_fd = -1;
  self->stages = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * foundry_build_progress_await:
 * @self: a #FoundryBuildProgress
 *
 * Gets a [class@Dex.Future] that will resolve when the progress
 * has completed running.
 *
 * Returns: (transfer full): a [class@Dex.Future].
 */
DexFuture *
foundry_build_progress_await (FoundryBuildProgress *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_PROGRESS (self));

  if (self->fiber == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_INITIALIZED,
                                  "Attempt to await build progress without an operation");

  return dex_ref (self->fiber);
}

FoundryBuildProgress *
_foundry_build_progress_new (FoundryBuildPipeline      *pipeline,
                             FoundryBuildPipelinePhase  phase,
                             int                        pty_fd)
{
  FoundryBuildProgress *self;
  GListModel *model;
  guint n_stages;

  g_return_val_if_fail (FOUNDRY_IS_BUILD_PIPELINE (pipeline), NULL);
  g_return_val_if_fail (FOUNDRY_BUILD_PIPELINE_PHASE_MASK (phase) != 0, NULL);

  model = G_LIST_MODEL (pipeline);
  n_stages = g_list_model_get_n_items (model);

  self = g_object_new (FOUNDRY_TYPE_BUILD_PROGRESS, NULL);
  self->phase = phase;
  self->pty_fd = dup (pty_fd);

  for (guint i = 0; i < n_stages; i++)
    {
      g_autoptr(FoundryBuildStage) stage = g_list_model_get_item (model, i);

      if (_foundry_build_stage_matches (stage, phase))
        g_ptr_array_add (self->stages, g_steal_pointer (&stage));
    }

  return self;
}

static DexFuture *
foundry_build_progress_build_fiber (gpointer user_data)
{
  FoundryBuildProgress *self = user_data;

  g_assert (FOUNDRY_IS_BUILD_PROGRESS (self));

  for (guint i = 0; i < self->stages->len; i++)
    {
      FoundryBuildStage *stage = g_ptr_array_index (self->stages, i);
      g_autoptr(GError) error = NULL;

      if (!dex_await (foundry_build_stage_build (stage, self), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

DexFuture *
_foundry_build_progress_build (FoundryBuildProgress *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_PROGRESS (self));
  dex_return_error_if_fail (self->fiber == NULL);

  self->fiber = dex_scheduler_spawn (NULL, 0,
                                     foundry_build_progress_build_fiber,
                                     g_object_ref (self),
                                     g_object_unref);

  return foundry_build_progress_await (self);
}

static DexFuture *
foundry_build_progress_clean_fiber (gpointer user_data)
{
  FoundryBuildProgress *self = user_data;

  g_assert (FOUNDRY_IS_BUILD_PROGRESS (self));

  for (guint i = self->stages->len; i > 0; i--)
    {
      g_autoptr(FoundryBuildStage) stage = g_object_ref (g_ptr_array_index (self->stages, i - 1));
      g_autoptr(GError) error = NULL;

      g_assert (FOUNDRY_IS_BUILD_STAGE (stage));

      if (!dex_await (foundry_build_stage_clean (stage, self), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

DexFuture *
_foundry_build_progress_clean (FoundryBuildProgress *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_PROGRESS (self));
  dex_return_error_if_fail (self->fiber == NULL);

  self->fiber = dex_scheduler_spawn (NULL, 0,
                                     foundry_build_progress_clean_fiber,
                                     g_object_ref (self),
                                     g_object_unref);

  return foundry_build_progress_await (self);
}

static DexFuture *
foundry_build_progress_purge_fiber (gpointer user_data)
{
  FoundryBuildProgress *self = user_data;

  g_assert (FOUNDRY_IS_BUILD_PROGRESS (self));

  for (guint i = self->stages->len; i > 0; i--)
    {
      g_autoptr(FoundryBuildStage) stage = g_object_ref (g_ptr_array_index (self->stages, i - 1));
      g_autoptr(GError) error = NULL;

      if (!dex_await (foundry_build_stage_purge (stage, self), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

DexFuture *
_foundry_build_progress_purge (FoundryBuildProgress *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_PROGRESS (self));
  dex_return_error_if_fail (self->fiber == NULL);

  self->fiber = dex_scheduler_spawn (NULL, 0,
                                     foundry_build_progress_purge_fiber,
                                     g_object_ref (self),
                                     g_object_unref);

  return foundry_build_progress_await (self);
}

/**
 * foundry_build_progress_print: (skip)
 * @self: a [class@Foundry.BuildProgress]
 *
 * Prints a message to the build pipeline PTY device.
 */
void
foundry_build_progress_print (FoundryBuildProgress *self,
                              const char           *format,
                              ...)
{
  g_autofree char *message = NULL;
  va_list args;

  g_return_if_fail (FOUNDRY_IS_BUILD_PROGRESS (self));

  if (self->pty_fd < 0)
    return;

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  write (self->pty_fd, message, strlen (message));
}
