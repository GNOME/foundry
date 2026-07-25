/* foundry-ci-run.h
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

#include "foundry-ci-enums.h"
#include "foundry-contextual.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_CI_RUN (foundry_ci_run_get_type())

FOUNDRY_AVAILABLE_IN_1_2
G_DECLARE_DERIVABLE_TYPE (FoundryCiRun, foundry_ci_run, FOUNDRY, CI_RUN, FoundryContextual)

struct _FoundryCiRunClass
{
  FoundryContextualClass parent_class;

  DexFuture         *(*await)           (FoundryCiRun *self);
  void               (*cancel)          (FoundryCiRun *self);
  FoundryCiRunState  (*get_state)       (FoundryCiRun *self);
  double             (*get_progress)    (FoundryCiRun *self);
  int                (*get_exit_status) (FoundryCiRun *self);
  char              *(*dup_output_dir)  (FoundryCiRun *self);
  GListModel        *(*list_artifacts)  (FoundryCiRun *self);

  /*< private >*/
  gpointer _reserved[9];
};

FOUNDRY_AVAILABLE_IN_1_2
DexFuture         *foundry_ci_run_await           (FoundryCiRun *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
void               foundry_ci_run_cancel          (FoundryCiRun *self);
FOUNDRY_AVAILABLE_IN_1_2
FoundryCiRunState  foundry_ci_run_get_state       (FoundryCiRun *self);
FOUNDRY_AVAILABLE_IN_1_2
double             foundry_ci_run_get_progress    (FoundryCiRun *self);
FOUNDRY_AVAILABLE_IN_1_2
int                foundry_ci_run_get_exit_status (FoundryCiRun *self);
FOUNDRY_AVAILABLE_IN_1_2
char              *foundry_ci_run_dup_output_dir  (FoundryCiRun *self);
FOUNDRY_AVAILABLE_IN_1_2
GListModel        *foundry_ci_run_list_artifacts  (FoundryCiRun *self);

G_END_DECLS
