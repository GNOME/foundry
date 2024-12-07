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

#include "foundry-build-progress.h"

struct _FoundryBuildProgress
{
  FoundryContextual parent_instance;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryBuildProgress, foundry_build_progress, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static void
foundry_build_progress_finalize (GObject *object)
{
  FoundryBuildProgress *self = (FoundryBuildProgress *)object;

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

  object_class->finalize = foundry_build_progress_finalize;
  object_class->get_property = foundry_build_progress_get_property;
  object_class->set_property = foundry_build_progress_set_property;
}

static void
foundry_build_progress_init (FoundryBuildProgress *self)
{
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

  /* TODO: */

  return dex_future_new_true ();
}
