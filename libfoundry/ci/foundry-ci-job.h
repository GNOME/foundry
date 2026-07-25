/* foundry-ci-job.h
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

#include "foundry-ci-enums.h"
#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_CI_JOB (foundry_ci_job_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_DERIVABLE_TYPE (FoundryCiJob, foundry_ci_job, FOUNDRY, CI_JOB, GObject)

struct _FoundryCiJobClass
{
  GObjectClass parent_class;

  char                    *(*dup_id)          (FoundryCiJob *self);
  char                    *(*dup_title)       (FoundryCiJob *self);
  char                    *(*dup_stage)       (FoundryCiJob *self);
  char                    *(*dup_image)       (FoundryCiJob *self);
  char                    *(*dup_reason)      (FoundryCiJob *self);
  FoundryCiJobDisposition  (*get_disposition) (FoundryCiJob *self);
  gboolean                 (*get_can_run)     (FoundryCiJob *self);
  gboolean                 (*get_can_shell)   (FoundryCiJob *self);
  FoundryCiPipeline       *(*dup_pipeline)    (FoundryCiJob *self);
  FoundryCiProvider       *(*dup_provider)    (FoundryCiJob *self);

  /*< private >*/
  gpointer _reserved[5];
};

FOUNDRY_AVAILABLE_IN_1_2
char                    *foundry_ci_job_dup_id          (FoundryCiJob *self);
FOUNDRY_AVAILABLE_IN_1_2
char                    *foundry_ci_job_dup_title       (FoundryCiJob *self);
FOUNDRY_AVAILABLE_IN_1_2
char                    *foundry_ci_job_dup_stage       (FoundryCiJob *self);
FOUNDRY_AVAILABLE_IN_1_2
char                    *foundry_ci_job_dup_image       (FoundryCiJob *self);
FOUNDRY_AVAILABLE_IN_1_2
char                    *foundry_ci_job_dup_reason      (FoundryCiJob *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryCiJobDisposition  foundry_ci_job_get_disposition (FoundryCiJob *self);
FOUNDRY_AVAILABLE_IN_1_2
gboolean                 foundry_ci_job_get_can_run     (FoundryCiJob *self);
FOUNDRY_AVAILABLE_IN_1_2
gboolean                 foundry_ci_job_get_can_shell   (FoundryCiJob *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryCiPipeline       *foundry_ci_job_dup_pipeline    (FoundryCiJob *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryCiProvider       *foundry_ci_job_dup_provider    (FoundryCiJob *self);

G_END_DECLS
