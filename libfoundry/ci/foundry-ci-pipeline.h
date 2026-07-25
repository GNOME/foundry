/* foundry-ci-pipeline.h
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

#pragma once

#include <libdex.h>

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_CI_PIPELINE (foundry_ci_pipeline_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_DERIVABLE_TYPE (FoundryCiPipeline, foundry_ci_pipeline, FOUNDRY, CI_PIPELINE, GObject)

struct _FoundryCiPipelineClass
{
  GObjectClass parent_class;

  char              *(*dup_id)       (FoundryCiPipeline *self);
  char              *(*dup_title)    (FoundryCiPipeline *self);
  FoundryCiProvider *(*dup_provider) (FoundryCiPipeline *self);
  GListModel        *(*list_jobs)    (FoundryCiPipeline *self);

  /*< private >*/
  gpointer _reserved[11];
};

FOUNDRY_AVAILABLE_IN_1_2
char              *foundry_ci_pipeline_dup_id       (FoundryCiPipeline *self);
FOUNDRY_AVAILABLE_IN_1_2
char              *foundry_ci_pipeline_dup_title    (FoundryCiPipeline *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryCiProvider *foundry_ci_pipeline_dup_provider (FoundryCiPipeline *self);
FOUNDRY_AVAILABLE_IN_1_2
GListModel        *foundry_ci_pipeline_list_jobs    (FoundryCiPipeline *self);
FOUNDRY_AVAILABLE_IN_1_2
DexFuture         *foundry_ci_pipeline_find_job     (FoundryCiPipeline *self,
                                                     const char        *id) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
